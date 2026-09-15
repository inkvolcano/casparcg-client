#pragma once

#include "Shared.h"
#include "NdiManager.h"

#include <QtCore/QObject>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioSink>

#include <atomic>

// Per-viewer NDI receiver.  Runs a capture loop on its own QThread.
// Emits video frames as QImage to the UI thread.
class NDI_EXPORT NdiReceiver : public QObject
{
    Q_OBJECT

public:
    explicit NdiReceiver(const NDIlib_v5* ndi, QObject* parent = nullptr);
    ~NdiReceiver();

    void setSource(const NdiSourceInfo& source);
    void setMuted(bool muted);
    void setBandwidth(NDIlib_recv_bandwidth_e bw);
    void setFpsLimit(int fps);  // 0 = no limit
    void requestStop();

    // Called on the GUI thread once the last frame sent has been drawn. Until it
    // is, the receiver drops new frames instead of queueing them.
    void frameShown();

    Q_SIGNAL void videoFrameReceived(const QImage& image);
    Q_SIGNAL void connectionStateChanged(bool connected);
    Q_SIGNAL void finished();

public slots:
    void run();

private:
    void setupAudioSink(int sampleRate, int channels);

    const NDIlib_v5* p_NDI;
    NdiSourceInfo source;
    std::atomic<bool> stopFlag{false};
    std::atomic<bool> muted{false};
    std::atomic<int> fpsLimit{0};

    // A frame is on its way to the GUI and has not been drawn yet. Each video
    // frame is a deep copy - about 8 MB at 1080p - posted to the GUI thread, and
    // nothing limited how many could wait there: a GUI thread busy for a second
    // held a second's worth of frames, per viewer.
    std::atomic<bool> framePending{false};
    NDIlib_recv_bandwidth_e bandwidth_ = NDIlib_recv_bandwidth_highest;

    QAudioSink* audioSink = nullptr;
    QIODevice* audioDevice = nullptr;
};
