#include "NdiReceiver.h"

#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>
#include <QtGui/QImage>
#include <QtMultimedia/QMediaDevices>

#include <cstring>
#include <vector>

NdiReceiver::NdiReceiver(const NDIlib_v5* ndi, QObject* parent)
    : QObject(parent), p_NDI(ndi)
{
}

NdiReceiver::~NdiReceiver()
{
    delete audioSink;
}

void NdiReceiver::setSource(const NdiSourceInfo& src)
{
    source = src;
}

void NdiReceiver::setMuted(bool m)
{
    muted = m;
}

void NdiReceiver::setBandwidth(NDIlib_recv_bandwidth_e bw)
{
    bandwidth_ = bw;
}

void NdiReceiver::setFpsLimit(int fps)
{
    fpsLimit = fps;
}

void NdiReceiver::requestStop()
{
    stopFlag = true;
}

void NdiReceiver::run()
{
    if (!p_NDI)
    {
        emit finished();
        return;
    }

    qDebug("NDI Receiver: [1] Creating receiver for '%s'", qPrintable(source.name));

    // Reconstruct NDIlib_source_t from the owned QString name.
    QByteArray nameUtf8 = source.name.toUtf8();
    NDIlib_source_t ndiSrc;
    ndiSrc.p_ndi_name = nameUtf8.constData();
    ndiSrc.p_url_address = nullptr;  // NDI resolves URL from name.

    NDIlib_recv_create_v3_t settings;
    settings.source_to_connect_to = ndiSrc;
    settings.color_format = NDIlib_recv_color_format_BGRX_BGRA;
    settings.bandwidth = bandwidth_;
    settings.allow_video_fields = true;
    settings.p_ndi_recv_name = "CasparCG Client";

    qDebug("NDI Receiver: [2] Calling recv_create_v3");
    NDIlib_recv_instance_t recv = p_NDI->recv_create_v3(&settings);
    if (!recv)
    {
        qWarning("NDI Receiver: Failed to create receiver for %s",
                 qPrintable(source.name));
        emit finished();
        return;
    }

    qDebug("NDI Receiver: [3] Receiver created, entering capture loop");
    emit connectionStateChanged(true);

    NDIlib_video_frame_v2_t video;
    NDIlib_audio_frame_v3_t audio;
    NDIlib_metadata_frame_t metadata;

    int frameCount = 0;
    QElapsedTimer frameTimer;
    frameTimer.start();
    qint64 lastFrameEmitMs = 0;

    while (!stopFlag)
    {
        NDIlib_frame_type_e type = p_NDI->recv_capture_v3(recv, &video, &audio, &metadata, 100);

        switch (type)
        {
        case NDIlib_frame_type_video:
        {
            if (frameCount < 3)
                qDebug("NDI Receiver: [V] Video frame %d: %dx%d stride=%d p_data=%p",
                       frameCount, video.xres, video.yres,
                       video.line_stride_in_bytes, (void*)video.p_data);

            if (!video.p_data || video.xres <= 0 || video.yres <= 0 ||
                video.line_stride_in_bytes <= 0)
            {
                qWarning("NDI Receiver: Invalid video frame, skipping");
                p_NDI->recv_free_video_v2(recv, &video);
                break;
            }

            // Frame rate limiting: skip frames if emitting too fast.
            int limit = fpsLimit.load();
            if (limit > 0)
            {
                qint64 nowMs = frameTimer.elapsed();
                if ((nowMs - lastFrameEmitMs) < (1000 / limit))
                {
                    p_NDI->recv_free_video_v2(recv, &video);
                    break;
                }
                lastFrameEmitMs = nowMs;
            }

            // BGRX/BGRA -> QImage::Format_ARGB32 (native Qt byte order on little-endian).
            QImage img(video.p_data, video.xres, video.yres,
                       video.line_stride_in_bytes, QImage::Format_ARGB32);
            emit videoFrameReceived(img.copy());  // Deep copy before freeing.
            p_NDI->recv_free_video_v2(recv, &video);
            frameCount++;
            break;
        }

        case NDIlib_frame_type_audio:
        {
            if (frameCount < 3)
                qDebug("NDI Receiver: [A] Audio frame: rate=%d ch=%d samples=%d",
                       audio.sample_rate, audio.no_channels, audio.no_samples);

            if (!muted && p_NDI->util_audio_to_interleaved_16s_v3)
            {
                int srcChannels = audio.no_channels;
                int samples = audio.no_samples;

                if (srcChannels > 0 && samples > 0)
                {
                    // Lazily create audio sink on first audio frame.
                    if (!audioSink)
                        setupAudioSink(audio.sample_rate, srcChannels);

                    if (audioDevice)
                    {
                        // Allocate for ALL source channels — the conversion
                        // function may write all channels regardless of
                        // no_channels in the destination struct.
                        std::vector<int16_t> pcmBuf(samples * srcChannels);

                        NDIlib_audio_frame_interleaved_16s_t pcm;
                        pcm.sample_rate = audio.sample_rate;
                        pcm.no_channels = srcChannels;
                        pcm.no_samples = samples;
                        pcm.reference_level = 20;
                        pcm.p_data = pcmBuf.data();

                        p_NDI->util_audio_to_interleaved_16s_v3(&audio, &pcm);

                        if (srcChannels <= 2)
                        {
                            audioDevice->write(
                                reinterpret_cast<const char*>(pcmBuf.data()),
                                samples * srcChannels * sizeof(int16_t));
                        }
                        else
                        {
                            // Extract first 2 channels from interleaved N-channel data.
                            std::vector<int16_t> stereoBuf(samples * 2);
                            for (int s = 0; s < samples; s++)
                            {
                                stereoBuf[s * 2]     = pcmBuf[s * srcChannels];
                                stereoBuf[s * 2 + 1] = pcmBuf[s * srcChannels + 1];
                            }
                            audioDevice->write(
                                reinterpret_cast<const char*>(stereoBuf.data()),
                                samples * 2 * sizeof(int16_t));
                        }
                    }
                }
            }

            p_NDI->recv_free_audio_v3(recv, &audio);
            break;
        }

        case NDIlib_frame_type_error:
            qWarning("NDI Receiver: Connection lost to %s", qPrintable(source.name));
            emit connectionStateChanged(false);
            break;

        case NDIlib_frame_type_metadata:
            p_NDI->recv_free_metadata(recv, &metadata);
            break;

        case NDIlib_frame_type_none:
            break;

        default:
            break;
        }
    }

    qDebug("NDI Receiver: [4] Capture loop ended, cleaning up");
    emit connectionStateChanged(false);
    p_NDI->recv_destroy(recv);
    qDebug("NDI Receiver: [5] Disconnected from %s", qPrintable(source.name));

    emit finished();
}

void NdiReceiver::setupAudioSink(int sampleRate, int channels)
{
    qDebug("NDI Receiver: Setting up audio sink: rate=%d ch=%d", sampleRate, channels);

    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(qMin(channels, 2));
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    if (!defaultDevice.isFormatSupported(format))
    {
        qWarning("NDI Receiver: Audio format not supported by default output device");
        return;
    }

    audioSink = new QAudioSink(defaultDevice, format);
    audioDevice = audioSink->start();
    qDebug("NDI Receiver: Audio sink started, device=%p", (void*)audioDevice);
}
