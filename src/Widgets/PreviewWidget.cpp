#include "PreviewWidget.h"

#include "Global.h"
#include "PanelHelper.h"

#include "DatabaseManager.h"
#include "EventManager.h"
#include "Models/ConfigurationModel.h"
#include "Models/LibraryModel.h"
#include "Models/ThumbnailModel.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QVideoFrame>
#include <QtMultimedia/QVideoSink>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QToolButton>

PreviewWidget::PreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);

    // Create content widget (replaces the old QLabel).
    this->contentWidget = new PreviewContentWidget(this);

    // Create transport bar.
    this->transportBar = new QWidget(this);
    this->transportBar->setFixedHeight(22);
    this->transportBar->setVisible(false);

    this->playPauseButton = new QToolButton(this->transportBar);
    this->playPauseButton->setText(QString::fromUtf8("\xe2\x96\xb6")); // ▶
    this->playPauseButton->setFixedSize(22, 22);
    this->playPauseButton->setFocusPolicy(Qt::NoFocus);
    QObject::connect(this->playPauseButton, &QToolButton::clicked, this, &PreviewWidget::playPause);

    this->seekSlider = new QSlider(Qt::Horizontal, this->transportBar);
    this->seekSlider->setRange(0, 0);
    this->seekSlider->setFocusPolicy(Qt::NoFocus);
    QObject::connect(this->seekSlider, &QSlider::sliderPressed, this, &PreviewWidget::sliderPressed);
    QObject::connect(this->seekSlider, &QSlider::sliderReleased, this, &PreviewWidget::sliderReleased);
    QObject::connect(this->seekSlider, &QSlider::sliderMoved, this, &PreviewWidget::sliderMoved);

    this->timeLabel = new QLabel("0:00 / 0:00", this->transportBar);
    this->timeLabel->setFixedWidth(90);
    this->timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont timeFont = this->timeLabel->font();
    timeFont.setPixelSize(10);
    this->timeLabel->setFont(timeFont);

    QHBoxLayout* transportLayout = new QHBoxLayout(this->transportBar);
    transportLayout->setContentsMargins(2, 0, 2, 0);
    transportLayout->setSpacing(4);
    transportLayout->addWidget(this->playPauseButton);
    transportLayout->addWidget(this->seekSlider, 1);
    transportLayout->addWidget(this->timeLabel);

    // Add content + transport to the tab's layout.
    this->verticalLayout->addWidget(this->contentWidget, 1);
    this->verticalLayout->addWidget(this->transportBar, 0);

    // Set up video player.
    this->player = new QMediaPlayer(this);
    this->videoSink = new QVideoSink(this);
    this->player->setVideoSink(this->videoSink);

    QObject::connect(this->player, &QMediaPlayer::positionChanged, this, &PreviewWidget::positionChanged);
    QObject::connect(this->player, &QMediaPlayer::durationChanged, this, &PreviewWidget::durationChanged);

    QObject::connect(this->videoSink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame& frame) {
        QVideoFrame f = frame;
        if (f.map(QVideoFrame::ReadOnly))
        {
            this->contentWidget->setImage(f.toImage());
            f.unmap();
        }
    });

    setupMenus();

    this->collapsed = PanelHelper::isPanelCollapsed("Preview");
    if (this->collapsed)
        this->expandCollapseAction->setText("Expand");

    this->setFixedHeight(Panel::DEFAULT_PREVIEW_HEIGHT);

    QTimer::singleShot(0, this, [this]() {
        PanelHelper::applyExpandedHeight(this, "Preview", Panel::DEFAULT_PREVIEW_HEIGHT);
    });

    QObject::connect(&EventManager::getInstance(), SIGNAL(libraryItemSelected(const LibraryItemSelectedEvent&)), this, SLOT(libraryItemSelected(const LibraryItemSelectedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(targetChanged(const TargetChangedEvent&)), this, SLOT(targetChanged(const TargetChangedEvent&)));
}

void PreviewWidget::setupMenus()
{
    this->dropdownMenu = new QMenu(this);
    this->dropdownMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->dropdownMenu, "Preview", this);
    this->dropdownMenu->addSeparator();
    this->expandCollapseAction = this->dropdownMenu->addAction("Collapse", this, &PreviewWidget::toggleExpandCollapse);

    this->menuButton = new QToolButton(this->tabWidgetPreview);
    this->menuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->menuButton->setFixedSize(22, 22);
    this->menuButton->setMenu(this->dropdownMenu);
    this->menuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetPreview->setCornerWidget(this->menuButton);
}

void PreviewWidget::targetChanged(const TargetChangedEvent& event)
{
    this->model->setName(event.getTarget());

    setThumbnail();
}

void PreviewWidget::libraryItemSelected(const LibraryItemSelectedEvent& event)
{
    this->model = event.getLibraryModel();

    setThumbnail();
}

void PreviewWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->model = event.getLibraryModel();

    setThumbnail();
}

void PreviewWidget::setThumbnail()
{
    // Stop any playing video first.
    stopVideo();

    if (this->model->getType() != Rundown::STILL && this->model->getType() != Rundown::MOVIE)
    {
        this->image = QImage();
        this->contentWidget->clearContent();
        return;
    }

    QString name = this->model->getName();
    QString deviceName = this->model->getDeviceName();

    // Load thumbnail.
    QString data = DatabaseManager::getInstance().getThumbnailByNameAndDeviceName(name, deviceName).getData();

    if (!data.isEmpty())
    {
        this->image.loadFromData(QByteArray::fromBase64(data.toLatin1()), "PNG");
        updateThumbnailDisplay();
    }
    else
    {
        this->image = QImage();
        this->contentWidget->clearContent();
    }

    // For movies, try to load the local video file.
    if (this->model->getType() == Rundown::MOVIE)
    {
        QString filePath = resolveMediaFile(deviceName, name);
        if (!filePath.isEmpty())
            loadVideo(filePath);
    }
}

void PreviewWidget::updateThumbnailDisplay()
{
    if (this->image.isNull())
        return;

    this->contentWidget->setImage(this->image);
}

QString PreviewWidget::resolveMediaFile(const QString& deviceName, const QString& mediaName)
{
    DeviceModel device = DatabaseManager::getInstance().getDeviceByName(deviceName);
    QString mediaPath = device.getMediaPath();

    if (mediaPath.isEmpty())
        return QString();

    // CasparCG media names use forward slashes for subdirs and no extension.
    QString baseName = mediaName;
    baseName.replace('\\', '/');

    QFileInfo baseInfo(mediaPath + "/" + baseName);
    QDir dir = baseInfo.dir();

    if (!dir.exists())
        return QString();

    // Search for the file with any common video extension.
    QStringList filters;
    QString nameOnly = baseInfo.fileName();
    for (const QString& ext : {".mov", ".mp4", ".mxf", ".avi", ".mkv", ".wmv", ".webm", ".mpg", ".mpeg", ".ts", ".m4v"})
        filters << (nameOnly + ext);

    QStringList matches = dir.entryList(filters, QDir::Files, QDir::Name);
    if (!matches.isEmpty())
        return dir.absoluteFilePath(matches.first());

    return QString();
}

void PreviewWidget::loadVideo(const QString& filePath)
{
    this->player->setSource(QUrl::fromLocalFile(filePath));
    this->player->pause();
    this->transportBar->setVisible(true);
    this->playPauseButton->setText(QString::fromUtf8("\xe2\x96\xb6")); // ▶
}

void PreviewWidget::stopVideo()
{
    this->player->stop();
    this->player->setSource(QUrl());
    this->transportBar->setVisible(false);
    this->seekSlider->setRange(0, 0);
    this->timeLabel->setText("0:00 / 0:00");
}

QString PreviewWidget::formatTime(qint64 ms)
{
    int totalSec = static_cast<int>(ms / 1000);
    int min = totalSec / 60;
    int sec = totalSec % 60;
    return QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0'));
}

void PreviewWidget::playPause()
{
    if (this->player->playbackState() == QMediaPlayer::PlayingState)
    {
        this->player->pause();
        this->playPauseButton->setText(QString::fromUtf8("\xe2\x96\xb6")); // ▶
    }
    else
    {
        this->player->play();
        this->playPauseButton->setText(QString::fromUtf8("\xe2\x8f\xb8")); // ⏸
    }
}

void PreviewWidget::positionChanged(qint64 position)
{
    if (!this->sliderDragging)
        this->seekSlider->setValue(static_cast<int>(position));

    qint64 duration = this->player->duration();
    this->timeLabel->setText(formatTime(position) + " / " + formatTime(duration));
}

void PreviewWidget::durationChanged(qint64 duration)
{
    this->seekSlider->setRange(0, static_cast<int>(duration));
}

void PreviewWidget::sliderPressed()
{
    this->sliderDragging = true;
}

void PreviewWidget::sliderReleased()
{
    this->sliderDragging = false;
    this->player->setPosition(this->seekSlider->value());
}

void PreviewWidget::sliderMoved(int value)
{
    qint64 duration = this->player->duration();
    this->timeLabel->setText(formatTime(value) + " / " + formatTime(duration));
}

void PreviewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // Only update thumbnail display if no video is playing.
    if (this->player->playbackState() == QMediaPlayer::StoppedState &&
        this->player->source().isEmpty())
    {
        updateThumbnailDisplay();
    }
}

void PreviewWidget::toggleExpandCollapse()
{
    this->collapsed = !this->collapsed;
    PanelHelper::setPanelCollapsed("Preview", this->collapsed);

    this->expandCollapseAction->setText(this->collapsed ? "Expand" : "Collapse");

    if (this->collapsed)
        this->setFixedHeight(Panel::COMPACT_PREVIEW_HEIGHT);
    else
        PanelHelper::applyExpandedHeight(this, "Preview", Panel::DEFAULT_PREVIEW_HEIGHT);
}
