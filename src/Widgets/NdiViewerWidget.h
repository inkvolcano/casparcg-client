#pragma once

#include "Shared.h"

#include "NdiManager.h"
#include "NdiReceiver.h"

#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QWidget>

// Single NDI output viewer — displays video from one NDI source.
// Each viewer owns an NdiReceiver running on a dedicated QThread.
class WIDGETS_EXPORT NdiViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NdiViewerWidget(QWidget* parent = nullptr);
    ~NdiViewerWidget();

    QString sourceName() const;
    bool isMuted() const;

    void connectToSource(const NdiSourceInfo& source);
    void disconnectSource();
    void setMuted(bool muted);

    // When enabled (multi-viewer grids), the source-name overlay hides itself
    // 5 seconds after appearing; error states (No Source, Signal Lost) stay.
    void setLabelAutoHide(bool enabled);
    void setBandwidth(NDIlib_recv_bandwidth_e bw);
    void setFpsLimit(int fps);
    void setScalingQuality(Qt::TransformationMode mode);

    Q_SIGNAL void sourceChanged();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void startReceiver();
    void stopReceiver();
    void updateOverlayPosition();
    void restartLabelHide();

    Q_SLOT void onVideoFrame(const QImage& image);
    Q_SLOT void onConnectionStateChanged(bool connected);
    Q_SLOT void setupSourceMenu();

    void setOverlayStyle(bool signalLost);

    QLabel* videoLabel;
    QLabel* overlayLabel;
    QTimer* labelHideTimer = nullptr;
    bool labelAutoHide_ = false;

    QMenu* contextMenu;
    QMenu* sourceMenu;
    QAction* disconnectAction;
    QAction* muteAction;

    NdiSourceInfo currentSource;
    bool muted_ = false;
    bool connected_ = false;
    Qt::TransformationMode scalingMode_ = Qt::SmoothTransformation;
    NDIlib_recv_bandwidth_e bandwidth_ = NDIlib_recv_bandwidth_highest;
    int fpsLimit_ = 0;

    QThread* receiverThread = nullptr;
    NdiReceiver* receiver = nullptr;
};
