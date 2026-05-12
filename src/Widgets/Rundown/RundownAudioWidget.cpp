#include "RundownAudioWidget.h"

#include "Global.h"

#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "EventManager.h"
#include "GpiManager.h"
#include "EventManager.h"
#include "Events/ConnectionStateChangedEvent.h"
#include "Utils/ItemScheduler.h"

#include <QtCore/QObject>

#include <QtGui/QPixmap>

#include <QtWidgets/QGraphicsOpacityEffect>

#include "RundownWidgetHelper.h"

RundownAudioWidget::RundownAudioWidget(const LibraryModel& model, QWidget* parent, const QString& color, bool active,
                                       bool loaded, bool paused, bool playing, bool inGroup, bool compactView)
    : QWidget(parent),
      active(active), loaded(loaded), paused(paused), playing(playing), inGroup(inGroup), compactView(compactView), color(color),
      model(model), stopControlSubscription(NULL), playControlSubscription(NULL), playNowControlSubscription(NULL), loadControlSubscription(NULL),
      pauseControlSubscription(NULL), nextControlSubscription(NULL), updateControlSubscription(NULL), clearControlSubscription(NULL),
      clearVideolayerControlSubscription(NULL), clearChannelControlSubscription(NULL)
{
    setupUi(this);

    this->animation = new ActiveAnimation(this->labelActiveColor);

    this->delayType = RundownWidgetHelper::cachedDelayType();
    this->markUsedItems = RundownWidgetHelper::cachedMarkUsedItems();

    setColor(this->color);
    setActive(this->active);
    setCompactView(this->compactView);

    this->command.setAudioName(this->model.getName());

    this->labelLoopOverlay->setVisible(false);

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

    QObject::connect(&this->itemScheduler, SIGNAL(executePlay()), this, SLOT(executePlay()));
    QObject::connect(&this->itemScheduler, SIGNAL(executeStop()), this, SLOT(executeStop()));

    QObject::connect(&this->command, SIGNAL(channelChanged(int)), this, SLOT(channelChanged(int)));
    QObject::connect(&this->command, SIGNAL(videolayerChanged(int)), this, SLOT(videolayerChanged(int)));
    QObject::connect(&this->command, SIGNAL(delayChanged(int)), this, SLOT(delayChanged(int)));
    QObject::connect(&this->command, SIGNAL(durationChanged(int)), this, SLOT(durationChanged(int)));
    QObject::connect(&this->command, SIGNAL(allowGpiChanged(bool)), this, SLOT(allowGpiChanged(bool)));
    QObject::connect(&this->command, SIGNAL(loopChanged(bool)), this, SLOT(loopChanged(bool)));
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
    configureOscSubscriptions();
}

void RundownAudioWidget::labelChanged(const LabelChangedEvent& event)
{
    // This event is not for us.
    if (!this->selected)
        return;

    this->model.setLabel(event.getLabel());

    this->labelLabel->setText(this->model.getLabel().split('/').last());
}

void RundownAudioWidget::targetChanged(const TargetChangedEvent& event)
{
    // This event is not for us.
    if (!this->selected)
        return;

    this->model.setName(event.getTarget());
    this->command.setAudioName(event.getTarget());
}

void RundownAudioWidget::deviceChanged(const DeviceChangedEvent& event)
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

AbstractRundownWidget* RundownAudioWidget::clone()
{
    RundownAudioWidget* widget = new RundownAudioWidget(this->model, this->parentWidget(), this->color, this->active,
                                                        this->loaded, this->paused, this->playing, this->inGroup, this->compactView);

    AudioCommand* command = dynamic_cast<AudioCommand*>(widget->getCommand());
    command->setChannel(this->command.getChannel());
    command->setVideolayer(this->command.getVideolayer());
    command->setDelay(this->command.getDelay());
    command->setDuration(this->command.getDuration());
    command->setAllowGpi(this->command.getAllowGpi());
    command->setAllowRemoteTriggering(this->command.getAllowRemoteTriggering());
    command->setRemoteTriggerId(this->command.getRemoteTriggerId());
    command->setAudioName(this->command.getAudioName());
    command->setTransition(this->command.getTransition());
    command->setTransitionDuration(this->command.getTransitionDuration());
    command->setTween(this->command.getTween());
    command->setDirection(this->command.getDirection());
    command->setLoop(this->command.getLoop());
    command->setUseAuto(this->command.getUseAuto());
    command->setTriggerOnNext(this->command.getTriggerOnNext());

    return widget;
}

void RundownAudioWidget::setCompactView(bool compactView)
{
    if (compactView)
    {
        this->labelColor->setFixedSize(RundownWidgetHelper::BADGE_WIDTH, Rundown::COMPACT_ITEM_HEIGHT);
        this->labelIcon->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
        this->labelGpiConnected->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
        this->labelDisconnected->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
    }
    else
    {
        this->labelColor->setFixedSize(RundownWidgetHelper::BADGE_WIDTH, Rundown::DEFAULT_ITEM_HEIGHT);
        this->labelIcon->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
        this->labelGpiConnected->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
        this->labelDisconnected->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
    }

    this->compactView = compactView;
}

void RundownAudioWidget::readProperties(boost::property_tree::wptree& pt)
{
    if (pt.count(L"color") > 0) setColor(QString::fromStdWString(pt.get<std::wstring>(L"color")));
}

void RundownAudioWidget::writeProperties(QXmlStreamWriter& writer)
{
    writer.writeTextElement("color", this->color);
}

bool RundownAudioWidget::isGroup() const
{
    return false;
}

bool RundownAudioWidget::isInGroup() const
{
    return this->inGroup;
}

AbstractCommand* RundownAudioWidget::getCommand()
{
    return &this->command;
}

LibraryModel* RundownAudioWidget::getLibraryModel()
{
    return &this->model;
}

void RundownAudioWidget::setSelected(bool selected)
{
    this->selected = selected;
}

void RundownAudioWidget::setActive(bool active)
{
    this->active = active;

    this->animation->stop();

    if (this->active)
        RundownWidgetHelper::setActiveColorPalette(this->labelActiveColor, this->command.getChannel());
    else
        RundownWidgetHelper::clearActiveColorPalette(this->labelActiveColor);
}

void RundownAudioWidget::setInGroup(bool inGroup)
{
    this->inGroup = inGroup;
    this->labelGroupColor->setVisible(this->inGroup);
}

QString RundownAudioWidget::getColor() const
{
    return this->color;
}

void RundownAudioWidget::setColor(const QString& color)
{
    this->color = color;
    this->setStyleSheet(QString("#frameItem, #frameStatus { background-color: %1; }").arg(color));
}

void RundownAudioWidget::checkEmptyDevice()
{
    if (this->labelDevice->text() == "Device: ")
        this->labelDevice->setStyleSheet("color: firebrick;");
    else
        this->labelDevice->setStyleSheet("");
}

void RundownAudioWidget::clearDelayedCommands()
{
    this->itemScheduler.cancel();

    this->paused = false;
    this->loaded = false;
    this->playing = false;
}

void RundownAudioWidget::setUsed(bool used)
{
    if (used)
    {
        if (this->frameItem->graphicsEffect() == NULL)
        {
            QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(this);
            effect->setOpacity(0.25);

            this->frameItem->setGraphicsEffect(effect);
        }
    }
    else
        this->frameItem->setGraphicsEffect(NULL);
}

bool RundownAudioWidget::executeCommand(Playout::PlayoutType type)
{
    // Cancel any stale duration/delay timers from a previous playout before
    // executing a new command.  The Play/Update path restarts them via the scheduler.
    if (type != Playout::PlayoutType::Play && type != Playout::PlayoutType::Update)
        this->itemScheduler.cancel();

    if (type == Playout::PlayoutType::Stop)
        executeStop();
    else if ((type == Playout::PlayoutType::Play && !this->command.getTriggerOnNext()) || type == Playout::PlayoutType::Update)
    {
        if (this->command.getDelay() < 0)
            return true;

        if (!this->model.getDeviceName().isEmpty()) // The user need to select a device.
        {
            const QStringList& channelFormats = DatabaseManager::getInstance().getDeviceByName(this->model.getDeviceName()).getChannelFormats().split(",");
            if (this->command.getChannel() > channelFormats.count())
                return true;

            this->itemScheduler.schedulePlayAndStop(
                this->command.getDelay(),
                this->command.getDuration(),
                this->delayType,
                DatabaseManager::getInstance().getFormat(channelFormats[this->command.getChannel() - 1]).getFramesPerSecond().toDouble());
        }
    }
    else if (type == Playout::PlayoutType::PlayNow)
        executePlay();
    else if (type == Playout::PlayoutType::Next && this->command.getTriggerOnNext())
        executePlay();
    else if (type == Playout::PlayoutType::PauseResume)
        executePause();
    else if (type == Playout::PlayoutType::Load)
        executeLoad();
    else if (type == Playout::PlayoutType::Preview)
        executePlayPreview();
    else if (type == Playout::PlayoutType::Clear)
        executeClearVideolayer();
    else if (type == Playout::PlayoutType::ClearVideoLayer)
        executeClearVideolayer();
    else if (type == Playout::PlayoutType::ClearChannel)
        executeClearChannel();

    if (this->active)
    {
        this->animation->setChannel(this->command.getChannel());
        this->animation->start(1);
    }

    return true;
}

void RundownAudioWidget::executeStop()
{
    this->itemScheduler.cancel();

    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device != NULL && device->isConnected())
        device->stop(this->command.getChannel(), this->command.getVideolayer());

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

void RundownAudioWidget::executePlay()
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
        {
            device->play(this->command.getChannel(), this->command.getVideolayer());
        }
        else
        {
            device->playAudio(this->command.getChannel(), this->command.getVideolayer(), this->command.getAudioName(),
                              this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                              this->command.getDirection(), this->command.getLoop(), this->command.getUseAuto());
        }

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
            {
                deviceShadow->play(this->command.getChannel(), this->command.getVideolayer());
            }
            else
            {
                deviceShadow->playAudio(this->command.getChannel(), this->command.getVideolayer(), this->command.getAudioName(),
                                        this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                                        this->command.getDirection(), this->command.getLoop(), this->command.getUseAuto());
            }

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
}

void RundownAudioWidget::executePause()
{
    if (!this->playing)
        return;

    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device != NULL && device->isConnected())
    {
        if (this->paused)
            device->resume(this->command.getChannel(), this->command.getVideolayer());
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
                deviceShadow->resume(this->command.getChannel(), this->command.getVideolayer());
            else
                deviceShadow->pause(this->command.getChannel(), this->command.getVideolayer());
        }
    }

    this->paused = !this->paused;
}

void RundownAudioWidget::executeLoad()
{
    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device != NULL && device->isConnected())
    {
        device->loadAudio(this->command.getChannel(), this->command.getVideolayer(), this->command.getAudioName(),
                          this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                          this->command.getDirection(), this->command.getLoop(), this->command.getUseAuto());
    }

    foreach (const DeviceModel& model, DeviceManager::getInstance().getDeviceModels())
    {
        if (model.getShadow() == "No")
            continue;

        const QSharedPointer<CasparDevice>  deviceShadow = DeviceManager::getInstance().getDeviceByName(model.getName());
        if (deviceShadow != NULL && deviceShadow->isConnected())
        {
            deviceShadow->loadAudio(this->command.getChannel(), this->command.getVideolayer(), this->command.getAudioName(),
                                    this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                                    this->command.getDirection(), this->command.getLoop(), this->command.getUseAuto());
        }
    }

    this->loaded = true;
    this->paused = false;
    this->playing = false;
}

void RundownAudioWidget::executePlayPreview()
{
    const QSharedPointer<DeviceModel> deviceModel = DeviceManager::getInstance().getDeviceModelByName(this->model.getDeviceName());
    if (deviceModel != NULL && deviceModel->getPreviewChannel() > 0)
    {
        const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
        if (device != NULL && device->isConnected())
        {
            device->playAudio(deviceModel->getPreviewChannel(), this->command.getVideolayer(),
                              this->command.getAudioName(), this->command.getTransition(),
                              this->command.getTransitionDuration(), this->command.getTween(),
                              this->command.getDirection(), this->command.getLoop(), false);
        }
    }

    foreach (const DeviceModel& model, DeviceManager::getInstance().getDeviceModels())
    {
        if (model.getShadow() == "No")
            continue;

        if (model.getPreviewChannel() > 0)
        {
            const QSharedPointer<CasparDevice> deviceShadow = DeviceManager::getInstance().getDeviceByName(model.getName());
            if (deviceShadow != NULL && deviceShadow->isConnected())
            {
                deviceShadow->playAudio(model.getPreviewChannel(), this->command.getVideolayer(),
                                        this->command.getAudioName(), this->command.getTransition(),
                                        this->command.getTransitionDuration(), this->command.getTween(),
                                        this->command.getDirection(), this->command.getLoop(), false);
            }
        }
    }
}

void RundownAudioWidget::executeClearVideolayer()
{
    this->itemScheduler.cancel();

    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device != NULL && device->isConnected())
        device->clearVideolayer(this->command.getChannel(), this->command.getVideolayer());

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

void RundownAudioWidget::executeClearChannel()
{
    this->itemScheduler.cancel();

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

void RundownAudioWidget::checkGpiConnection()
{
    this->labelGpiConnected->setVisible(this->command.getAllowGpi());

    if (GpiManager::getInstance().getGpiDevice()->isConnected())
        this->labelGpiConnected->setPixmap(RundownWidgetHelper::gpiConnectedPixmap());
    else
        this->labelGpiConnected->setPixmap(RundownWidgetHelper::gpiDisconnectedPixmap());
}

void RundownAudioWidget::checkDeviceConnection()
{
    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device == NULL)
        this->labelDisconnected->setVisible(true);
    else
        this->labelDisconnected->setVisible(!device->isConnected());
}

void RundownAudioWidget::configureOscSubscriptions()
{
    // File playback OSC subscriptions (always active when device is available).
    if (DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName()) != NULL)
    {
        delete this->timeSubscription;
        this->timeSubscription = nullptr;
        delete this->clipSubscription;
        this->clipSubscription = nullptr;
        delete this->fpsSubscription;
        this->fpsSubscription = nullptr;
        delete this->nameSubscription;
        this->nameSubscription = nullptr;
        delete this->pausedOscSubscription;
        this->pausedOscSubscription = nullptr;
        delete this->loopOscSubscription;
        this->loopOscSubscription = nullptr;

        QString ipAddress = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName())->resolveIpAddress();
        QString channel = QString("%1").arg(this->command.getChannel());
        QString videolayer = QString("%1").arg(this->command.getVideolayer());

        QString timeFilter = Osc::VIDEOLAYER_TIME_FILTER;
        timeFilter.replace("#IPADDRESS#", ipAddress).replace("#CHANNEL#", channel).replace("#VIDEOLAYER#", videolayer);
        this->timeSubscription = new OscSubscription(timeFilter, this);
        QObject::connect(this->timeSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                         this, SLOT(timeSubscriptionReceived(const QString&, const QList<QVariant>&)));

        QString clipFilter = Osc::VIDEOLAYER_CLIP_FILTER;
        clipFilter.replace("#IPADDRESS#", ipAddress).replace("#CHANNEL#", channel).replace("#VIDEOLAYER#", videolayer);
        this->clipSubscription = new OscSubscription(clipFilter, this);
        QObject::connect(this->clipSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                         this, SLOT(clipSubscriptionReceived(const QString&, const QList<QVariant>&)));

        QString fpsFilter = Osc::VIDEOLAYER_FPS_FILTER;
        fpsFilter.replace("#IPADDRESS#", ipAddress).replace("#CHANNEL#", channel).replace("#VIDEOLAYER#", videolayer);
        this->fpsSubscription = new OscSubscription(fpsFilter, this);
        QObject::connect(this->fpsSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                         this, SLOT(fpsSubscriptionReceived(const QString&, const QList<QVariant>&)));

        QString nameFilter = Osc::VIDEOLAYER_NAME_FILTER;
        nameFilter.replace("#IPADDRESS#", ipAddress).replace("#CHANNEL#", channel).replace("#VIDEOLAYER#", videolayer);
        this->nameSubscription = new OscSubscription(nameFilter, this);
        QObject::connect(this->nameSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                         this, SLOT(nameSubscriptionReceived(const QString&, const QList<QVariant>&)));

        QString pausedFilter = Osc::VIDEOLAYER_PAUSED_FILTER;
        pausedFilter.replace("#IPADDRESS#", ipAddress).replace("#CHANNEL#", channel).replace("#VIDEOLAYER#", videolayer);
        this->pausedOscSubscription = new OscSubscription(pausedFilter, this);
        QObject::connect(this->pausedOscSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                         this, SLOT(pausedSubscriptionReceived(const QString&, const QList<QVariant>&)));

        QString loopFilter = Osc::VIDEOLAYER_LOOP_FILTER;
        loopFilter.replace("#IPADDRESS#", ipAddress).replace("#CHANNEL#", channel).replace("#VIDEOLAYER#", videolayer);
        this->loopOscSubscription = new OscSubscription(loopFilter, this);
        QObject::connect(this->loopOscSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                         this, SLOT(loopOscSubscriptionReceived(const QString&, const QList<QVariant>&)));
    }

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

    delete this->updateControlSubscription;
    this->updateControlSubscription = nullptr;

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

    QString updateControlFilter = Osc::ITEM_CONTROL_UPDATE_FILTER;
    updateControlFilter.replace("#UID#", this->command.getRemoteTriggerId());
    this->updateControlSubscription = new OscSubscription(updateControlFilter, this);
    QObject::connect(this->updateControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(updateControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

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

void RundownAudioWidget::channelChanged(int channel)
{
    this->labelChannel->setText(QString("%1").arg(channel));
    RundownWidgetHelper::updateChannelBadge(this->labelColor, channel, this->command.getVideolayer());
    configureOscSubscriptions();
}

void RundownAudioWidget::videolayerChanged(int videolayer)
{
    this->labelVideolayer->setText(QString::fromUtf8("\xe2\xa7\x89 %1").arg(videolayer));
    RundownWidgetHelper::updateChannelBadge(this->labelColor, this->command.getChannel(), videolayer);
    configureOscSubscriptions();
}

void RundownAudioWidget::delayChanged(int delay)
{
    this->labelDelay->setText(RundownWidgetHelper::formatDelay(delay, this->delayType, RundownWidgetHelper::getChannelFps(this->model.getDeviceName(), this->command.getChannel())));
}

void RundownAudioWidget::durationChanged(int duration)
{
    Q_UNUSED(duration);
    updateDurationLabel();
}

void RundownAudioWidget::updateDurationLabel()
{
    double fps = RundownWidgetHelper::getChannelFps(this->model.getDeviceName(), this->command.getChannel());
    this->labelDuration->setText(RundownWidgetHelper::formatDuration(this->command.getDuration(), fps));
}

void RundownAudioWidget::allowGpiChanged(bool allowGpi)
{
    Q_UNUSED(allowGpi);

    checkGpiConnection();
}

void RundownAudioWidget::loopChanged(bool loop)
{
    this->labelLoopOverlay->setVisible(loop);
}

void RundownAudioWidget::gpiConnectionStateChanged(bool connected, GpiDevice* device)
{
    Q_UNUSED(connected);
    Q_UNUSED(device);

    checkGpiConnection();
}

void RundownAudioWidget::remoteTriggerIdChanged(const QString& remoteTriggerId)
{
    configureOscSubscriptions();

    if (remoteTriggerId.trimmed().isEmpty() || !this->command.getAllowRemoteTriggering())
        this->labelRemoteTriggerId->setText("");
    else
        this->labelRemoteTriggerId->setText(QString::fromUtf8("\xe2\x87\xa5 %1").arg(remoteTriggerId));
}

void RundownAudioWidget::deviceConnectionStateChanged(CasparDevice& device)
{
    Q_UNUSED(device);

    checkDeviceConnection();
}

void RundownAudioWidget::deviceAdded(CasparDevice& device)
{
    if (DeviceManager::getInstance().getDeviceModelByAddress(device.getAddress())->getName() == this->model.getDeviceName())
        QObject::connect(&device, SIGNAL(connectionStateChanged(CasparDevice&)), this, SLOT(deviceConnectionStateChanged(CasparDevice&)));

    checkDeviceConnection();
    configureOscSubscriptions();
}

void RundownAudioWidget::stopControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownAudioWidget::playControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownAudioWidget::playNowControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownAudioWidget::loadControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownAudioWidget::pauseControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownAudioWidget::nextControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownAudioWidget::updateControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeCommand(Playout::PlayoutType::Update);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::Update);
    }
}

void RundownAudioWidget::clearControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownAudioWidget::clearVideolayerControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownAudioWidget::clearChannelControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownAudioWidget::timeSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    this->oscTime = arguments.at(0).toDouble();
    this->oscTotalTime = arguments.at(1).toDouble();
    fireProgressEvent();
}

void RundownAudioWidget::clipSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    this->oscClip = arguments.at(0).toDouble();
    this->oscTotalClip = arguments.at(1).toDouble();
    fireProgressEvent();
}

void RundownAudioWidget::fpsSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    this->oscFps = arguments.at(0).toDouble();
    fireProgressEvent();
}

void RundownAudioWidget::nameSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    QString name = arguments.at(0).toString();

    int extIndex = name.lastIndexOf('.');
    if (extIndex != -1)
        name.remove(extIndex, name.length());

    if (this->model.getName().toLower() != name.toLower())
        return;

    this->oscName = arguments.at(0).toString();
    fireProgressEvent();
}

void RundownAudioWidget::pausedSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    this->oscPaused = arguments.at(0).toBool();
}

void RundownAudioWidget::loopOscSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    this->oscLoop = arguments.at(0).toBool();
}

void RundownAudioWidget::fireProgressEvent()
{
    if (this->oscTime > 0 && this->oscTotalTime > 0 &&
        !this->oscName.isEmpty() && this->oscFps > 0)
    {
        QString label = this->model.getLabel().isEmpty() ? this->model.getName() : this->model.getLabel();

        EventManager::getInstance().firePlaybackProgressEvent(
            PlaybackProgressEvent(
                this->command.getChannel(),
                this->command.getVideolayer(),
                label,
                "AUDIO",
                this->oscTime - this->oscClip,
                this->oscTotalTime,
                this->oscClip,
                this->oscTotalClip,
                this->oscFps,
                this->oscPaused, this->oscLoop));

        this->oscName = "";
        this->oscTime = 0;
        this->oscTotalTime = 0;
        this->oscFps = 0;
    }
}
