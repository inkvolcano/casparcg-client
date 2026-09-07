#include "RundownShellCommandWidget.h"

#include "Global.h"

#include "RundownWidgetHelper.h"
#include "DatabaseManager.h"
#include "GpiManager.h"
#include "EventManager.h"
#include "HttpResponseLog.h"
#include "Events/ConnectionStateChangedEvent.h"
#include "Utils/ItemScheduler.h"

#include <QtCore/QDir>
#include <QtCore/QObject>
#include <QtCore/QProcess>

#include <QtWidgets/QGraphicsOpacityEffect>

RundownShellCommandWidget::RundownShellCommandWidget(const LibraryModel& model, QWidget* parent, const QString& color, bool active,
                                                     bool inGroup, bool compactView)
    : QWidget(parent),
      active(active), inGroup(inGroup), compactView(compactView), color(color), model(model), stopControlSubscription(NULL),
      playControlSubscription(NULL), playNowControlSubscription(NULL), updateControlSubscription(NULL), clearControlSubscription(NULL),
      clearVideolayerControlSubscription(NULL), clearChannelControlSubscription(NULL)
{
    setupUi(this);

    this->animation = new ActiveAnimation(this->labelActiveColor);

    this->markUsedItems = RundownWidgetHelper::cachedMarkUsedItems();

    setColor(color);
    setActive(active);
    setCompactView(compactView);

    this->labelDisconnected->setVisible(false);

    this->labelGroupColor->setVisible(this->inGroup);
    this->labelGroupColor->setStyleSheet(QString("background-color: %1;").arg(Color::DEFAULT_GROUP_COLOR));

    this->labelLabel->setText(this->model.getLabel());
    this->labelDelay->setText(RundownWidgetHelper::formatDelay(this->command.getDelay(), Output::DEFAULT_DELAY_IN_MILLISECONDS));
    RundownWidgetHelper::setupChannelBadge(this->frameItem, this->labelColor, this->command.getChannel(), this->command.getVideolayer());
    QLabel* bankBadge = RundownWidgetHelper::createBankBadge(this->frameItem);
    QObject::connect(&this->command, &AbstractCommand::triggerBankChanged, [this, bankBadge](int bank) {
        RundownWidgetHelper::updateBankBadge(bankBadge, bank);
        RundownWidgetHelper::configureBankOscSubscriptions(this, this, bank);
    });
    RundownWidgetHelper::updateBankBadge(bankBadge, this->command.getTriggerBank());
    RundownWidgetHelper::configureBankOscSubscriptions(this, this, this->command.getTriggerBank());
    RundownWidgetHelper::setupCloneSupport(this, this->frameItem, &this->command);

    QObject::connect(&this->itemScheduler, SIGNAL(executePlay()), this, SLOT(executePlay()));
    QObject::connect(&this->itemScheduler, SIGNAL(executeStop()), this, SLOT(executeStop()));

    QObject::connect(&this->command, SIGNAL(delayChanged(int)), this, SLOT(delayChanged(int)));
    QObject::connect(&this->command, SIGNAL(commandLineChanged(const QString&)), this, SLOT(commandLineChanged(const QString&)));
    QObject::connect(&this->command, SIGNAL(allowGpiChanged(bool)), this, SLOT(allowGpiChanged(bool)));
    QObject::connect(&this->command, &AbstractCommand::disabledChanged, this, [this](bool d) { setRundownDisabled(d); });
    QObject::connect(&this->command, SIGNAL(remoteTriggerIdChanged(const QString&)), this, SLOT(remoteTriggerIdChanged(const QString&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(labelChanged(const LabelChangedEvent&)), this, SLOT(labelChanged(const LabelChangedEvent&)));

    QObject::connect(GpiManager::getInstance().getGpiDevice().data(), SIGNAL(connectionStateChanged(bool, GpiDevice*)), this, SLOT(gpiConnectionStateChanged(bool, GpiDevice*)));

    checkGpiConnection();
    updateCommandLabel();
}

bool RundownShellCommandWidget::shellCommandsAllowed()
{
    return DatabaseManager::getInstance().getConfigurationByName("AllowShellCommands").getValue() == "true";
}

void RundownShellCommandWidget::updateCommandLabel()
{
    // The row shows the command, because "Shell Command" on its own tells an
    // operator nothing about what pressing play here will do. An item that cannot
    // run says so on the row rather than only when it is fired.
    QString text = this->command.getCommandLine().trimmed();

    if (text.isEmpty())
        text = this->model.getLabel();

    if (!shellCommandsAllowed())
        text = QString::fromUtf8("\xf0\x9f\x9a\xab ") + text;

    this->labelLabel->setText(text);
    this->labelLabel->setToolTip(shellCommandsAllowed()
        ? this->command.getCommandLine()
        : QString("Shell commands are turned off. Settings \xe2\x86\x92 General \xe2\x86\x92 Rundown to allow them.\n\n%1")
              .arg(this->command.getCommandLine()));
}

void RundownShellCommandWidget::commandLineChanged(const QString& commandLine)
{
    Q_UNUSED(commandLine);

    updateCommandLabel();
}

void RundownShellCommandWidget::labelChanged(const LabelChangedEvent& event)
{
    // This event is not for us.
    if (!this->selected)
        return;

    this->model.setLabel(event.getLabel());

    updateCommandLabel();
}

AbstractRundownWidget* RundownShellCommandWidget::clone()
{
    RundownShellCommandWidget* widget = new RundownShellCommandWidget(this->model, this->parentWidget(), this->color, this->active,
                                                                      this->inGroup, this->compactView);

    ShellCommand* command = dynamic_cast<ShellCommand*>(widget->getCommand());
    command->setChannel(this->command.getChannel());
    command->setVideolayer(this->command.getVideolayer());
    command->setDelay(this->command.getDelay());
    command->setDuration(this->command.getDuration());
    command->setAllowGpi(this->command.getAllowGpi());
    command->setAllowRemoteTriggering(this->command.getAllowRemoteTriggering());
    command->setRemoteTriggerId(this->command.getRemoteTriggerId());
    command->setCommandLine(this->command.getCommandLine());
    command->setWorkingDirectory(this->command.getWorkingDirectory());
    command->setTriggerOnNext(this->command.getTriggerOnNext());
    command->setWaitForFinish(this->command.getWaitForFinish());
    command->setTimeout(this->command.getTimeout());

    return widget;
}

void RundownShellCommandWidget::setCompactView(bool compactView)
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

void RundownShellCommandWidget::readProperties(boost::property_tree::wptree& pt)
{
    if (pt.count(L"color") > 0) setColor(QString::fromStdWString(pt.get<std::wstring>(L"color")));

    updateCommandLabel();
}

void RundownShellCommandWidget::writeProperties(QXmlStreamWriter& writer)
{
    writer.writeTextElement("color", this->color);
}

bool RundownShellCommandWidget::isGroup() const
{
    return false;
}

bool RundownShellCommandWidget::isInGroup() const
{
    return this->inGroup;
}

AbstractCommand* RundownShellCommandWidget::getCommand()
{
    return &this->command;
}

LibraryModel* RundownShellCommandWidget::getLibraryModel()
{
    return &this->model;
}

void RundownShellCommandWidget::setSelected(bool selected)
{
    this->selected = selected;
}

void RundownShellCommandWidget::setActive(bool active)
{
    this->active = active;

    this->animation->stop();

    if (this->active)
        RundownWidgetHelper::setActiveColorPalette(this->labelActiveColor, this->command.getChannel());
    else
        RundownWidgetHelper::clearActiveColorPalette(this->labelActiveColor);
}

void RundownShellCommandWidget::setInGroup(bool inGroup)
{
    this->inGroup = inGroup;
    this->labelGroupColor->setVisible(this->inGroup);
}

QString RundownShellCommandWidget::getColor() const
{
    return this->color;
}

void RundownShellCommandWidget::setColor(const QString& color)
{
    this->color = color;

    // An item that runs a program should not look like every other row, so an
    // untouched one gets the slate. Picking any colour from the colour menu still
    // wins, because that arrives here as an explicit value.
    QString applied = color;
    if (applied.isEmpty() || applied == Color::DEFAULT_TRANSPARENT_COLOR)
        applied = Color::DEFAULT_SHELLCOMMAND_COLOR;

    this->setStyleSheet(QString("#frameItem, #frameStatus { background-color: %1; }").arg(applied));
}

void RundownShellCommandWidget::clearDelayedCommands()
{
    this->itemScheduler.cancel();
}

void RundownShellCommandWidget::setUsed(bool used)
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

bool RundownShellCommandWidget::executeCommand(Playout::PlayoutType type)
{
    if (this->command.getDisabled()) return true;
    if (type == Playout::PlayoutType::Stop)
        executeStop();
    else if ((type == Playout::PlayoutType::Play && !this->command.getTriggerOnNext()) || type == Playout::PlayoutType::Update)
    {
        if (this->command.getDelay() < 0)
            return true;

        if (!this->command.getCommandLine().trimmed().isEmpty())
        {
            this->itemScheduler.schedulePlayAndStop(this->command.getDelay(), 0, Output::DEFAULT_DELAY_IN_MILLISECONDS);
        }
    }
    else if (type == Playout::PlayoutType::PlayNow)
        executePlay();
    else if (type == Playout::PlayoutType::Next && this->command.getTriggerOnNext())
        executePlay();
    else if (type == Playout::PlayoutType::Clear)
        executeStop();
    else if (type == Playout::PlayoutType::ClearVideoLayer)
        executeStop();
    else if (type == Playout::PlayoutType::ClearChannel)
        executeStop();

    if (this->active)
    {
        this->animation->setChannel(this->command.getChannel());
        this->animation->start(1);
    }

    return true;
}

void RundownShellCommandWidget::executeStop()
{
    this->itemScheduler.cancel();

    // Stop on an item whose program is still going kills it. Anything that ran to
    // completion already cleared itself, so there is usually nothing here.
    if (this->process != nullptr && this->process->state() != QProcess::NotRunning)
    {
        this->process->kill();

        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent(QString("Shell: killed \"%1\"").arg(this->command.getCommandLine())));
    }
}

void RundownShellCommandWidget::executePlay()
{
    QString commandLine = this->command.getCommandLine().trimmed();
    if (commandLine.isEmpty())
        return;

    // The gate. Refusing here rather than at edit time means a rundown can be
    // built and moved between machines, and only the machine that has been told
    // to allow it will run anything.
    if (!shellCommandsAllowed())
    {
        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent(QString("Shell commands are turned off \xe2\x80\x94 refused \"%1\"").arg(commandLine)));

        HttpResponseLog::getInstance().logResponse("SHELL", commandLine, 0,
            "Refused: shell commands are turned off for this client. "
            "Settings > General > Rundown > Allow Shell Command items to run.");
        return;
    }

    // QProcess::splitCommand honours quoting, so a path with a space in it works
    // when it is quoted the way it would be in a terminal.
    QStringList parts = QProcess::splitCommand(commandLine);
    if (parts.isEmpty())
        return;

    QString program = parts.takeFirst();

    QProcess* process = new QProcess(this);

    QString workingDirectory = this->command.getWorkingDirectory().trimmed();
    if (!workingDirectory.isEmpty())
        process->setWorkingDirectory(workingDirectory);

    // stderr is merged in, because a program that failed usually says why there
    // and splitting the two would hide half the answer in the log.
    process->setProcessChannelMode(QProcess::MergedChannels);

    this->process = process;

    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                     [this, process, commandLine](int exitCode, QProcess::ExitStatus status) {
        QString output = QString::fromLocal8Bit(process->readAll()).trimmed();

        if (status == QProcess::CrashExit)
            output = output.isEmpty() ? QString("The program was killed or crashed.")
                                      : QString("%1\n(The program was killed or crashed.)").arg(output);

        HttpResponseLog::getInstance().logResponse("SHELL", commandLine, exitCode, output);

        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent(QString("Shell: \"%1\" exited %2").arg(commandLine).arg(exitCode)));

        if (this->process == process)
            this->process = nullptr;

        process->deleteLater();
    });

    QObject::connect(process, &QProcess::errorOccurred, this,
                     [this, process, commandLine](QProcess::ProcessError error) {
        // A program that could not be started never emits finished(), so this is
        // the only place a typo'd path is ever reported.
        if (error != QProcess::FailedToStart)
            return;

        QString message = QString("Could not start: %1").arg(process->errorString());

        HttpResponseLog::getInstance().logResponse("SHELL", commandLine, -1, message);

        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent(QString("Shell: %1").arg(message)));

        if (this->process == process)
            this->process = nullptr;

        process->deleteLater();
    });

    process->start(program, parts);

    if (this->command.getWaitForFinish())
    {
        // Blocking the UI is the point here: the operator asked for the next item
        // not to go until this one is done. The timeout is what stops that
        // becoming a hung client, and a program still running past it is killed
        // rather than left behind.
        if (!process->waitForFinished(this->command.getTimeout() * 1000))
        {
            process->kill();
            process->waitForFinished(2000);

            EventManager::getInstance().fireStatusbarEvent(
                StatusbarEvent(QString("Shell: \"%1\" timed out after %2s and was killed")
                    .arg(commandLine).arg(this->command.getTimeout())));
        }
    }

    if (this->markUsedItems)
        setUsed(true);
}

void RundownShellCommandWidget::delayChanged(int delay)
{
    this->labelDelay->setText(RundownWidgetHelper::formatDelay(delay, Output::DEFAULT_DELAY_IN_MILLISECONDS));
}

void RundownShellCommandWidget::checkGpiConnection()
{
    this->labelGpiConnected->setVisible(this->command.getAllowGpi());

    if (GpiManager::getInstance().getGpiDevice()->isConnected())
        this->labelGpiConnected->setPixmap(RundownWidgetHelper::gpiConnectedPixmap());
    else
        this->labelGpiConnected->setPixmap(RundownWidgetHelper::gpiDisconnectedPixmap());
}

void RundownShellCommandWidget::configureOscSubscriptions()
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

void RundownShellCommandWidget::allowGpiChanged(bool allowGpi)
{
    Q_UNUSED(allowGpi);

    checkGpiConnection();
}

void RundownShellCommandWidget::gpiConnectionStateChanged(bool connected, GpiDevice* device)
{
    Q_UNUSED(connected);
    Q_UNUSED(device);

    checkGpiConnection();
}

void RundownShellCommandWidget::remoteTriggerIdChanged(const QString& remoteTriggerId)
{
    configureOscSubscriptions();

    if (remoteTriggerId.trimmed().isEmpty() || !this->command.getAllowRemoteTriggering())
        this->labelRemoteTriggerId->setText("");
    else
        this->labelRemoteTriggerId->setText(QString::fromUtf8("\xe2\x87\xa5 %1").arg(remoteTriggerId));
}

void RundownShellCommandWidget::stopControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownShellCommandWidget::playControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownShellCommandWidget::playNowControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownShellCommandWidget::updateControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownShellCommandWidget::clearControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownShellCommandWidget::clearVideolayerControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownShellCommandWidget::clearChannelControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
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

void RundownShellCommandWidget::setRundownDisabled(bool disabled)
{
    RundownWidgetHelper::applyDisabledStyle(this, this->labelLabel, disabled);
}
