#include "LiveWidget.h"

#include "Global.h"
#include "PanelHelper.h"

#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "EventManager.h"
#include "Models/ConfigurationModel.h"
#include "Models/DeviceModel.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QTimer>
#include <QtWidgets/QLayout>
#include <QtWidgets/QToolButton>

LiveWidget::LiveWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
    setupMenus();

    this->collapsed = PanelHelper::isPanelCollapsed("Live");
    if (this->collapsed)
        this->expandCollapseAction->setText("Expand");

    this->setFixedHeight(Panel::DEFAULT_LIVE_HEIGHT);

    // Reapply fixed height after the event loop has resolved the initial layout.
    QTimer::singleShot(0, this, [this]() {
        PanelHelper::applyExpandedHeight(this, "Live", Panel::DEFAULT_LIVE_HEIGHT);
    });

    this->liveDialog = new LiveDialog(this);
    QObject::connect(this->liveDialog, SIGNAL(rejected()), this, SLOT(toggleWindowMode()));

    QString streamPort = DatabaseManager::getInstance().getConfigurationByName("StreamPort").getValue();
    this->streamPort = (streamPort.isEmpty() == true) ? Stream::DEFAULT_PORT : streamPort.toInt();

    QObject::connect(&EventManager::getInstance(), SIGNAL(closeApplication(const CloseApplicationEvent &)), this, SLOT(closeApplication(const CloseApplicationEvent &)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(disconnectStream()), this, SLOT(disconnectStream()));
}

void LiveWidget::closeApplication(const CloseApplicationEvent &event)
{
    Q_UNUSED(event);

    stopStream();

    if (this->vlcMediaPlayer != nullptr)
    {
        libvlc_media_player_release(this->vlcMediaPlayer);
        this->vlcMediaPlayer = nullptr;
    }

    if (this->vlcInstance != nullptr)
    {
        libvlc_release(this->vlcInstance);
        this->vlcInstance = nullptr;
    }
}

void LiveWidget::setupMenus()
{
    // Dropdown menu with stream, audio, window mode, and collapse options.
    this->contextMenuLiveDropdown = new QMenu(this);
    this->contextMenuLiveDropdown->setObjectName("panelMenu");
    this->contextMenuLiveDropdown->setTitle("Dropdown");

    PanelHelper::addMoveActions(this->contextMenuLiveDropdown, "Live", this);
    this->contextMenuLiveDropdown->addSeparator();

    this->audioTrackMenu = new QMenu(this);
    this->audioTrackMenu->setTitle("Audio Track");

    this->audioMenu = new QMenu(this);
    this->audioMenu->setTitle("Audio");
    this->audioTrackMenuAction = this->audioMenu->addMenu(this->audioTrackMenu);
    this->audioMenu->addSeparator();
    this->muteAction = this->audioMenu->addAction("Mute");
    this->muteAction->setCheckable(true);

    this->streamMenu = new QMenu(this);
    this->streamMenu->setTitle("Connect to");
    this->streamMenuAction = this->contextMenuLiveDropdown->addMenu(this->streamMenu);
    this->contextMenuLiveDropdown->addSeparator();
    this->contextMenuLiveDropdown->addMenu(this->audioMenu);
    this->contextMenuLiveDropdown->addSeparator();
    this->windowModeAction = this->contextMenuLiveDropdown->addAction("Window Mode", this, SLOT(toggleWindowMode()));
    this->windowModeAction->setCheckable(true);
    this->contextMenuLiveDropdown->addSeparator();
    this->expandCollapseAction = this->contextMenuLiveDropdown->addAction("Collapse", this, SLOT(toggleExpandCollapse()));

    QObject::connect(this->streamMenu, SIGNAL(aboutToShow()), this, SLOT(setupStreamMenu()));
    QObject::connect(this->streamMenu, SIGNAL(triggered(QAction *)), this, SLOT(streamMenuActionTriggered(QAction *)));
    QObject::connect(this->audioTrackMenu, SIGNAL(aboutToShow()), this, SLOT(setupAudioTrackMenu()));
    QObject::connect(this->audioTrackMenu, SIGNAL(triggered(QAction *)), this, SLOT(audioMenuActionTriggered(QAction *)));
    QObject::connect(this->muteAction, SIGNAL(toggled(bool)), this, SLOT(muteAudio(bool)));

    // Corner widget: dropdown menu button (matching RundownWidget pattern).
    QToolButton* toolButtonLiveDropdown = new QToolButton(this->tabWidgetLive);
    toolButtonLiveDropdown->setObjectName("toolButtonLiveDropdown");
    toolButtonLiveDropdown->setText(QString::fromUtf8("\xe2\x89\xa1"));
    toolButtonLiveDropdown->setFixedSize(22, 22);
    toolButtonLiveDropdown->setMenu(this->contextMenuLiveDropdown);
    toolButtonLiveDropdown->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetLive->setCornerWidget(toolButtonLiveDropdown);
}

void LiveWidget::setupAudioTrackMenu()
{
    if (!this->vlcMediaPlayer)
        return;

    if (!libvlc_media_player_is_playing(this->vlcMediaPlayer))
        return;

    foreach (QAction *action, this->audioTrackMenu->actions())
        this->audioTrackMenu->removeAction(action);

    int currentTrack = libvlc_audio_get_track(this->vlcMediaPlayer);
    libvlc_track_description_t *trackDescription = libvlc_audio_get_track_description(this->vlcMediaPlayer);
    while (trackDescription != NULL)
    {
        this->audioTrackAction = this->audioTrackMenu->addAction(trackDescription->psz_name);
        this->audioTrackAction->setCheckable(true);
        this->audioTrackAction->setData(trackDescription->i_id);

        if (trackDescription->i_id == currentTrack)
            this->audioTrackAction->setChecked(true);

        trackDescription = trackDescription->p_next;
    }
}

void LiveWidget::setupStreamMenu()
{
    foreach (QAction *action, this->streamMenu->actions())
        this->streamMenu->removeAction(action);

    QList<DeviceModel> models = DatabaseManager::getInstance().getDevice();
    foreach (DeviceModel model, models)
    {
        for (int i = 0; i < model.getChannels(); i++)
        {
            this->streamMenu->addAction(QString("Device: %1, Channel: %2 (Fill)").arg(model.getName()).arg(i + 1));
            this->streamMenu->addAction(QString("Device: %1, Channel: %2 (Key)").arg(model.getName()).arg(i + 1));
        }
    }

    this->streamMenu->addSeparator();
    this->streamMenu->addAction("Disconnect", this, SLOT(disconnectStream()));
}

void LiveWidget::audioMenuActionTriggered(QAction *action)
{
    if (!this->vlcMediaPlayer)
        return;

    libvlc_audio_set_track(this->vlcMediaPlayer, action->data().toInt());
}

void LiveWidget::streamMenuActionTriggered(QAction *action)
{
    if (!action->text().contains(',') && !action->text().contains(':'))
        return;

    stopStream();

    this->useKey = action->text().contains("(Key)");
    this->deviceName = action->text().split(',').at(0).split(':').at(1).trimmed();
    this->deviceChannel = action->text().split(',').at(1).split(':').at(1).trimmed().split(' ').at(0).trimmed();

    startStream();
}

void LiveWidget::disconnectStream()
{
    stopStream();

    this->deviceName.clear();
    this->deviceChannel.clear();
}

void LiveWidget::startStream()
{
    if (!this->deviceName.isEmpty() && !this->deviceChannel.isEmpty())
    {
        bool disableAudioInStream = (DatabaseManager::getInstance().getConfigurationByName("DisableAudioInStream").getValue() == "true") ? true : false;

        char *vlcArguments[] = {
            qstrdup("--ignore-config"),
            qstrdup("--deinterlace=-1"),
            qstrdup("--deinterlace-mode=yadif"),
            qstrdup("--video-filter=deinterlace"),
            QString("--verbose=%1").arg(DatabaseManager::getInstance().getConfigurationByName("LogLevel").getValue()).toUtf8().data(),
            QString("--network-caching=%1").arg(DatabaseManager::getInstance().getConfigurationByName("NetworkCache").getValue()).toUtf8().data(),
            qstrdup("--no-audio"),
        };
        int len = 6;
        if (disableAudioInStream)
            len += 1;

        // Set VLC plugin path relative to the executable
#ifdef Q_OS_MAC
        QString appPath = QCoreApplication::applicationDirPath();
        QString pluginPath = QDir(appPath).filePath("../Frameworks/plugins");
        QByteArray pluginPathBytes = QDir::cleanPath(pluginPath).toUtf8();
        setenv("VLC_PLUGIN_PATH", pluginPathBytes.constData(), 1);

        qDebug("Setting VLC_PLUGIN_PATH to: %s", pluginPathBytes.constData());
#endif

        this->vlcInstance = libvlc_new(len, vlcArguments);
        if (this->vlcInstance)
        {
            this->vlcMediaPlayer = libvlc_media_player_new(this->vlcInstance);
            if (this->vlcMediaPlayer)
            {
                this->vlcMedia = libvlc_media_new_location(this->vlcInstance, QString("udp://@0.0.0.0:%1").arg(this->streamPort).toStdString().c_str());
                if (this->vlcMedia)
                {
                    libvlc_media_player_set_media(this->vlcMediaPlayer, this->vlcMedia);

                    setupRenderTarget(this->windowMode);

                    libvlc_media_player_play(this->vlcMediaPlayer);

                    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->deviceName);
                    if (device != NULL && device->isConnected())
                    {
                        int quality = DatabaseManager::getInstance().getConfigurationByName("StreamQuality").getValue().toInt();

                        if (this->windowMode)
                            device->startStream(this->deviceChannel.toInt(), this->streamPort, quality, this->useKey);
                        else
                            device->startStream(this->deviceChannel.toInt(), this->streamPort, quality, this->useKey, Stream::COMPACT_WIDTH, Stream::COMPACT_HEIGHT);
                    }
                }
            }
        }
        else
        {
            auto err = "Failed to initialise libvlc";
            fprintf(stderr, "%s\n", err);

#ifdef Q_OS_MAC
            char *pluginPath = getenv("VLC_PLUGIN_PATH");
            if (pluginPath)
            {
                fprintf(stderr, "VLC_PLUGIN_PATH was set to: %s\n", pluginPath);
                fprintf(stderr, "Please verify that VLC plugins exist at this location\n");
            }
            else
            {
                fprintf(stderr, "VLC_PLUGIN_PATH was not set\n");
            }
#endif
        }
    }
}

void LiveWidget::setupRenderTarget(bool windowMode)
{
    if (!this->vlcMediaPlayer)
        return;

#if defined(Q_OS_WIN)
    libvlc_media_player_set_hwnd(this->vlcMediaPlayer, (windowMode == true) ? (void *)this->liveDialog->getRenderTarget()->winId() : (void *)this->labelLiveSmall->winId());
#elif defined(Q_OS_MAC)
    libvlc_media_player_set_nsobject(this->vlcMediaPlayer, (windowMode == true) ? (void *)this->liveDialog->getRenderTarget()->winId() : (void *)this->labelLiveSmall->winId());
#elif defined(Q_OS_LINUX)
    libvlc_media_player_set_xwindow(this->vlcMediaPlayer, (windowMode == true) ? this->liveDialog->getRenderTarget()->winId() : this->labelLiveSmall->winId());
#else
#error "Unsupported platform"
#endif

    auto err = libvlc_errmsg();
    if (err)
        fprintf(stderr, "%s\n", qPrintable(err));
}

void LiveWidget::stopStream()
{
    if (!this->vlcMediaPlayer)
        return;

    if (this->deviceName.isEmpty() || this->deviceChannel.isEmpty())
        return;

    libvlc_media_player_stop(this->vlcMediaPlayer);

    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->deviceName);
    if (device != NULL && device->isConnected())
        device->stopStream(this->deviceChannel.toInt(), this->streamPort);
}

void LiveWidget::muteAudio(bool mute)
{
    this->muted = mute;

    if (this->vlcMediaPlayer)
        libvlc_audio_set_mute(this->vlcMediaPlayer, mute);
}

void LiveWidget::toggleExpandCollapse()
{
    this->collapsed = !this->collapsed;
    PanelHelper::setPanelCollapsed("Live", this->collapsed);

    this->expandCollapseAction->setText(this->collapsed ? "Expand" : "Collapse");

    if (this->collapsed)
        this->setFixedHeight(Panel::COMPACT_LIVE_HEIGHT);
    else
        PanelHelper::applyExpandedHeight(this, "Live", Panel::DEFAULT_LIVE_HEIGHT);
}

void LiveWidget::toggleWindowMode()
{
    stopStream();

    if (this->windowMode)
    {
        this->windowModeAction->blockSignals(true);
        this->windowModeAction->setChecked(false);
        this->windowModeAction->blockSignals(false);

        this->liveDialog->hide();
    }
    else
    {
        this->liveDialog->visible();
    }

    this->windowMode = !this->windowMode;
    setupRenderTarget(this->windowMode);
    startStream();
}
