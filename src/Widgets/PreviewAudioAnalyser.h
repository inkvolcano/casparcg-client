#pragma once

#include "Shared.h"

#include "AudioLevelTrack.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVector>

QT_BEGIN_NAMESPACE
class QAudioDecoder;
QT_END_NAMESPACE

// Decodes a media file's audio in the background and builds a level timeline the
// preview meters read by playback position.
//
// This exists because Qt 6.5 has no way to meter what a QMediaPlayer is playing.
// Decoding separately is not a workaround with a cost — it is better for this
// job, because the levels end up addressed by position, so scrubbing and pausing
// both show the right thing and nothing can drift.
//
// Everything is local. No server is involved, and none needs to be running.
class WIDGETS_EXPORT PreviewAudioAnalyser : public QObject
{
    Q_OBJECT

    public:
        explicit PreviewAudioAnalyser(QObject* parent = nullptr);
        ~PreviewAudioAnalyser() override;

        // Starts analysing a file. Any analysis already running is abandoned, so
        // clicking through a library does not leave a queue of dead decodes.
        void analyse(const QString& filePath);
        void stop();

        bool hasLevels() const;
        int channels() const;

        // dBFS per channel at this position, or empty when that part of the file
        // has not been decoded yet or the file has no audio.
        QVector<double> dbfsAt(qint64 positionMs) const;

    Q_SIGNALS:
        // Fired once, when there is enough decoded to be worth drawing.
        void levelsAvailable();

        // Fired when the file turns out to have no audio at all, so the panel can
        // take the meters away rather than show eight silent bars.
        void noAudio();

    private:
        QAudioDecoder* decoder = nullptr;
        AudioLevelTrack track;
        bool announced = false;
        bool sawAnyAudio = false;

        Q_SLOT void bufferReady();
        Q_SLOT void decodingFinished();
        Q_SLOT void decodingErrored();
};
