#include "RundownSeparatorWidget.h"

#include "Global.h"

#include "RundownWidgetHelper.h"
#include "GpiManager.h"

#include "EventManager.h"

#include <QtCore/QObject>

#include <QtWidgets/QApplication>

RundownSeparatorWidget::RundownSeparatorWidget(const LibraryModel& model, QWidget* parent, const QString& color,
                                               bool active, bool inGroup, bool compactView)
    : QWidget(parent),
      active(active), inGroup(inGroup), compactView(compactView), color(color), model(model)
{
    setupUi(this);

    this->animation = new ActiveAnimation(this->labelActiveColor);

    setColor(this->color);
    setActive(this->active);
    setCompactView(this->compactView);

    this->labelGroupColor->setVisible(this->inGroup);
    this->labelGroupColor->setStyleSheet(QString("background-color: %1;").arg(Color::DEFAULT_GROUP_COLOR));

    this->labelActiveColor->setVisible(false);

    this->labelLabel->setText(this->model.getLabel());
    RundownWidgetHelper::setupChannelBadge(this->frameItem, this->labelColor, 0);
    QLabel* bankBadge = RundownWidgetHelper::createBankBadge(this->frameItem);
    QObject::connect(&this->command, &AbstractCommand::triggerBankChanged, [this, bankBadge](int bank) {
    QObject::connect(&this->command, &AbstractCommand::disabledChanged, this, [this](bool d) { setRundownDisabled(d); });
        RundownWidgetHelper::updateBankBadge(bankBadge, bank);
        RundownWidgetHelper::configureBankOscSubscriptions(this, this, bank);
    });
    RundownWidgetHelper::updateBankBadge(bankBadge, this->command.getTriggerBank());
    RundownWidgetHelper::configureBankOscSubscriptions(this, this, this->command.getTriggerBank());
    RundownWidgetHelper::setupCloneSupport(this, this->frameItem, &this->command);

    QObject::connect(&EventManager::getInstance(), SIGNAL(labelChanged(const LabelChangedEvent&)), this, SLOT(labelChanged(const LabelChangedEvent&)));
}

void RundownSeparatorWidget::labelChanged(const LabelChangedEvent& event)
{
    // This event is not for us.
    if (!this->selected)
        return;

    this->model.setLabel(event.getLabel());

    this->labelLabel->setText(this->model.getLabel());
}

AbstractRundownWidget* RundownSeparatorWidget::clone()
{
    RundownSeparatorWidget* widget = new RundownSeparatorWidget(this->model, this->parentWidget(), this->color,
                                                                this->active, this->inGroup, this->compactView);

    return widget;
}

void RundownSeparatorWidget::readProperties(boost::property_tree::wptree& pt)
{
    if (pt.count(L"color") > 0) setColor(QString::fromStdWString(pt.get<std::wstring>(L"color")));
}

void RundownSeparatorWidget::writeProperties(QXmlStreamWriter& writer)
{
    writer.writeTextElement("color", this->color);
}

void RundownSeparatorWidget::setCompactView(bool compactView)
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

bool RundownSeparatorWidget::isGroup() const
{
    return false;
}

bool RundownSeparatorWidget::isInGroup() const
{
    return this->inGroup;
}

AbstractCommand* RundownSeparatorWidget::getCommand()
{
    return &this->command;
}

LibraryModel* RundownSeparatorWidget::getLibraryModel()
{
    return &this->model;
}

QString RundownSeparatorWidget::getColor() const
{
    return this->color;
}

void RundownSeparatorWidget::setColor(const QString& color)
{
    this->color = color;

    if (this->color.isEmpty())
        this->setStyleSheet(QString("#frameItem, #frameStatus { background-color: %1; }").arg(Color::DEFAULT_SEPARATOR_COLOR));
    else
        this->setStyleSheet(QString("#frameItem, #frameStatus { background-color: %1; }").arg(color));
}

void RundownSeparatorWidget::setSelected(bool selected)
{
    this->selected = selected;
}

void RundownSeparatorWidget::setActive(bool active)
{
    this->active = active;

    this->animation->stop();

    if (this->active)
        RundownWidgetHelper::setActiveColorPalette(this->labelActiveColor, this->command.getChannel());
    else
        RundownWidgetHelper::clearActiveColorPalette(this->labelActiveColor);
}

void RundownSeparatorWidget::setInGroup(bool inGroup)
{
    this->inGroup = inGroup;
    this->labelGroupColor->setVisible(this->inGroup);
}

bool RundownSeparatorWidget::executeCommand(Playout::PlayoutType type)
{
    if (this->command.getDisabled()) return true;
    Q_UNUSED(type);

    if (this->active)
    {
        this->animation->setChannel(this->command.getChannel());
        this->animation->start(1);
    }

    return false;
}

void RundownSeparatorWidget::setRundownDisabled(bool disabled)
{
    RundownWidgetHelper::applyDisabledStyle(this, this->labelLabel, disabled);
}
