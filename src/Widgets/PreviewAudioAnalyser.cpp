#include "PreviewAudioAnalyser.h"

#include <QtCore/QUrl>
#include <QtMultimedia/QAudioBuffer>
#include <QtMultimedia/QAudioDecoder>
#include <QtMultimedia/QAudioFormat>

PreviewAudioAnalyser::PreviewAudioAnalyser(QObject* parent)
    : QObject(parent)
{
    this->decoder = new QAudioDecoder(this);

    QObject::connect(this->decoder, SIGNAL(bufferReady()), this, SLOT(bufferReady()));
    QObject::connect(this->decoder, SIGNAL(finished()), this, SLOT(decodingFinished()));
    QObject::connect(this->decoder, SIGNAL(error(QAudioDecoder::Error)), this, SLOT(decodingErrored()));
}

PreviewAudioAnalyser::~PreviewAudioAnalyser()
{
    stop();
}

void PreviewAudioAnalyser::analyse(const QString& filePath)
{
    stop();

    this->track.clear();
    this->announced = false;
    this->sawAnyAudio = false;

    if (filePath.isEmpty())
        return;

    this->decoder->setSource(QUrl::fromLocalFile(filePath));
    this->decoder->start();
}

void PreviewAudioAnalyser::stop()
{
    if (this->decoder != nullptr)
        this->decoder->stop();
}

bool PreviewAudioAnalyser::hasLevels() const
{
    return !this->track.isEmpty();
}

int PreviewAudioAnalyser::channels() const
{
    return this->track.channels();
}

QVector<double> PreviewAudioAnalyser::dbfsAt(qint64 positionMs) const
{
    return this->track.dbfsAt(positionMs);
}

void PreviewAudioAnalyser::bufferReady()
{
    QAudioBuffer buffer = this->decoder->read();
    if (!buffer.isValid())
        return;

    const QAudioFormat format = buffer.format();
    if (!format.isValid() || format.channelCount() <= 0 || format.sampleRate() <= 0)
        return;

    const int channelCount = format.channelCount();
    const int bytesPerSample = format.bytesPerSample();
    const qsizetype frames = buffer.frameCount();

    if (bytesPerSample <= 0 || frames <= 0)
        return;

    // startTime() is microseconds and is -1 when the decoder does not know. A
    // buffer with no position cannot be placed on the timeline, and guessing
    // would put the meter out of step with the picture, so it is dropped.
    const qint64 startUs = buffer.startTime();
    if (startUs < 0)
        return;

    this->sawAnyAudio = true;

    // The void* overload is private; the templated one is how Qt means this to be
    // read. char is the right type here because the sample format is not known
    // until run time and normalizedSampleValue() takes the raw address anyway.
    const char* bytes = buffer.constData<char>();
    const qint64 startMs = startUs / 1000;
    const double msPerFrame = 1000.0 / static_cast<double>(format.sampleRate());

    // normalizedSampleValue() handles every sample format Qt can hand back —
    // unsigned 8-bit, signed 16 and 32, and float — so the peaks below are
    // comparable whatever the file was encoded as.
    for (qsizetype frame = 0; frame < frames; frame++)
    {
        const qint64 timeMs = startMs + static_cast<qint64>(frame * msPerFrame);

        for (int channel = 0; channel < channelCount; channel++)
        {
            const void* sample = bytes + (frame * channelCount + channel) * bytesPerSample;
            const double value = std::abs(static_cast<double>(format.normalizedSampleValue(sample)));

            this->track.addPeak(timeMs, channel, value);
        }
    }

    // Announced once, as soon as there is anything to draw, so a long file starts
    // metering immediately instead of after the whole decode.
    if (!this->announced && !this->track.isEmpty())
    {
        this->announced = true;
        emit levelsAvailable();
    }
}

void PreviewAudioAnalyser::decodingFinished()
{
    if (!this->sawAnyAudio)
        emit noAudio();
}

void PreviewAudioAnalyser::decodingErrored()
{
    // A file the decoder cannot read is not worth reporting to the operator: the
    // picture still plays, and the panel simply shows no meters. Plenty of
    // perfectly good media has no audio track at all.
    if (!this->sawAnyAudio)
        emit noAudio();
}
