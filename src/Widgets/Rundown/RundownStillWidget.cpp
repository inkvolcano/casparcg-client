#include "RundownStillWidget.h"

#include "Global.h"

#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "GpiManager.h"
#include "EventManager.h"
#include "Events/ConnectionStateChangedEvent.h"
#include "Utils/ItemScheduler.h"

#include <QtCore/QObject>

#include <QtWidgets/QGraphicsOpacityEffect>

#include "RundownWidgetHelper.h"

RundownStillWidget::RundownStillWidget(const LibraryModel& model, QWidget* parent, const QString& color,
                                       bool active, bool loaded, bool paused, bool playing, bool inGroup,
                                       bool compactView)
    : QWidget(parent),
      active(active), loaded(loaded), paused(paused), playing(playing), inGroup(inGroup),
      compactView(compactView), color(color), model(model), stopControlSubscription(NULL),
      playControlSubscription(NULL), playNowControlSubscription(NULL), loadControlSubscription(NULL), pauseControlSubscription(NULL),
      nextControlSubscription(NULL), clearControlSubscription(NULL), clearVideolayerControlSubscription(NULL), clearChannelControlSubscription(NULL)
{
    setupUi(this);

    this->animation = new ActiveAnimation(this->labelActiveColor);

    this->labelThumbnail->hide();
    this->labelAutoPlay->setVisible(false);

    this->delayType = RundownWidgetHelper::cachedDelayType();
    this->markUsedItems = RundownWidgetHelper::cachedMarkUsedItems();

    setThumbnail();
    setColor(this->color);
    setActive(this->active);
    setCompactView(this->compactView);

    this->command.setImageName(this->model.getName());

    this->labelGroupColor->setVisible(this->inGroup);
    this->labelGroupColor->setStyleSheet(QString("background-color: %1;").arg(Color::DEFAULT_GROUP_COLOR));

    this->labelLabel->setText(this->model.getLabel().split('/').last());
    this->labelChannel->setText(QString("%1").arg(this->command.getChannel()));
    RundownWidgetHelper::setupChannelBadge(this->frameItem, this->labelColor, this->command.getChannel(), this->command.getVideolayer());
    QLabel* bankBadge = RundownWidgetHelper::createBankBadge(this->frameItem);
    QObject::connect(&this->command, &AbstractCommand::triggerBankChanged, [this, bankBadge](int bank) {
        RundownWidgetHelper::updateBankBadge(bankBadge, bank);
        RundownWidgetHelper::configureBankOscSubscriptions(this, this, bank);
    });
    RundownWidgetHelper::updateBankBadge(bankBadge, this->command.getTriggerBank());
    RundownWidgetHelper::configureBankOscSubscriptions(this, this, this->command.getTriggerBank());
    RundownWidgetHelper::setupCloneSupport(this, this->frameItem, &this->command);
    this->labelChannel->setVisible(false);
    this->labelVideolayer->setText(QString::fromUtf8("\xe2\xa7\x89 %1").arg(this->command.getVideolayer()));
    this->labelDelay->setText(RundownWidgetHelper::formatDelay(this->command.getDelay(), this->delayType, RundownWidgetHelper::getChannelFps(this->model.getDeviceName(), this->command.getChannel())));
    this->labelDevice->setText(QString("%1").arg(this->model.getDeviceName()));
    updateDurationLabel();

    this->progressTimer = new QTimer(this);
    this->progressTimer->setInterval(200);
    QObject::connect(this->progressTimer, &QTimer::timeout, this, &RundownStillWidget::updateProgress);

    QObject::connect(&this->itemScheduler, SIGNAL(executePlay()), this, SLOT(executePlay()));
    QObject::connect(&this->itemScheduler, SIGNAL(executeStop()), this, SLOT(executeStop()));

    QObject::connect(&this->command, SIGNAL(channelChanged(int)), this, SLOT(channelChanged(int)));
    QObject::connect(&this->command, SIGNAL(videolayerChanged(int)), this, SLOT(videolayerChanged(int)));
    QObject::connect(&this->command, SIGNAL(delayChanged(int)), this, SLOT(delayChanged(int)));
    QObject::connect(&this->command, SIGNAL(durationChanged(int)), this, SLOT(durationChanged(int)));
    QObject::connect(&this->command, SIGNAL(autoPlayChanged(bool)), this, SLOT(autoPlayChanged(bool)));
    QObject::connect(&this->command, SIGNAL(allowGpiChanged(bool)), this, SLOT(allowGpiChanged(bool)));
    QObject::connect(&this->command, SIGNAL(remoteTriggerIdChanged(const QString&)), this, SLOT(remoteTriggerIdChanged(const QString&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(deviceChanged(const DeviceChangedEvent&)), this, SLOT(deviceChanged(const DeviceChangedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(targetChanged(const TargetChangedEvent&)), this, SLOT(targetChanged(const TargetChangedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(labelChanged(const LabelChangedEvent&)), this, SLOT(labelChanged(const LabelChangedEvent&)));

    QObject::connect(&DeviceManager::getInstance(), SIGNAL(deviceAdded(CasparDevice&)), this, SLOT(deviceAdded(CasparDevice&)));
    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device != NULL)
        QObject::connect(device.data(), SIGNAL(connectionStateChanged(CasparDevice&)), this, SLOT(deviceConnectionStateChanged(CasparDevice&)));

    QObject::connect(GpiManager::getInstance().getGpiDevice().data(), SIGNAL(connectionStateChanged(bool, GpiDevice*)), this, SLOT(gpiConnectionStateChanged(bool, GpiDevice*)));

    checkEmptyDevice();
    checkGpiConnection();
    checkDeviceConnection();
}

void RundownStillWidget::labelChanged(const LabelChangedEvent& event)
{
    // This event is not for us.
    if (!this->selected)
        return;

    this->model.setLabel(event.getLabel());
    this->labelLabel->setText(this->model.getLabel().split('/').last());
}

void RundownStillWidget::targetChanged(const TargetChangedEvent& event)
{
    // This event is not for us.
    if (!this->selected)
        return;

    this->model.setName(event.getTarget());
    this->command.setImageName(event.getTarget());

    setThumbnail();
}

void RundownStillWidget::deviceChanged(const DeviceChangedEvent& event)
{
    // This event is not for us.
    if (!this->selected)
        return;

    // Should we update the device name?
    if (!event.getDeviceName().isEmpty() && event.getDeviceName() != this->model.getDeviceName())
    {
        // Disconnect connectionStateChanged() from the old device.
        const QSharedPointer<CasparDevice> oldDevice = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
        if (oldDevice != NULL)
            QObject::disconnect(oldDevice.data(), SIGNAL(connectionStateChanged(CasparDevice&)), this, SLOT(deviceConnectionStateChanged(CasparDevice&)));

        // Update the model with the new device.
        this->model.setDeviceName(event.getDeviceName());
        this->labelDevice->setText(QString("%1").arg(this->model.getDeviceName()));

        // Connect connectionStateChanged() to the new device.
        const QSharedPointer<CasparDevice> newDevice = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
        if (newDevice != NULL)
            QObject::connect(newDevice.data(), SIGNAL(connectionStateChanged(CasparDevice&)), this, SLOT(deviceConnectionStateChanged(CasparDevice&)));
    }

    checkEmptyDevice();
    checkDeviceConnection();
}

AbstractRundownWidget* RundownStillWidget::clone()
{
    RundownStillWidget* widget = new RundownStillWidget(this->model, this->parentWidget(), this->color,
                                                        this->active, this->loaded, this->paused, this->playing,
                                                        this->inGroup, this->compactView);

    StillCommand* command = dynamic_cast<StillCommand*>(widget->getCommand());
    command->setChannel(this->command.getChannel());
    command->setVideolayer(this->command.getVideolayer());
    command->setDelay(this->command.getDelay());
    command->setDuration(this->command.getDuration());
    command->setAllowGpi(this->command.getAllowGpi());
    command->setAllowRemoteTriggering(this->command.getAllowRemoteTriggering());
    command->setRemoteTriggerId(this->command.getRemoteTriggerId());
    command->setImageName(this->command.getImageName());
    command->setTransition(this->command.getTransition());
    command->setTransitionDuration(this->command.getTransitionDuration());
    command->setTween(this->command.getTween());
    command->setDirection(this->command.getDirection());
    command->setUseAuto(this->command.getUseAuto());
    command->setTriggerOnNext(this->command.getTriggerOnNext());
    command->setAutoPlay(this->command.getAutoPlay());

    return widget;
}

void RundownStillWidget::setCompactView(bool compactView)
{
    if (compactView)
    {
        this->labelColor->setFixedSize(RundownWidgetHelper::BADGE_WIDTH, Rundown::COMPACT_ITEM_HEIGHT);
        this->labelIcon->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
        this->labelAutoPlay->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
        this->labelGpiConnected->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
        this->labelDisconnected->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
        this->labelThumbnail->setFixedSize(Rundown::COMPACT_THUMBNAIL_WIDTH, Rundown::COMPACT_THUMBNAIL_HEIGHT);
    }
    else
    {
        this->labelColor->setFixedSize(RundownWidgetHelper::BADGE_WIDTH, Rundown::DEFAULT_ITEM_HEIGHT);
        this->labelIcon->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
        this->labelAutoPlay->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
        this->labelGpiConnected->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
        this->labelDisconnected->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
        this->labelThumbnail->setFixedSize(Rundown::DEFAULT_THUMBNAIL_WIDTH, Rundown::DEFAULT_THUMBNAIL_HEIGHT);
    }

    this->compactView = compactView;
}

void RundownStillWidget::readProperties(boost::property_tree::wptree& pt)
{
    if (pt.count(L"color") > 0) setColor(QString::fromStdWString(pt.get<std::wstring>(L"color")));
}

void RundownStillWidget::writeProperties(QXmlStreamWriter& writer)
{
    writer.writeTextElement("color", this->color);
}

bool RundownStillWidget::isGroup() const
{
    return false;
}

bool RundownStillWidget::isInGroup() const
{
    return this->inGroup;
}

AbstractCommand* RundownStillWidget::getCommand()
{
    return &this->command;
}

LibraryModel* RundownStillWidget::getLibraryModel()
{
    return &this->model;
}

void RundownStillWidget::setThumbnail()
{
    QString data = DatabaseManager::getInstance().getThumbnailByNameAndDeviceName(this->model.getName(), this->model.getDeviceName()).getData();

    /*
    QString data = DatabaseManager::getInstance().getThumbnailById(this->model.getThumbnailId()).getData();
    if (data.isEmpty())
        data = DatabaseManager::getInstance().getThumbnailByNameAndDeviceName(this->model.getName(), this->model.getDeviceName()).getData();
    */

    QImage image;
    image.loadFromData(QByteArray::fromBase64(data.toLatin1()), "PNG");
    this->labelThumbnail->setPixmap(QPixmap::fromImage(image));

    bool displayThumbnailTooltip = RundownWidgetHelper::cachedShowThumbnailTooltip();
    if (displayThumbnailTooltip)
        this->labelThumbnail->setToolTip(QString("<img src=\"data:image/png;base64,%1 \"/>").arg(data));
}

void RundownStillWidget::setSelected(bool selected)
{
    this->selected = selected;
}

void RundownStillWidget::setActive(bool active)
{
    this->active = active;

    this->animation->stop();

    if (this->active)
        RundownWidgetHelper::setActiveColorPalette(this->labelActiveColor, this->command.getChannel());
    else
        RundownWidgetHelper::clearActiveColorPalette(this->labelActiveColor);
}

void RundownStillWidget::setInGroup(bool inGroup)
{
    this->inGroup = inGroup;
    this->labelGroupColor->setVisible(this->inGroup);

    if (!this->inGroup)
    {
        // Don't force-clear autoPlay when leaving group; top-level items can have autoPlay too.
    }
}

QString RundownStillWidget::getColor() const
{
    return this->color;
}

void RundownStillWidget::setColor(const QString& color)
{
    this->color = color;
    this->setStyleSheet(QString("#frameItem, #frameStatus { background-color: %1; }").arg(color));
}

void RundownStillWidget::checkEmptyDevice()
{
    if (this->labelDevice->text() == "Device: ")
        this->labelDevice->setStyleSheet("color: firebrick;");
    else
        this->labelDevice->setStyleSheet("");
}

void RundownStillWidget::clearDelayedCommands()
{
    this->itemScheduler.cancel();
    this->progressTimer->stop();

    this->sendAutoPlay = false;
    this->paused = false;
    this->loaded = false;
    this->playing = false;
}

void RundownStillWidget::setUsed(bool used)
{
    if (used)
    {
        if (this->frameItem->graphicsEffect() == NULL || this->labelThumbnail->graphicsEffect() == NULL)
        {
            QGraphicsOpacityEffect* frameItemEffect = new QGraphicsOpacityEffect(this);
            frameItemEffect->setOpacity(0.25);

            QGraphicsOpacityEffect* labelThumbnailEffect = new QGraphicsOpacityEffect(this);
            labelThumbnailEffect->setOpacity(0.25);

            this->frameItem->setGraphicsEffect(frameItemEffect);
            this->labelThumbnail->setGraphicsEffect(labelThumbnailEffect);
        }
    }
    else
    {
        this->frameItem->setGraphicsEffect(NULL);
        this->labelThumbnail->setGraphicsEffect(NULL);
    }
}

bool RundownStillWidget::executeCommand(Playout::PlayoutType type)
{
    if (type == Playout::PlayoutType::Stop)
    {
        this->sendAutoPlay = false; // Manual stop should not trigger auto-play.
        executeStop();
    }
    else if (type == Playout::PlayoutType::Play && !this->command.getTriggerOnNext())
    {
        if (this->command.getDelay() < 0)
            return true;

        if (!this->model.getDeviceName().isEmpty()) // The user need to select a device.
        {
            const QStringList& channelFormats = DatabaseManager::getInstance().getDeviceByName(this->model.getDeviceName()).getChannelFormats().split(",");
            if (this->command.getChannel() > channelFormats.count())
                return true;

            // If duration is set and autoPlay is enabled, trigger auto-play when it expires.
            if (this->command.getDuration() > 0 && this->command.getAutoPlay())
                this->sendAutoPlay = true;

            this->itemScheduler.schedulePlayAndStop(
                this->command.getDelay(),
                this->command.getDuration(),
                this->delayType,
                DatabaseManager::getInstance().getFormat(channelFormats[this->command.getChannel() - 1]).getFramesPerSecond().toDouble());
        }
    }
    else if (type == Playout::PlayoutType::PlayNow)
        executePlay();
    else if (type == Playout::PlayoutType::PauseResume)
        executePause();
    else if (type == Playout::PlayoutType::Load)
        executeLoad();
    else if (type == Playout::PlayoutType::Next && this->command.getTriggerOnNext())
        executePlay();
    else if (type == Playout::PlayoutType::Clear)
        executeClearVideolayer();
    else if (type == Playout::PlayoutType::ClearVideoLayer)
        executeClearVideolayer();
    else if (type == Playout::PlayoutType::ClearChannel)
        executeClearChannel();
    else if (type == Playout::PlayoutType::Preview)
        executePlayPreview();

    if (this->active)
    {
        this->animation->setChannel(this->command.getChannel());
        this->animation->start(1);
    }

    return true;
}

void RundownStillWidget::executeStop()
{
    this->itemScheduler.cancel();
    this->progressTimer->stop();

    // Fire AutoPlay event if this stop is from duration timer expiring.
    // Don't send stop command to CasparCG - let the next item replace this one with its transition.
    if (this->sendAutoPlay)
    {
        EventManager::getInstance().fireAutoPlayRundownItemEvent(AutoPlayRundownItemEvent(this));
        this->sendAutoPlay = false;

        this->paused = false;
        this->loaded = false;
        this->playing = false;
        return; // Don't stop the layer - next item will replace it seamlessly.
    }

    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device != NULL && device->isConnected())
    {
        device->stop(this->command.getChannel(), this->command.getVideolayer());
    }

    foreach (const DeviceModel& model, DeviceManager::getInstance().getDeviceModels())
    {
        if (model.getShadow() == "No")
            continue;

        const QSharedPointer<CasparDevice> deviceShadow = DeviceManager::getInstance().getDeviceByName(model.getName());
        if (deviceShadow != NULL && deviceShadow->isConnected())
            deviceShadow->stop(this->command.getChannel(), this->command.getVideolayer());
    }

    this->paused = false;
    this->loaded = false;
    this->playing = false;
}

void RundownStillWidget::executePlay()
{
    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device != NULL && device->isConnected())
    {
        // Apply embedded transforms atomically before content plays.
        if (this->command.getTransform().hasAnyTransform())
        {
            this->command.getTransform().applyDeferred(device.data(),
                this->command.getChannel(), this->command.getVideolayer());
            this->command.getTransform().commit(device.data(), this->command.getChannel());
        }

        if (this->loaded)
            device->play(this->command.getChannel(), this->command.getVideolayer());
        else
            device->playStill(this->command.getChannel(), this->command.getVideolayer(), this->command.getImageName(),
                              this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                              this->command.getDirection(), this->command.getUseAuto());

        // Entrance animation (after content is on-air).
        if (this->command.getTransform().entrance.has_value())
            this->command.getTransform().applyEntrance(device.data(),
                this->command.getChannel(), this->command.getVideolayer());
    }

    foreach (const DeviceModel& model, DeviceManager::getInstance().getDeviceModels())
    {
        if (model.getShadow() == "No")
            continue;

        const QSharedPointer<CasparDevice>  deviceShadow = DeviceManager::getInstance().getDeviceByName(model.getName());
        if (deviceShadow != NULL && deviceShadow->isConnected())
        {
            // Apply embedded transforms atomically before content plays.
            if (this->command.getTransform().hasAnyTransform())
            {
                this->command.getTransform().applyDeferred(deviceShadow.data(),
                    this->command.getChannel(), this->command.getVideolayer());
                this->command.getTransform().commit(deviceShadow.data(), this->command.getChannel());
            }

            if (this->loaded)
                deviceShadow->play(this->command.getChannel(), this->command.getVideolayer());
            else
                deviceShadow->playStill(this->command.getChannel(), this->command.getVideolayer(), this->command.getImageName(),
                                        this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                                        this->command.getDirection(), this->command.getUseAuto());

            // Entrance animation (after content is on-air).
            if (this->command.getTransform().entrance.has_value())
                this->command.getTransform().applyEntrance(deviceShadow.data(),
                    this->command.getChannel(), this->command.getVideolayer());
        }
    }

    if (this->markUsedItems)
        setUsed(true);

    this->paused = false;
    this->loaded = false;
    this->playing = true;

    // Start progress timer for stills with duration.
    if (this->command.getDuration() > 0)
    {
        QString durationUnit = DatabaseManager::getInstance().getConfigurationByName("DurationUnit").getValue();
        double fps = RundownWidgetHelper::getChannelFps(this->model.getDeviceName(), this->command.getChannel());

        if (durationUnit == Output::DEFAULT_DELAY_IN_FRAMES && fps > 0)
            this->progressDurationMs = static_cast<int>(this->command.getDuration() * (1000.0 / fps));
        else
            this->progressDurationMs = this->command.getDuration();

        this->progressElapsed.start();
        this->progressTimer->start();
    }
}

void RundownStillWidget::executePlayPreview()
{
    const QSharedPointer<DeviceModel> deviceModel = DeviceManager::getInstance().getDeviceModelByName(this->model.getDeviceName());
    if (deviceModel != NULL && deviceModel->getPreviewChannel() > 0)
    {
        const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
        if (device != NULL && device->isConnected())
        {
            device->playStill(deviceModel->getPreviewChannel(), this->command.getVideolayer(), this->command.getImageName(),
                              this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                              this->command.getDirection(), false);
        }
    }

    foreach (const DeviceModel& model, DeviceManager::getInstance().getDeviceModels())
    {
        if (model.getShadow() == "No")
            continue;

        if (model.getPreviewChannel() > 0)
        {
            const QSharedPointer<CasparDevice>  deviceShadow = DeviceManager::getInstance().getDeviceByName(model.getName());
            if (deviceShadow != NULL && deviceShadow->isConnected())
            {
                deviceShadow->playStill(model.getPreviewChannel(), this->command.getVideolayer(), this->command.getImageName(),
                                        this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                                        this->command.getDirection(), false);
            }
        }
    }
}

void RundownStillWidget::executePause()
{
    if (!this->playing)
        return;

    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device != NULL && device->isConnected())
    {
        if (this->paused)
            device->play(this->command.getChannel(), this->command.getVideolayer());
        else
            device->pause(this->command.getChannel(), this->command.getVideolayer());
    }

    foreach (const DeviceModel& model, DeviceManager::getInstance().getDeviceModels())
    {
        if (model.getShadow() == "No")
            continue;

        const QSharedPointer<CasparDevice> deviceShadow = DeviceManager::getInstance().getDeviceByName(model.getName());
        if (deviceShadow != NULL && deviceShadow->isConnected())
        {
            if (this->paused)
                deviceShadow->play(this->command.getChannel(), this->command.getVideolayer());
            else
                deviceShadow->pause(this->command.getChannel(), this->command.getVideolayer());
        }
    }

    this->paused = !this->paused;
}

void RundownStillWidget::executeLoad()
{
    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device != NULL && device->isConnected())
    {
        device->loadStill(this->command.getChannel(), this->command.getVideolayer(), this->command.getImageName(),
                          this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                          this->command.getDirection(), this->command.getUseAuto());
    }

    foreach (const DeviceModel& model, DeviceManager::getInstance().getDeviceModels())
    {
        if (model.getShadow() == "No")
            continue;

        const QSharedPointer<CasparDevice>  deviceShadow = DeviceManager::getInstance().getDeviceByName(model.getName());
        if (deviceShadow != NULL && deviceShadow->isConnected())
        {
            deviceShadow->loadStill(this->command.getChannel(), this->command.getVideolayer(), this->command.getImageName(),
                                    this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                                    this->command.getDirection(), this->command.getUseAuto());
        }
    }

    this->loaded = true;
    this->paused = false;
    this->playing = false;
}

void RundownStillWidget::executeClearVideolayer()
{
    this->itemScheduler.cancel();
    this->progressTimer->stop();
    this->sendAutoPlay = false;

    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device != NULL && device->isConnected())
    {
        device->clearVideolayer(this->command.getChannel(), this->command.getVideolayer());
    }

    foreach (const DeviceModel& model, DeviceManager::getInstance().getDeviceModels())
    {
        if (model.getShadow() == "No")
            continue;

        const QSharedPointer<CasparDevice> deviceShadow = DeviceManager::getInstance().getDeviceByName(model.getName());
        if (deviceShadow != NULL && deviceShadow->isConnected())
            deviceShadow->clearVideolayer(this->command.getChannel(), this->command.getVideolayer());
    }

    this->paused = false;
    this->loaded = false;
    this->playing = false;
}

void RundownStillWidget::executeClearChannel()
{
    this->itemScheduler.cancel();
    this->progressTimer->stop();
    this->sendAutoPlay = false;

    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device != NULL && device->isConnected())
    {
        device->clearChannel(this->command.getChannel());
        device->clearMixerChannel(this->command.getChannel());
    }

    foreach (const DeviceModel& model, DeviceManager::getInstance().getDeviceModels())
    {
        if (model.getShadow() == "No")
            continue;

        const QSharedPointer<CasparDevice> deviceShadow = DeviceManager::getInstance().getDeviceByName(model.getName());
        if (deviceShadow != NULL && deviceShadow->isConnected())
        {
            deviceShadow->clearChannel(this->command.getChannel());
            deviceShadow->clearMixerChannel(this->command.getChannel());
        }
    }

    this->paused = false;
    this->loaded = false;
    this->playing = false;
}

void RundownStillWidget::channelChanged(int channel)
{
    this->labelChannel->setText(QString("%1").arg(channel));
    RundownWidgetHelper::updateChannelBadge(this->labelColor, channel, this->command.getVideolayer());
}

void RundownStillWidget::videolayerChanged(int videolayer)
{
    this->labelVideolayer->setText(QString::fromUtf8("\xe2\xa7\x89 %1").arg(videolayer));
    RundownWidgetHelper::updateChannelBadge(this->labelColor, this->command.getChannel(), videolayer);
}

void RundownStillWidget::delayChanged(int delay)
{
    this->labelDelay->setText(RundownWidgetHelper::formatDelay(delay, this->delayType, RundownWidgetHelper::getChannelFps(this->model.getDeviceName(), this->command.getChannel())));
}

void RundownStillWidget::durationChanged(int duration)
{
    Q_UNUSED(duration);
    // Show auto-play icon when autoPlay is enabled and duration is set.
    this->labelAutoPlay->setVisible(this->command.getAutoPlay() && this->command.getDuration() > 0);
    updateDurationLabel();
}

void RundownStillWidget::autoPlayChanged(bool autoPlay)
{
    Q_UNUSED(autoPlay);
    // Show auto-play icon when autoPlay is enabled and duration is set.
    this->labelAutoPlay->setVisible(this->command.getAutoPlay() && this->command.getDuration() > 0);
}

void RundownStillWidget::updateDurationLabel()
{
    double fps = RundownWidgetHelper::getChannelFps(this->model.getDeviceName(), this->command.getChannel());
    this->labelDuration->setText(RundownWidgetHelper::formatDuration(this->command.getDuration(), fps));
}

void RundownStillWidget::checkGpiConnection()
{
    this->labelGpiConnected->setVisible(this->command.getAllowGpi());

    if (GpiManager::getInstance().getGpiDevice()->isConnected())
        this->labelGpiConnected->setPixmap(RundownWidgetHelper::gpiConnectedPixmap());
    else
        this->labelGpiConnected->setPixmap(RundownWidgetHelper::gpiDisconnectedPixmap());
}

void RundownStillWidget::checkDeviceConnection()
{
    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device == NULL)
        this->labelDisconnected->setVisible(true);
    else
        this->labelDisconnected->setVisible(!device->isConnected());
}

void RundownStillWidget::configureOscSubscriptions()
{
    if (!this->command.getAllowRemoteTriggering())
        return;

    delete this->stopControlSubscription;
    this->stopControlSubscription = nullptr;

    delete this->playControlSubscription;
    this->playControlSubscription = nullptr;

    delete this->playNowControlSubscription;
    this->playNowControlSubscription = nullptr;

    delete this->loadControlSubscription;
    this->loadControlSubscription = nullptr;

    delete this->pauseControlSubscription;
    this->pauseControlSubscription = nullptr;

    delete this->nextControlSubscription;
    this->nextControlSubscription = nullptr;

    delete this->clearControlSubscription;
    this->clearControlSubscription = nullptr;

    delete this->clearVideolayerControlSubscription;
    this->clearVideolayerControlSubscription = nullptr;

    delete this->clearChannelControlSubscription;
    this->clearChannelControlSubscription = nullptr;

    QString stopControlFilter = Osc::ITEM_CONTROL_STOP_FILTER;
    stopControlFilter.replace("#UID#", this->command.getRemoteTriggerId());
    this->stopControlSubscription = new OscSubscription(stopControlFilter, this);
    QObject::connect(this->stopControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(stopControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString playControlFilter = Osc::ITEM_CONTROL_PLAY_FILTER;
    playControlFilter.replace("#UID#", this->command.getRemoteTriggerId());
    this->playControlSubscription = new OscSubscription(playControlFilter, this);
    QObject::connect(this->playControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(playControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString playNowControlFilter = Osc::ITEM_CONTROL_PLAYNOW_FILTER;
    playNowControlFilter.replace("#UID#", this->command.getRemoteTriggerId());
    this->playNowControlSubscription = new OscSubscription(playNowControlFilter, this);
    QObject::connect(this->playNowControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(playNowControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString loadControlFilter = Osc::ITEM_CONTROL_LOAD_FILTER;
    loadControlFilter.replace("#UID#", this->command.getRemoteTriggerId());
    this->loadControlSubscription = new OscSubscription(loadControlFilter, this);
    QObject::connect(this->loadControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(loadControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString pauseControlFilter = Osc::ITEM_CONTROL_PAUSE_FILTER;
    pauseControlFilter.replace("#UID#", this->command.getRemoteTriggerId());
    this->pauseControlSubscription = new OscSubscription(pauseControlFilter, this);
    QObject::connect(this->pauseControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(pauseControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString nextControlFilter = Osc::ITEM_CONTROL_NEXT_FILTER;
    nextControlFilter.replace("#UID#", this->command.getRemoteTriggerId());
    this->nextControlSubscription = new OscSubscription(nextControlFilter, this);
    QObject::connect(this->nextControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(nextControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString clearControlFilter = Osc::ITEM_CONTROL_CLEAR_FILTER;
    clearControlFilter.replace("#UID#", this->command.getRemoteTriggerId());
    this->clearControlSubscription = new OscSubscription(clearControlFilter, this);
    QObject::connect(this->clearControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(clearControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString clearVideolayerControlFilter = Osc::ITEM_CONTROL_CLEARVIDEOLAYER_FILTER;
    clearVideolayerControlFilter.replace("#UID#", this->command.getRemoteTriggerId());
    this->clearVideolayerControlSubscription = new OscSubscription(clearVideolayerControlFilter, this);
    QObject::connect(this->clearVideolayerControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(clearVideolayerControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString clearChannelControlFilter = Osc::ITEM_CONTROL_CLEARCHANNEL_FILTER;
    clearChannelControlFilter.replace("#UID#", this->command.getRemoteTriggerId());
    this->clearChannelControlSubscription = new OscSubscription(clearChannelControlFilter, this);
    QObject::connect(this->clearChannelControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(clearChannelControlSubscriptionReceived(const QString&, const QList<QVariant>&)));
}

void RundownStillWidget::allowGpiChanged(bool allowGpi)
{
    Q_UNUSED(allowGpi);

    checkGpiConnection();
}

void RundownStillWidget::gpiConnectionStateChanged(bool connected, GpiDevice* device)
{
    Q_UNUSED(connected);
    Q_UNUSED(device);

    checkGpiConnection();
}

void RundownStillWidget::remoteTriggerIdChanged(const QString& remoteTriggerId)
{
    configureOscSubscriptions();

    if (remoteTriggerId.trimmed().isEmpty() || !this->command.getAllowRemoteTriggering())
        this->labelRemoteTriggerId->setText("");
    else
        this->labelRemoteTriggerId->setText(QString::fromUtf8("\xe2\x87\xa5 %1").arg(remoteTriggerId));
}

void RundownStillWidget::deviceConnectionStateChanged(CasparDevice& device)
{
    Q_UNUSED(device);

    checkDeviceConnection();
}

void RundownStillWidget::deviceAdded(CasparDevice& device)
{
    if (DeviceManager::getInstance().getDeviceModelByAddress(device.getAddress())->getName() == this->model.getDeviceName())
        QObject::connect(&device, SIGNAL(connectionStateChanged(CasparDevice&)), this, SLOT(deviceConnectionStateChanged(CasparDevice&)));

    checkDeviceConnection();
}

void RundownStillWidget::stopControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeCommand(Playout::PlayoutType::Stop);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::Stop);
    }
}

void RundownStillWidget::playControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeCommand(Playout::PlayoutType::Play);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::Play);
    }
}

void RundownStillWidget::playNowControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeCommand(Playout::PlayoutType::PlayNow);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::PlayNow);
    }
}

void RundownStillWidget::loadControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeCommand(Playout::PlayoutType::Load);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::Load);
    }
}

void RundownStillWidget::pauseControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeCommand(Playout::PlayoutType::PauseResume);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::PauseResume);
    }
}

void RundownStillWidget::nextControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeCommand(Playout::PlayoutType::Next);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::Next);
    }
}

void RundownStillWidget::clearControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeCommand(Playout::PlayoutType::Clear);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::Clear);
    }
}

void RundownStillWidget::clearVideolayerControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeCommand(Playout::PlayoutType::ClearVideoLayer);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::ClearVideoLayer);
    }
}

void RundownStillWidget::clearChannelControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeCommand(Playout::PlayoutType::ClearChannel);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::ClearChannel);
    }
}

void RundownStillWidget::updateProgress()
{
    double elapsed = this->progressElapsed.elapsed() / 1000.0;
    double total = this->progressDurationMs / 1000.0;
    if (elapsed >= total)
    {
        elapsed = total;
        this->progressTimer->stop();
    }

    double fps = RundownWidgetHelper::getChannelFps(this->model.getDeviceName(), this->command.getChannel());
    if (fps <= 0)
        fps = 25.0;

    QString label = this->model.getLabel().isEmpty() ? this->model.getName() : this->model.getLabel();

    EventManager::getInstance().firePlaybackProgressEvent(
        PlaybackProgressEvent(
            this->command.getChannel(),
            this->command.getVideolayer(),
            label,
            "STILL",
            elapsed, total,
            0.0, total,
            fps, false, false));
}
