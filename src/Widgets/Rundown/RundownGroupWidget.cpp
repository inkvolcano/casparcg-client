#include "RundownGroupWidget.h"
#include "RundownWidgetHelper.h"
#include "AbstractRundownWidget.h"

#include "Global.h"
#include "GpiManager.h"
#include "EventManager.h"
#include "DatabaseManager.h"
#include "Timecode.h"

#include "Events/DurationChangedEvent.h"
#include "Events/Inspector/LabelChangedEvent.h"

#include <QtCore/QDebug>
#include <QtCore/QObject>
#include <QtCore/QSet>

#include <QtWidgets/QApplication>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QGraphicsOpacityEffect>

RundownGroupWidget::RundownGroupWidget(const LibraryModel& model, QWidget* parent, const QString& color, bool active, bool compactView)
    : QWidget(parent),
      active(active), compactView(compactView), color(color), model(model), stopControlSubscription(NULL),
      playControlSubscription(NULL), loadControlSubscription(NULL), pauseControlSubscription(NULL), nextControlSubscription(NULL),
      updateControlSubscription(NULL), invokeControlSubscription(NULL), clearControlSubscription(NULL), clearVideolayerControlSubscription(NULL),
      clearChannelControlSubscription(NULL)
{
    setupUi(this);

    this->animation = new ActiveAnimation(this->labelActiveColor);

    this->markUsedItems = RundownWidgetHelper::cachedMarkUsedItems();
    this->useDropFrameNotation = (DatabaseManager::getInstance().getConfigurationByName("UseDropFrameNotation").getValue() == "true") ? true : false;

    setColor(this->color);
    setActive(this->active);
    setCompactView(this->compactView);

    this->labelAutoPlay->setVisible(false);
    this->labelLoop->setVisible(false);

    this->labelGroupColor->setStyleSheet(QString("background-color: %1;").arg(Color::DEFAULT_GROUP_COLOR));

    // Set up channel badge on labelColor.
    RundownWidgetHelper::setupChannelBadge(this->frameItem, this->labelColor, this->command.getChannel(), this->command.getVideolayer());

    // Create type list label AFTER setupChannelBadge to avoid capture by setupBottomRow().
    // Apply badge shift manually since the label wasn't present during the shift loop.
    this->labelTypeList = new QLabel(this->frameItem);
    this->labelTypeList->setGeometry(80 + RundownWidgetHelper::BADGE_SHIFT, 19, 130, 16);
    this->labelTypeList->setStyleSheet("color: rgba(200, 200, 200, 180); font-size: 11px;");
    this->labelTypeList->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    this->labelTypeList->setText("");
    this->labelTypeList->raise();
    this->labelColor->setText("-");

    QLabel* bankBadge = RundownWidgetHelper::createBankBadge(this->frameItem);
    QObject::connect(&this->command, &AbstractCommand::triggerBankChanged, [this, bankBadge](int bank) {
        RundownWidgetHelper::updateBankBadge(bankBadge, bank);
        RundownWidgetHelper::configureBankOscSubscriptions(this, this, bank);
    });
    RundownWidgetHelper::updateBankBadge(bankBadge, this->command.getTriggerBank());
    RundownWidgetHelper::configureBankOscSubscriptions(this, this, this->command.getTriggerBank());
    RundownWidgetHelper::setupCloneSupport(this, this->frameItem, &this->command);

    this->labelLabel->setText(this->model.getLabel());

    QObject::connect(&this->command, SIGNAL(durationChanged(int)), this, SLOT(durationChanged(int)));
    QObject::connect(&this->command, SIGNAL(notesChanged(const QString&)), this, SLOT(notesChanged(const QString&)));
    QObject::connect(&this->command, SIGNAL(allowGpiChanged(bool)), this, SLOT(allowGpiChanged(bool)));
    QObject::connect(&this->command, SIGNAL(autoPlayChanged(bool)), this, SLOT(autoPlayChanged(bool)));
    QObject::connect(&this->command, SIGNAL(loopChanged(bool)), this, SLOT(loopChanged(bool)));
    QObject::connect(&this->command, SIGNAL(allowRemoteTriggeringChanged(bool)), this, SLOT(configureOscSubscriptions()));
    QObject::connect(&this->command, SIGNAL(remoteTriggerIdChanged(const QString&)), this, SLOT(remoteTriggerIdChanged(const QString&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(labelChanged(const LabelChangedEvent&)), this, SLOT(labelChanged(const LabelChangedEvent&)));

    QObject::connect(GpiManager::getInstance().getGpiDevice().data(), SIGNAL(connectionStateChanged(bool, GpiDevice*)), this, SLOT(gpiConnectionStateChanged(bool, GpiDevice*)));

    checkGpiConnection();
}

void RundownGroupWidget::labelChanged(const LabelChangedEvent& event)
{
    // This event is not for us.
    if (!this->selected)
        return;

    this->model.setLabel(event.getLabel());

    this->labelLabel->setText(this->model.getLabel());
}

AbstractRundownWidget* RundownGroupWidget::clone()
{
    RundownGroupWidget* widget = new RundownGroupWidget(this->model, this->parentWidget(), this->color,
                                                        this->active, this->compactView);

    GroupCommand* command = dynamic_cast<GroupCommand*>(widget->getCommand());
    command->setChannel(this->command.getChannel());
    command->setVideolayer(this->command.getVideolayer());
    command->setDelay(this->command.getDelay());
    command->setDuration(this->command.getDuration());
    command->setAllowGpi(this->command.getAllowGpi());
    command->setAllowRemoteTriggering(this->command.getAllowRemoteTriggering());
    command->setRemoteTriggerId(this->command.getRemoteTriggerId());
    command->setNotes(this->command.getNotes());
    command->setAutoPlay(this->command.getAutoPlay());
    command->setLoop(this->command.getLoop());
    command->setTriggerBank(this->command.getTriggerBank());

    return widget;
}

void RundownGroupWidget::readProperties(boost::property_tree::wptree& pt)
{
    if (pt.count(L"color") > 0) setColor(QString::fromStdWString(pt.get<std::wstring>(L"color")));
}

void RundownGroupWidget::writeProperties(QXmlStreamWriter& writer)
{
    writer.writeTextElement("color", this->color);
}

void RundownGroupWidget::setCompactView(bool compactView)
{
    if (compactView)
    {
        this->labelColor->setFixedSize(RundownWidgetHelper::BADGE_WIDTH, Rundown::COMPACT_ITEM_HEIGHT);
        this->labelGroupColor->move(this->labelGroupColor->x(), Rundown::COMPACT_ITEM_HEIGHT - 1);
        this->labelIcon->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
        this->labelGpiConnected->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
        this->labelAutoPlay->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
        this->labelLoop->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
    }
    else
    {
        this->labelColor->setFixedSize(RundownWidgetHelper::BADGE_WIDTH, Rundown::DEFAULT_ITEM_HEIGHT);
        this->labelGroupColor->move(this->labelGroupColor->x(), Rundown::DEFAULT_ITEM_HEIGHT - 1);
        this->labelIcon->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
        this->labelGpiConnected->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
        this->labelAutoPlay->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
        this->labelLoop->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
    }

    this->compactView = compactView;
}

bool RundownGroupWidget::isGroup() const
{
    return true;
}

bool RundownGroupWidget::isInGroup() const
{
    return this->inGroup;
}

void RundownGroupWidget::setInGroup(bool inGroup)
{
    this->inGroup = inGroup;
}

AbstractCommand* RundownGroupWidget::getCommand()
{
    return &this->command;
}

LibraryModel* RundownGroupWidget::getLibraryModel()
{
    return &this->model;
}

QString RundownGroupWidget::getColor() const
{
    return this->color;
}

void RundownGroupWidget::setExpanded(bool expanded)
{
    this->labelGroupColor->setVisible(expanded);
}

void RundownGroupWidget::setColor(const QString& color)
{
    this->color = color;
    this->setStyleSheet(QString("#frameItem, #frameStatus { background-color: %1; }").arg(color));
}

void RundownGroupWidget::setSelected(bool selected)
{
    this->selected = selected;
}

void RundownGroupWidget::setActive(bool active)
{
    this->active = active;

    this->animation->stop();

    if (this->active)
        RundownWidgetHelper::setActiveColorPalette(this->labelActiveColor, this->command.getChannel());
    else
        RundownWidgetHelper::clearActiveColorPalette(this->labelActiveColor);
}

void RundownGroupWidget::setUsed(bool used)
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

bool RundownGroupWidget::executeCommand(Playout::PlayoutType type)
{
    if (this->active)
    {
        this->animation->setChannel(this->command.getChannel());
        this->animation->start(1);
    }

    if (type == Playout::PlayoutType::Play || type == Playout::PlayoutType::PlayNow)
    {
        if (this->command.getDuration() > 0)
            EventManager::getInstance().fireDurationChangedEvent(DurationChangedEvent(this->command.getDuration()));

        if (this->markUsedItems)
            setUsed(true);
    }
    else if (type == Playout::PlayoutType::Stop)
    {
        if (this->command.getDuration() > 0)
            EventManager::getInstance().fireDurationChangedEvent(DurationChangedEvent(0));
    }
    else if (type == Playout::PlayoutType::Clear || type == Playout::PlayoutType::ClearVideoLayer || type == Playout::PlayoutType::ClearChannel)
    {
        EventManager::getInstance().fireDurationChangedEvent(DurationChangedEvent(0)); // Reset counter.
    }

    return true;
}

bool RundownGroupWidget::executeOscCommand(Playout::PlayoutType type)
{
    if (this->parentWidget()->parentWidget() == NULL)
        return true;

    QTreeWidget* treeWidgetRundown = dynamic_cast<QTreeWidget*>(this->parentWidget()->parentWidget());
    for (int i = 0; i < treeWidgetRundown->invisibleRootItem()->childCount(); i++)
    {
        QTreeWidgetItem* child = treeWidgetRundown->invisibleRootItem()->child(i);
        QWidget* widget = treeWidgetRundown->itemWidget(child, 0);
        if (widget == this)
        {
            EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(type, child));

            if (type == Playout::PlayoutType::Play || type == Playout::PlayoutType::PlayNow)
            {
                EventManager::getInstance().fireDurationChangedEvent(DurationChangedEvent(this->command.getDuration()));

                if (this->markUsedItems)
                    setUsed(true);
            }

            break;
        }
    }

    return true;
}

void RundownGroupWidget::checkGpiConnection()
{
    this->labelGpiConnected->setVisible(this->command.getAllowGpi());

    if (GpiManager::getInstance().getGpiDevice()->isConnected())
        this->labelGpiConnected->setPixmap(RundownWidgetHelper::gpiConnectedPixmap());
    else
        this->labelGpiConnected->setPixmap(RundownWidgetHelper::gpiDisconnectedPixmap());
}

void RundownGroupWidget::configureOscSubscriptions()
{
    delete this->stopControlSubscription;
    this->stopControlSubscription = nullptr;

    delete this->playControlSubscription;
    this->playControlSubscription = nullptr;

    delete this->loadControlSubscription;
    this->loadControlSubscription = nullptr;

    delete this->pauseControlSubscription;
    this->pauseControlSubscription = nullptr;

    delete this->nextControlSubscription;
    this->nextControlSubscription = nullptr;

    delete this->updateControlSubscription;
    this->updateControlSubscription = nullptr;

    delete this->invokeControlSubscription;
    this->invokeControlSubscription = nullptr;

    delete this->clearControlSubscription;
    this->clearControlSubscription = nullptr;

    delete this->clearVideolayerControlSubscription;
    this->clearVideolayerControlSubscription = nullptr;

    delete this->clearChannelControlSubscription;
    this->clearChannelControlSubscription = nullptr;

    if (!this->command.getAllowRemoteTriggering() || this->command.getRemoteTriggerId().trimmed().isEmpty())
        return;

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

    QString invokeControlFilter = Osc::ITEM_CONTROL_INVOKE_FILTER;
    invokeControlFilter.replace("#UID#", this->command.getRemoteTriggerId());
    this->invokeControlSubscription = new OscSubscription(invokeControlFilter, this);
    QObject::connect(this->invokeControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(invokeControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

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

void RundownGroupWidget::durationChanged(int duration)
{
    Q_UNUSED(duration);

    QTime time = QTime::fromString(QString("00:00:00").append((this->useDropFrameNotation == true) ? ".00" : ":00"));
    this->labelDuration->setText(QString("Duration: %1").arg(Timecode::fromTime(time.addMSecs(this->command.getDuration()), this->useDropFrameNotation)));
}

void RundownGroupWidget::notesChanged(const QString& note)
{
    Q_UNUSED(note);

    this->labelNoteField->setText(this->command.getNotes());
}

void RundownGroupWidget::allowGpiChanged(bool allowGpi)
{
    Q_UNUSED(allowGpi);

    checkGpiConnection();
}

void RundownGroupWidget::gpiConnectionStateChanged(bool connected, GpiDevice* device)
{
    Q_UNUSED(connected);
    Q_UNUSED(device);

    checkGpiConnection();
}

void RundownGroupWidget::autoPlayChanged(bool autoPlay)
{
    this->labelAutoPlay->setVisible(autoPlay);
}

void RundownGroupWidget::loopChanged(bool loop)
{
    this->labelLoop->setVisible(loop);
}

void RundownGroupWidget::remoteTriggerIdChanged(const QString& remoteTriggerId)
{
    configureOscSubscriptions();

    if (remoteTriggerId.trimmed().isEmpty() || !this->command.getAllowRemoteTriggering())
        this->labelRemoteTriggerId->setText("");
    else
        this->labelRemoteTriggerId->setText(QString::fromUtf8("\xe2\x87\xa5 %1").arg(remoteTriggerId));
}

void RundownGroupWidget::stopControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeOscCommand(Playout::PlayoutType::Stop);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::Stop);
    }
}

void RundownGroupWidget::playControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeOscCommand(Playout::PlayoutType::Play);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::Play);
    }
}

void RundownGroupWidget::loadControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeOscCommand(Playout::PlayoutType::Load);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::Load);
    }
}

void RundownGroupWidget::pauseControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeOscCommand(Playout::PlayoutType::PauseResume);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::PauseResume);
    }
}

void RundownGroupWidget::invokeControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeOscCommand(Playout::PlayoutType::Invoke);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::Invoke);
    }
}

void RundownGroupWidget::nextControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeOscCommand(Playout::PlayoutType::Next);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::Next);
    }
}

void RundownGroupWidget::updateControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeOscCommand(Playout::PlayoutType::Update);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::Update);
    }
}

void RundownGroupWidget::clearControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeOscCommand(Playout::PlayoutType::Clear);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::Clear);
    }
}

void RundownGroupWidget::clearVideolayerControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeOscCommand(Playout::PlayoutType::ClearVideoLayer);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::ClearVideoLayer);
    }
}

void RundownGroupWidget::clearChannelControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (this->command.getAllowRemoteTriggering() && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->command.clearChannelOverride();
        if (RundownWidgetHelper::isItemChannelLocked(this))
            return;
        executeOscCommand(Playout::PlayoutType::ClearChannel);
        RundownWidgetHelper::logPlayoutAction(this, Playout::PlayoutType::ClearChannel);
    }
}

void RundownGroupWidget::updateGroupInfo(QTreeWidgetItem* groupItem)
{
    if (groupItem == nullptr)
        return;

    QTreeWidget* treeWidget = groupItem->treeWidget();
    if (treeWidget == nullptr)
        return;

    QSet<int> channels;
    int imageCount = 0;
    int videoCount = 0;
    int templateCount = 0;
    int audioCount = 0;
    int otherCount = 0;

    for (int i = 0; i < groupItem->childCount(); i++)
    {
        QTreeWidgetItem* child = groupItem->child(i);
        QWidget* childWidget = treeWidget->itemWidget(child, 0);
        if (childWidget == nullptr)
            continue;

        AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(childWidget);
        if (rundownWidget == nullptr)
            continue;

        QString type = rundownWidget->getLibraryModel()->getType();
        int channel = rundownWidget->getCommand()->getChannel();
        channels.insert(channel);

        if (type == Rundown::STILL)
            imageCount++;
        else if (type == Rundown::MOVIE)
            videoCount++;
        else if (type == Rundown::TEMPLATE)
            templateCount++;
        else if (type == Rundown::AUDIO)
            audioCount++;
        else
            otherCount++;
    }

    // Build type list in fixed order with counts and plurals.
    int typeLineCount = 0;
    QString html = "<table cellspacing='0' cellpadding='0'>";
    auto addRow = [&](int count, const QString& singular, const QString& plural)
    {
        if (count > 0)
        {
            html += QString("<tr><td align='right'>%1</td><td>&nbsp;%2</td></tr>")
                    .arg(count).arg(count == 1 ? singular : plural);
            typeLineCount++;
        }
    };
    addRow(imageCount, "Image", "Images");
    addRow(videoCount, "Video", "Videos");
    addRow(templateCount, "Template", "Templates");
    addRow(audioCount, "Audio", "Audio");
    addRow(otherCount, "Other", "Others");
    html += "</table>";

    this->labelTypeList->setTextFormat(Qt::RichText);
    this->labelTypeList->setText(html);

    // Update channel badge text and color based on children's channels.
    QFont badgeFont = this->labelColor->font();
    if (channels.isEmpty())
    {
        badgeFont.setPixelSize(16);
        this->labelColor->setFont(badgeFont);
        this->labelColor->setText("-");
        QColor color = RundownWidgetHelper::channelColor(0);
        this->labelColor->setStyleSheet(QString("background-color: %1; color: white; border: 2px solid #1a1a1a;").arg(color.name()));
    }
    else if (channels.size() == 1)
    {
        badgeFont.setPixelSize(16);
        this->labelColor->setFont(badgeFont);
        this->labelColor->setText(QString::number(*channels.begin()));
        QColor color = RundownWidgetHelper::channelColor(*channels.begin());
        this->labelColor->setStyleSheet(QString("background-color: %1; color: white; border: 2px solid #1a1a1a;").arg(color.name()));
    }
    else
    {
        badgeFont.setPixelSize(10);
        this->labelColor->setFont(badgeFont);
        this->labelColor->setText("mix");
        this->labelColor->setStyleSheet("background-color: #444444; color: white; border: 2px solid #1a1a1a;");
    }

    // Calculate needed height based on type lines.
    int lineHeight = 16;
    int bottomPadding = 6;
    int baseHeight = Rundown::DEFAULT_ITEM_HEIGHT;
    int neededHeight = qMax(baseHeight, 19 + typeLineCount * lineHeight + bottomPadding);

    if (this->compactView)
        neededHeight = qMax((int)Rundown::COMPACT_ITEM_HEIGHT, neededHeight);

    // Resize the widget, badge, and activity bar.
    this->setFixedHeight(neededHeight);
    this->labelColor->setFixedHeight(neededHeight);
    this->labelActiveColor->setFixedSize(this->labelActiveColor->width(), neededHeight);

    // Reposition labelGroupColor to the bottom edge.
    this->labelGroupColor->move(this->labelGroupColor->x(), neededHeight - 1);

    // Resize the type list label to fit.
    this->labelTypeList->setGeometry(this->labelTypeList->x(), this->labelTypeList->y(),
                                      this->labelTypeList->width(), typeLineCount * lineHeight);
}
