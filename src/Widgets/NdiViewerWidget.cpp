#include "NdiViewerWidget.h"

#include <QtCore/QDebug>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QPixmap>

NdiViewerWidget::NdiViewerWidget(QWidget* parent)
    : QWidget(parent)
{
    // Video display label — fills the widget, black background.
    this->videoLabel = new QLabel(this);
    this->videoLabel->setAlignment(Qt::AlignCenter);
    this->videoLabel->setStyleSheet("QLabel { background-color: black; }");
    this->videoLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // Source name overlay — semi-transparent, bottom-left.
    this->overlayLabel = new QLabel(this);
    this->overlayLabel->setText("No Source");
    setOverlayStyle(false);
    this->overlayLabel->setFixedHeight(18);
    this->overlayLabel->adjustSize();

    // Context menu.
    this->contextMenu = new QMenu(this);
    this->sourceMenu = new QMenu("Set Source", this);
    this->contextMenu->addMenu(this->sourceMenu);
    this->disconnectAction = this->contextMenu->addAction("Disconnect");
    this->contextMenu->addSeparator();
    this->muteAction = this->contextMenu->addAction("Mute");
    this->muteAction->setCheckable(true);

    QObject::connect(this->sourceMenu, &QMenu::aboutToShow, this, &NdiViewerWidget::setupSourceMenu);
    QObject::connect(this->disconnectAction, &QAction::triggered, this, &NdiViewerWidget::disconnectSource);
    QObject::connect(this->muteAction, &QAction::toggled, this, [this](bool checked) {
        setMuted(checked);
    });

    setMinimumSize(0, 0);
}

NdiViewerWidget::~NdiViewerWidget()
{
    stopReceiver();
}

QString NdiViewerWidget::sourceName() const
{
    return currentSource.name;
}

bool NdiViewerWidget::isMuted() const
{
    return muted_;
}

void NdiViewerWidget::connectToSource(const NdiSourceInfo& source)
{
    stopReceiver();

    currentSource = source;
    connected_ = false;
    this->overlayLabel->setText("Connecting...");
    setOverlayStyle(false);
    this->overlayLabel->adjustSize();
    updateOverlayPosition();

    startReceiver();
    emit sourceChanged();
}

void NdiViewerWidget::disconnectSource()
{
    stopReceiver();

    currentSource = NdiSourceInfo();
    connected_ = false;
    this->overlayLabel->setText("No Source");
    setOverlayStyle(false);
    this->overlayLabel->adjustSize();
    updateOverlayPosition();
    this->videoLabel->clear();
    this->videoLabel->setStyleSheet("QLabel { background-color: black; }");

    emit sourceChanged();
}

void NdiViewerWidget::setMuted(bool muted)
{
    muted_ = muted;

    this->muteAction->blockSignals(true);
    this->muteAction->setChecked(muted);
    this->muteAction->blockSignals(false);

    if (receiver)
        receiver->setMuted(muted);
}

void NdiViewerWidget::setBandwidth(NDIlib_recv_bandwidth_e bw)
{
    if (bandwidth_ == bw)
        return;
    bandwidth_ = bw;
    // Reconnect to apply new bandwidth (NDI SDK sets bandwidth at receiver creation time).
    if (!currentSource.name.isEmpty())
        connectToSource(currentSource);
}

void NdiViewerWidget::setFpsLimit(int fps)
{
    fpsLimit_ = fps;
    if (receiver)
        receiver->setFpsLimit(fps);
}

void NdiViewerWidget::setScalingQuality(Qt::TransformationMode mode)
{
    scalingMode_ = mode;
}

void NdiViewerWidget::contextMenuEvent(QContextMenuEvent* event)
{
    this->contextMenu->exec(event->globalPos());
}

void NdiViewerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    this->videoLabel->setGeometry(0, 0, width(), height());
    updateOverlayPosition();
}

void NdiViewerWidget::updateOverlayPosition()
{
    int x = 4;
    int y = height() - this->overlayLabel->height() - 4;
    this->overlayLabel->move(x, qMax(y, 0));
    this->overlayLabel->raise();
}

void NdiViewerWidget::setOverlayStyle(bool signalLost)
{
    if (signalLost)
    {
        this->overlayLabel->setStyleSheet(
            "QLabel { background-color: rgba(180, 0, 0, 200); color: rgba(255, 255, 255, 255); "
            "padding: 2px 6px; font-size: 10px; border-radius: 2px; }");
    }
    else
    {
        this->overlayLabel->setStyleSheet(
            "QLabel { background-color: rgba(0, 0, 0, 160); color: rgba(200, 200, 200, 255); "
            "padding: 2px 6px; font-size: 10px; border-radius: 2px; }");
    }
}

void NdiViewerWidget::startReceiver()
{
    if (currentSource.name.isEmpty())
        return;

    const NDIlib_v5* ndi = NdiManager::getInstance().api();
    if (!ndi)
        return;

    receiverThread = new QThread(this);
    receiverThread->setObjectName("NdiRecv_" + currentSource.name);

    receiver = new NdiReceiver(ndi);
    receiver->setSource(currentSource);
    receiver->setMuted(muted_);
    receiver->setBandwidth(bandwidth_);
    receiver->setFpsLimit(fpsLimit_);
    receiver->moveToThread(receiverThread);

    QObject::connect(receiverThread, &QThread::started, receiver, &NdiReceiver::run);
    QObject::connect(receiver, &NdiReceiver::videoFrameReceived, this, &NdiViewerWidget::onVideoFrame);
    QObject::connect(receiver, &NdiReceiver::connectionStateChanged, this, &NdiViewerWidget::onConnectionStateChanged);
    QObject::connect(receiver, &NdiReceiver::finished, receiverThread, &QThread::quit);
    QObject::connect(receiverThread, &QThread::finished, receiver, &QObject::deleteLater);

    receiverThread->start();
}

void NdiViewerWidget::stopReceiver()
{
    if (!receiverThread)
        return;

    // Disconnect signals from receiver to this widget before stopping,
    // preventing stale connectionStateChanged signals from arriving
    // after a manual disconnect (which would show "Signal Lost" instead of "No Source").
    QObject::disconnect(receiver, nullptr, this, nullptr);

    receiver->requestStop();
    receiverThread->quit();
    receiverThread->wait(3000);

    delete receiverThread;
    receiverThread = nullptr;
    receiver = nullptr;  // Deleted by deleteLater after thread finishes.
}

void NdiViewerWidget::onVideoFrame(const QImage& image)
{
    // First frame received — mark as connected.
    if (!connected_)
    {
        connected_ = true;
        this->overlayLabel->setText(currentSource.name);
        setOverlayStyle(false);
        this->overlayLabel->adjustSize();
        updateOverlayPosition();
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    this->videoLabel->setPixmap(pixmap.scaled(
        this->videoLabel->size(), Qt::KeepAspectRatio, scalingMode_));
}

void NdiViewerWidget::onConnectionStateChanged(bool connected)
{
    if (!connected)
    {
        connected_ = false;
        this->overlayLabel->setText("Signal Lost");
        setOverlayStyle(true);
        this->overlayLabel->adjustSize();
        updateOverlayPosition();
    }
}

void NdiViewerWidget::setupSourceMenu()
{
    this->sourceMenu->clear();

    QList<NdiSourceInfo> sources = NdiManager::getInstance().getSources();

    if (sources.isEmpty())
    {
        QAction* none = this->sourceMenu->addAction("(No sources found)");
        none->setEnabled(false);
        return;
    }

    for (const NdiSourceInfo& src : sources)
    {
        QAction* action = this->sourceMenu->addAction(src.name);
        if (src.name == currentSource.name)
        {
            action->setCheckable(true);
            action->setChecked(true);
        }

        // Capture source by value for the lambda.
        NdiSourceInfo capturedSrc = src;
        QObject::connect(action, &QAction::triggered, this, [this, capturedSrc]() {
            connectToSource(capturedSrc);
        });
    }
}
