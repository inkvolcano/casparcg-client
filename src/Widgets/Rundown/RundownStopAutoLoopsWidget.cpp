#include "RundownStopAutoLoopsWidget.h"

#include "Global.h"

#include "RundownWidgetHelper.h"

#include "EventManager.h"

#include <QtCore/QObject>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsOpacityEffect>

RundownStopAutoLoopsWidget::RundownStopAutoLoopsWidget(const LibraryModel& model, QWidget* parent, const QString& color,
                                                       bool active, bool inGroup, bool compactView)
    : QWidget(parent),
      active(active), inGroup(inGroup), compactView(compactView), color(color), model(model)
{
    setupUi(this);

    this->animation = new ActiveAnimation(this->labelActiveColor);

    this->markUsedItems = RundownWidgetHelper::cachedMarkUsedItems();

    setColor(this->color);
    setActive(this->active);
    setCompactView(this->compactView);

    this->labelGroupColor->setVisible(this->inGroup);
    this->labelGroupColor->setStyleSheet(QString("background-color: %1;").arg(Color::DEFAULT_GROUP_COLOR));

    this->labelLabel->setText(this->model.getLabel());
    RundownWidgetHelper::setupChannelBadge(this->frameItem, this->labelColor, 0);
    QLabel* bankBadge = RundownWidgetHelper::createBankBadge(this->frameItem);
    QObject::connect(&this->command, &AbstractCommand::triggerBankChanged, [this, bankBadge](int bank) {
        RundownWidgetHelper::updateBankBadge(bankBadge, bank);
        RundownWidgetHelper::configureBankOscSubscriptions(this, this, bank);
    });
    QObject::connect(&this->command, &AbstractCommand::disabledChanged, this, [this](bool d) { setRundownDisabled(d); });
    RundownWidgetHelper::updateBankBadge(bankBadge, this->command.getTriggerBank());
    RundownWidgetHelper::configureBankOscSubscriptions(this, this, this->command.getTriggerBank());
    RundownWidgetHelper::setupCloneSupport(this, this->frameItem, &this->command);

    QObject::connect(&EventManager::getInstance(), SIGNAL(labelChanged(const LabelChangedEvent&)), this, SLOT(labelChanged(const LabelChangedEvent&)));
}

void RundownStopAutoLoopsWidget::labelChanged(const LabelChangedEvent& event)
{
    // This event is not for us.
    if (!this->selected)
        return;

    this->model.setLabel(event.getLabel());

    this->labelLabel->setText(this->model.getLabel());
}

AbstractRundownWidget* RundownStopAutoLoopsWidget::clone()
{
    RundownStopAutoLoopsWidget* widget = new RundownStopAutoLoopsWidget(this->model, this->parentWidget(), this->color,
                                                                        this->active, this->inGroup, this->compactView);

    AbstractCommand* command = widget->getCommand();
    command->setDelay(this->command.getDelay());
    command->setAllowGpi(this->command.getAllowGpi());
    command->setAllowRemoteTriggering(this->command.getAllowRemoteTriggering());
    command->setRemoteTriggerId(this->command.getRemoteTriggerId());

    return widget;
}

void RundownStopAutoLoopsWidget::readProperties(boost::property_tree::wptree& pt)
{
    if (pt.count(L"color") > 0) setColor(QString::fromStdWString(pt.get<std::wstring>(L"color")));
}

void RundownStopAutoLoopsWidget::writeProperties(QXmlStreamWriter& writer)
{
    writer.writeTextElement("color", this->color);
}

void RundownStopAutoLoopsWidget::setCompactView(bool compactView)
{
    if (compactView)
    {
        this->labelColor->setFixedSize(RundownWidgetHelper::BADGE_WIDTH, Rundown::COMPACT_ITEM_HEIGHT);
        this->labelIcon->setFixedSize(Rundown::COMPACT_ICON_WIDTH, Rundown::COMPACT_ICON_HEIGHT);
    }
    else
    {
        this->labelColor->setFixedSize(RundownWidgetHelper::BADGE_WIDTH, Rundown::DEFAULT_ITEM_HEIGHT);
        this->labelIcon->setFixedSize(Rundown::DEFAULT_ICON_WIDTH, Rundown::DEFAULT_ICON_HEIGHT);
    }

    this->compactView = compactView;
}

bool RundownStopAutoLoopsWidget::isGroup() const
{
    return false;
}

bool RundownStopAutoLoopsWidget::isInGroup() const
{
    return this->inGroup;
}

AbstractCommand* RundownStopAutoLoopsWidget::getCommand()
{
    return &this->command;
}

LibraryModel* RundownStopAutoLoopsWidget::getLibraryModel()
{
    return &this->model;
}

QString RundownStopAutoLoopsWidget::getColor() const
{
    return this->color;
}

void RundownStopAutoLoopsWidget::setColor(const QString& color)
{
    this->color = color;
    this->setStyleSheet(QString("#frameItem, #frameStatus { background-color: %1; }").arg(color));
}

void RundownStopAutoLoopsWidget::setUsed(bool used)
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

void RundownStopAutoLoopsWidget::setSelected(bool selected)
{
    this->selected = selected;
}

void RundownStopAutoLoopsWidget::setActive(bool active)
{
    this->active = active;

    this->animation->stop();

    if (this->active)
        RundownWidgetHelper::setActiveColorPalette(this->labelActiveColor, this->command.getChannel());
    else
        RundownWidgetHelper::clearActiveColorPalette(this->labelActiveColor);
}

void RundownStopAutoLoopsWidget::setInGroup(bool inGroup)
{
    this->inGroup = inGroup;
    this->labelGroupColor->setVisible(this->inGroup);
}

bool RundownStopAutoLoopsWidget::executeCommand(Playout::PlayoutType type)
{
    if (this->command.getDisabled()) return true;

    if (type == Playout::PlayoutType::Play || type == Playout::PlayoutType::PlayNow ||
        type == Playout::PlayoutType::Update || type == Playout::PlayoutType::Next)
    {
        // Fires immediately — a panic action should not wait on the item delay.
        EventManager::getInstance().fireStopAllAutoLoopsEvent();

        if (this->markUsedItems)
            setUsed(true);
    }

    if (this->active)
    {
        this->animation->setChannel(this->command.getChannel());
        this->animation->start(1);
    }

    return true;
}

void RundownStopAutoLoopsWidget::setRundownDisabled(bool disabled)
{
    RundownWidgetHelper::applyDisabledStyle(this, this->labelLabel, disabled);
}
