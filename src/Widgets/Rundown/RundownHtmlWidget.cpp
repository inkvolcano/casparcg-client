#include "RundownHtmlWidget.h"

#include "Global.h"

#include "RundownWidgetHelper.h"
#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "GpiManager.h"
#include "EventManager.h"
#include "Events/ConnectionStateChangedEvent.h"
#include "Utils/ItemScheduler.h"

#include <QtCore/QObject>

#include <QtWidgets/QGraphicsOpacityEffect>

RundownHtmlWidget::RundownHtmlWidget(const LibraryModel& model, QWidget* parent, const QString& color, bool active,
                                     bool loaded, bool paused, bool playing, bool inGroup, bool compactView)
    : QWidget(parent),
      active(active), loaded(loaded), paused(paused), playing(playing), inGroup(inGroup), compactView(compactView), color(color), model(model),
      stopControlSubscription(NULL), playControlSubscription(NULL), playNowControlSubscription(NULL), updateControlSubscription(NULL),
      previewControlSubscription(NULL), clearControlSubscription(NULL), clearVideolayerControlSubscription(NULL), clearChannelControlSubscription(NULL)
{
    setupUi(this);

    this->animation = new ActiveAnimation(this->labelActiveColor);

    this->delayType = RundownWidgetHelper::cachedDelayType();
    this->markUsedItems = RundownWidgetHelper::cachedMarkUsedItems();

    setColor(this->color);
    setActive(this->active);
    setCompactView(this->compactView);

    this->labelGroupColor->setVisible(this->inGroup);
    this->labelGroupColor->setStyleSheet(QString("background-color: %1;").arg(Color::DEFAULT_GROUP_COLOR));

    this->labelLabel->setText(this->model.getLabel());
    this->labelChannel->setText(QString("%1").arg(this->command.getChannel()));
    this->labelVideolayer->setText(QString::fromUtf8("\xe2\xa7\x89 %1").arg(this->command.getVideolayer()));
    this->labelDelay->setText(RundownWidgetHelper::formatDelay(this->command.getDelay(), this->delayType, RundownWidgetHelper::getChannelFps(this->model.getDeviceName(), this->command.getChannel())));
    this->labelDevice->setText(QString("%1").arg(this->model.getDeviceName()));
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

    QObject::connect(&this->itemScheduler, SIGNAL(executePlay()), this, SLOT(executePlay()));
    QObject::connect(&this->itemScheduler, SIGNAL(executeStop()), this, SLOT(executeStop()));

    QObject::connect(&this->command, SIGNAL(channelChanged(int)), this, SLOT(channelChanged(int)));
    QObject::connect(&this->command, SIGNAL(videolayerChanged(int)), this, SLOT(videolayerChanged(int)));
    QObject::connect(&this->command, SIGNAL(delayChanged(int)), this, SLOT(delayChanged(int)));
    QObject::connect(&this->command, SIGNAL(allowGpiChanged(bool)), this, SLOT(allowGpiChanged(bool)));
    QObject::connect(&this->command, &AbstractCommand::disabledChanged, this, [this](bool d) { setRundownDisabled(d); });
    QObject::connect(&this->command, SIGNAL(remoteTriggerIdChanged(const QString&)), this, SLOT(remoteTriggerIdChanged(const QString&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(deviceChanged(const DeviceChangedEvent&)), this, SLOT(deviceChanged(const DeviceChangedEvent&)));
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

void RundownHtmlWidget::labelChanged(const LabelChangedEvent& event)
{
    // This event is not for us.
    if (!this->selected)
        return;

    this->model.setLabel(event.getLabel());

    this->labelLabel->setText(this->model.getLabel());
}

void RundownHtmlWidget::deviceChanged(const DeviceChangedEvent& event)
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

AbstractRundownWidget* RundownHtmlWidget::clone()
{
    RundownHtmlWidget* widget = new RundownHtmlWidget(this->model, this->parentWidget(), this->color, this->active,
                                                      this->loaded, this->paused, this->playing, this->inGroup, this->compactView);

    HtmlCommand* command = dynamic_cast<HtmlCommand*>(widget->getCommand());
    command->setChannel(this->command.getChannel());
    command->setVideolayer(this->command.getVideolayer());
    command->setDelay(this->command.getDelay());
    command->setDuration(this->command.getDuration());
    command->setAllowGpi(this->command.getAllowGpi());
    command->setAllowRemoteTriggering(this->command.getAllowRemoteTriggering());
    command->setRemoteTriggerId(this->command.getRemoteTriggerId());
    command->setTransition(this->command.getTransition());
    command->setUrl(this->command.getUrl());
    command->setTransitionDuration(this->command.getTransitionDuration());
    command->setTween(this->command.getTween());
    command->setDirection(this->command.getDirection());
    command->setTriggerOnNext(this->command.getTriggerOnNext());
    command->setFreezeOnLoad(this->command.getFreezeOnLoad());
    command->setUseAuto(this->command.getUseAuto());

    return widget;
}

void RundownHtmlWidget::setCompactView(bool compactView)
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

void RundownHtmlWidget::readProperties(boost::property_tree::wptree& pt)
{
    if (pt.count(L"color") > 0) setColor(QString::fromStdWString(pt.get<std::wstring>(L"color")));
}

void RundownHtmlWidget::writeProperties(QXmlStreamWriter& writer)
{
    writer.writeTextElement("color", this->color);
}

bool RundownHtmlWidget::isGroup() const
{
    return false;
}

bool RundownHtmlWidget::isInGroup() const
{
    return this->inGroup;
}

AbstractCommand* RundownHtmlWidget::getCommand()
{
    return &this->command;
}

LibraryModel* RundownHtmlWidget::getLibraryModel()
{
    return &this->model;
}

void RundownHtmlWidget::setSelected(bool selected)
{
    this->selected = selected;
}

void RundownHtmlWidget::setActive(bool active)
{
    this->active = active;

    this->animation->stop();

    if (this->active)
        RundownWidgetHelper::setActiveColorPalette(this->labelActiveColor, this->command.getChannel());
    else
        RundownWidgetHelper::clearActiveColorPalette(this->labelActiveColor);
}

void RundownHtmlWidget::setInGroup(bool inGroup)
{
    this->inGroup = inGroup;
    this->labelGroupColor->setVisible(this->inGroup);
}

QString RundownHtmlWidget::getColor() const
{
    return this->color;
}

void RundownHtmlWidget::setColor(const QString& color)
{
    this->color = color;
    this->setStyleSheet(QString("#frameItem, #frameStatus { background-color: %1; }").arg(color));
}

void RundownHtmlWidget::checkEmptyDevice()
{
    if (this->labelDevice->text() == "Device: ")
        this->labelDevice->setStyleSheet("color: firebrick;");
    else
        this->labelDevice->setStyleSheet("");
}

void RundownHtmlWidget::clearDelayedCommands()
{
    this->itemScheduler.cancel();

    this->paused = false;
    this->loaded = false;
    this->playing = false;
}

void RundownHtmlWidget::setUsed(bool used)
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

bool RundownHtmlWidget::executeCommand(Playout::PlayoutType type)
{
    if (this->command.getDisabled()) return true;
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
    else if (type == Playout::PlayoutType::Clear)
        executeStop();
    else if (type == Playout::PlayoutType::ClearVideoLayer)
        executeStop();
    else if (type == Playout::PlayoutType::ClearChannel)
        executeStop();
    else if (type == Playout::PlayoutType::Preview)
        executeLoadPreview();

    if (this->active)
    {
        this->animation->setChannel(this->command.getChannel());
        this->animation->start(1);
    }

    return true;
}

void RundownHtmlWidget::executeStop()
{
    this->itemScheduler.cancel();

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

void RundownHtmlWidget::executePlay()
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
            device->playHtml(this->command.getChannel(), this->command.getVideolayer(), this->command.getUrl(),
                             this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                             this->command.getDirection(), this->command.getUseAuto());
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
                deviceShadow->playHtml(this->command.getChannel(), this->command.getVideolayer(), this->command.getUrl(),
                                       this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                                       this->command.getDirection(), this->command.getUseAuto());
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

void RundownHtmlWidget::executePause()
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

void RundownHtmlWidget::executeLoad()
{
    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device != NULL && device->isConnected())
    {
        device->loadHtml(this->command.getChannel(), this->command.getVideolayer(), this->command.getUrl(),
                            this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                            this->command.getDirection(), this->command.getFreezeOnLoad(), this->command.getUseAuto());
    }

    foreach (const DeviceModel& model, DeviceManager::getInstance().getDeviceModels())
    {
        if (model.getShadow() == "No")
            continue;

        const QSharedPointer<CasparDevice>  deviceShadow = DeviceManager::getInstance().getDeviceByName(model.getName());
        if (deviceShadow != NULL && deviceShadow->isConnected())
        {
            deviceShadow->loadHtml(this->command.getChannel(), this->command.getVideolayer(), this->command.getUrl(),
                                      this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                                      this->command.getDirection(), this->command.getFreezeOnLoad(), this->command.getUseAuto());
        }
    }

    this->loaded = true;
    this->paused = false;
    this->playing = false;
}

void RundownHtmlWidget::executeLoadPreview()
{
    const QSharedPointer<DeviceModel> deviceModel = DeviceManager::getInstance().getDeviceModelByName(this->model.getDeviceName());
    if (deviceModel != NULL && deviceModel->getPreviewChannel() > 0)
    {
        const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
        if (device != NULL && device->isConnected())
        {
            device->loadHtml(deviceModel->getPreviewChannel(), this->command.getVideolayer(), this->command.getUrl(),
                             this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                             this->command.getDirection(), true, false);
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
                deviceShadow->loadHtml(model.getPreviewChannel(), this->command.getVideolayer(), this->command.getUrl(),
                                       this->command.getTransition(), this->command.getTransitionDuration(), this->command.getTween(),
                                       this->command.getDirection(), true, false);
            }
        }
    }
}

void RundownHtmlWidget::executeClearVideolayer()
{
    this->itemScheduler.cancel();

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

void RundownHtmlWidget::executeClearChannel()
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

void RundownHtmlWidget::videolayerChanged(int videolayer)
{
    this->labelVideolayer->setText(QString::fromUtf8("\xe2\xa7\x89 %1").arg(videolayer));
    RundownWidgetHelper::updateChannelBadge(this->labelColor, this->command.getBaseChannel(), videolayer);
}

void RundownHtmlWidget::channelChanged(int channel)
{
    this->labelChannel->setText(QString("%1").arg(channel));
    RundownWidgetHelper::updateChannelBadge(this->labelColor, channel, this->command.getVideolayer());
}

void RundownHtmlWidget::delayChanged(int delay)
{
    this->labelDelay->setText(RundownWidgetHelper::formatDelay(delay, this->delayType, RundownWidgetHelper::getChannelFps(this->model.getDeviceName(), this->command.getChannel())));
}

void RundownHtmlWidget::checkGpiConnection()
{
    this->labelGpiConnected->setVisible(this->command.getAllowGpi());

    if (GpiManager::getInstance().getGpiDevice()->isConnected())
        this->labelGpiConnected->setPixmap(RundownWidgetHelper::gpiConnectedPixmap());
    else
        this->labelGpiConnected->setPixmap(RundownWidgetHelper::gpiDisconnectedPixmap());
}

void RundownHtmlWidget::checkDeviceConnection()
{
    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->model.getDeviceName());
    if (device == NULL)
        this->labelDisconnected->setVisible(true);
    else
        this->labelDisconnected->setVisible(!device->isConnected());
}

void RundownHtmlWidget::configureOscSubscriptions()
{
    if (!this->command.getAllowRemoteTriggering())
        return;

    delete this->stopControlSubscription;
    this->stopControlSubscription = nullptr;

    delete this->playControlSubscription;
    this->playControlSubscription = nullptr;

    delete this->playNowControlSubscription;
    this->playNowControlSubscription = nullptr;

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

    QString updateControlFilter = Osc::ITEM_CONTROL_UPDATE_FILTER;
    updateControlFilter.replace("#UID#", this->command.getRemoteTriggerId());
    this->updateControlSubscription = new OscSubscription(updateControlFilter, this);
    QObject::connect(this->updateControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(updateControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString previewControlFilter = Osc::ITEM_CONTROL_PREVIEW_FILTER;
    previewControlFilter.replace("#UID#", this->command.getRemoteTriggerId());
    this->previewControlSubscription = new OscSubscription(previewControlFilter, this);
    QObject::connect(this->previewControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(previewControlSubscriptionReceived(const QString&, const QList<QVariant>&)));


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

void RundownHtmlWidget::allowGpiChanged(bool allowGpi)
{
    Q_UNUSED(allowGpi);

    checkGpiConnection();
}

void RundownHtmlWidget::gpiConnectionStateChanged(bool connected, GpiDevice* device)
{
    Q_UNUSED(connected);
    Q_UNUSED(device);

    checkGpiConnection();
}

void RundownHtmlWidget::remoteTriggerIdChanged(const QString& remoteTriggerId)
{
    configureOscSubscriptions();

    if (remoteTriggerId.trimmed().isEmpty() || !this->command.getAllowRemoteTriggering())
        this->labelRemoteTriggerId->setText("");
    else
        this->labelRemoteTriggerId->setText(QString::fromUtf8("\xe2\x87\xa5 %1").arg(remoteTriggerId));
}

void RundownHtmlWidget::deviceConnectionStateChanged(CasparDevice& device)
{
    Q_UNUSED(device);

    checkDeviceConnection();
}

void RundownHtmlWidget::deviceAdded(CasparDevice& device)
{
    if (DeviceManager::getInstance().getDeviceModelByAddress(device.getAddress())->getName() == this->model.getDeviceName())
        QObject::connect(&device, SIGNAL(connectionStateChanged(CasparDevice&)), this, SLOT(deviceConnectionStateChanged(CasparDevice&)));

    checkDeviceConnection();
}

void RundownHtmlWidget::stopControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownHtmlWidget::playControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownHtmlWidget::playNowControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownHtmlWidget::loadControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownHtmlWidget::pauseControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownHtmlWidget::updateControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownHtmlWidget::previewControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeCommand(Playout::PlayoutType::Preview);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::Preview);
    }
}

void RundownHtmlWidget::clearControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownHtmlWidget::clearVideolayerControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownHtmlWidget::clearChannelControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownHtmlWidget::setRundownDisabled(bool disabled)
{
    RundownWidgetHelper::applyDisabledStyle(this, this->labelLabel, disabled);
}
