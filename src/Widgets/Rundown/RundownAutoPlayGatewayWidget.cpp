#include "RundownAutoPlayGatewayWidget.h"

#include "Global.h"

#include "RundownWidgetHelper.h"
#include "GpiManager.h"

#include "EventManager.h"

#include <QtCore/QObject>

#include <QtGui/QPixmap>

#include <QtCore/QTimer>

#include <QtWidgets/QApplication>
#include <QtWidgets/QTreeWidget>

RundownAutoPlayGatewayWidget::RundownAutoPlayGatewayWidget(const LibraryModel& model, QWidget* parent, const QString& color,
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
        RundownWidgetHelper::updateBankBadge(bankBadge, bank);
        RundownWidgetHelper::configureBankOscSubscriptions(this, this, bank);
    });
    RundownWidgetHelper::updateBankBadge(bankBadge, this->command.getTriggerBank());
    RundownWidgetHelper::configureBankOscSubscriptions(this, this, this->command.getTriggerBank());

    // Button container for exit selection buttons (entrance) or return button (exit).
    this->buttonContainer = new QWidget(this->frameItem);
    this->buttonLayout = new QVBoxLayout(this->buttonContainer);
    this->buttonLayout->setContentsMargins(0, 0, 0, 2);
    this->buttonLayout->setSpacing(2);

    QObject::connect(&EventManager::getInstance(), SIGNAL(labelChanged(const LabelChangedEvent&)), this, SLOT(labelChanged(const LabelChangedEvent&)));
    QObject::connect(&EventManager::getInstance(), &EventManager::gatewayExitsChanged, this, &RundownAutoPlayGatewayWidget::gatewayExitsChanged);

    QObject::connect(&this->command, &GatewayCommand::exitLabelChanged, this, &RundownAutoPlayGatewayWidget::updateVisuals);
    QObject::connect(&this->command, &GatewayCommand::selectedExitLabelChanged, this, &RundownAutoPlayGatewayWidget::updateVisuals);

    this->conditionTimer = new QTimer(this);
    this->conditionTimer->setInterval(30000);
    QObject::connect(this->conditionTimer, &QTimer::timeout, this, &RundownAutoPlayGatewayWidget::rebuildExitButtons);

    updateVisuals();
}

void RundownAutoPlayGatewayWidget::updateVisuals()
{
    if (this->command.getIsExit())
    {
        QString exitLabel = this->command.getExitLabel();
        if (exitLabel.isEmpty())
            exitLabel = "Exit";
        this->labelLabel->setText(QString("%1: %2").arg(exitLabel, this->model.getLabel()));
    }
    else
    {
        this->labelLabel->setText(this->model.getLabel());
    }

    this->labelIcon->setPixmap(QPixmap(this->command.getIsExit()
        ? ":/Graphics/Images/gateway_exit.png"
        : ":/Graphics/Images/gateway_entry.png"));

    RundownWidgetHelper::setupGatewayBadge(this->labelColor, this->command.getIsExit(), "rgba(0, 150, 136, 220)", "autoplay");
    rebuildExitButtons();
}

void RundownAutoPlayGatewayWidget::setTreeItem(QTreeWidgetItem* item)
{
    this->treeItem = item;
    rebuildExitButtons();
}

void RundownAutoPlayGatewayWidget::gatewayExitsChanged(const QString& gatewayId)
{
    if (this->command.getGatewayId() == gatewayId && !this->command.getIsExit())
        rebuildExitButtons();
}

void RundownAutoPlayGatewayWidget::rebuildExitButtons()
{
    // Clear existing buttons.
    QLayoutItem* child;
    while ((child = this->buttonLayout->takeAt(0)) != nullptr)
    {
        delete child->widget();
        delete child;
    }

    static const int BUTTON_HEIGHT = 18;
    static const int BUTTON_SPACING = 2;
    static const int BASE_HEIGHT = Rundown::DEFAULT_ITEM_HEIGHT;
    static const QString SELECTED_STYLE = "QPushButton { background-color: rgba(0, 180, 150, 200); border: 1px solid rgba(255,255,255,40); color: white; font-size: 11px; text-align: left; padding-left: 6px; }";
    static const QString NORMAL_STYLE = "QPushButton { background-color: rgba(60, 60, 60, 200); border: 1px solid rgba(255,255,255,20); color: rgba(200,200,200,200); font-size: 11px; text-align: left; padding-left: 6px; }";
    static const QString RETURN_STYLE = "QPushButton { background-color: rgba(80, 60, 120, 200); border: 1px solid rgba(255,255,255,30); color: rgba(200,200,200,220); font-size: 11px; text-align: left; padding-left: 6px; }";

    int buttonCount = 0;

    if (this->command.getIsExit())
    {
        // Exit widget: single "Return" button.
        QPushButton* returnBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90 Return"), this->buttonContainer);
        returnBtn->setFixedHeight(BUTTON_HEIGHT);
        returnBtn->setStyleSheet(RETURN_STYLE);
        returnBtn->setCursor(Qt::PointingHandCursor);
        QObject::connect(returnBtn, &QPushButton::clicked, [this]() {
            emit requestFocusJumpToEntrance(this->command.getGatewayId());
        });
        this->buttonLayout->addWidget(returnBtn);
        buttonCount = 1;
    }
    else
    {
        // Entrance widget: one button per exit.
        QStringList exitLabels = EventManager::getInstance().getGatewayExitLabels(this->command.getGatewayId());
        if (exitLabels.isEmpty())
            exitLabels << this->command.getSelectedExitLabel();

        QString selected = this->command.getEffectiveExitLabel();

        for (const QString& label : exitLabels)
        {
            QPushButton* btn = new QPushButton(label, this->buttonContainer);
            btn->setFixedHeight(BUTTON_HEIGHT);
            btn->setStyleSheet(label == selected ? SELECTED_STYLE : NORMAL_STYLE);
            btn->setCursor(Qt::PointingHandCursor);
            QObject::connect(btn, &QPushButton::clicked, [this, label]() {
                this->command.setSelectedExitLabel(label);
                rebuildExitButtons();
            });
            this->buttonLayout->addWidget(btn);
        }
        buttonCount = exitLabels.count();
    }

    int newHeight = BASE_HEIGHT + qMax(buttonCount, 1) * (BUTTON_HEIGHT + BUTTON_SPACING);
    this->buttonContainer->setGeometry(62, BASE_HEIGHT, this->frameItem->width() - 70, buttonCount * (BUTTON_HEIGHT + BUTTON_SPACING));
    this->buttonContainer->setVisible(true);

    this->setFixedHeight(newHeight);
    this->frameItem->setMinimumHeight(newHeight);
    this->labelActiveColor->setFixedHeight(newHeight);
    this->labelColor->setFixedHeight(newHeight);

    if (this->treeItem != nullptr)
    {
        this->treeItem->setSizeHint(0, QSize(this->width(), newHeight));

        if (this->treeItem->treeWidget() != nullptr)
            this->treeItem->treeWidget()->doItemsLayout();
    }

    // Auto-update button highlighting when time condition is active.
    if (!this->command.getIsExit() && this->command.getConditionEnabled())
    {
        if (!this->conditionTimer->isActive())
            this->conditionTimer->start();
    }
    else
    {
        this->conditionTimer->stop();
    }
}

void RundownAutoPlayGatewayWidget::labelChanged(const LabelChangedEvent& event)
{
    if (!this->selected)
        return;

    this->model.setLabel(event.getLabel());
    updateVisuals();
}

AbstractRundownWidget* RundownAutoPlayGatewayWidget::clone()
{
    RundownAutoPlayGatewayWidget* widget = new RundownAutoPlayGatewayWidget(this->model, this->parentWidget(), this->color,
                                                                          this->active, this->inGroup, this->compactView);

    widget->command.setGatewayId(this->command.getGatewayId());
    widget->command.setIsExit(this->command.getIsExit());
    widget->command.setExitLabel(this->command.getExitLabel());
    widget->command.setSelectedExitLabel(this->command.getSelectedExitLabel());
    widget->command.setConditionEnabled(this->command.getConditionEnabled());
    widget->command.setConditionOperator(this->command.getConditionOperator());
    widget->command.setConditionHour(this->command.getConditionHour());
    widget->command.setConditionMinute(this->command.getConditionMinute());
    widget->command.setConditionSecond(this->command.getConditionSecond());
    widget->command.setConditionExitLabel(this->command.getConditionExitLabel());
    widget->updateVisuals();

    return widget;
}

void RundownAutoPlayGatewayWidget::readProperties(boost::property_tree::wptree& pt)
{
    if (pt.count(L"color") > 0) setColor(QString::fromStdWString(pt.get<std::wstring>(L"color")));

    updateVisuals();
}

void RundownAutoPlayGatewayWidget::writeProperties(QXmlStreamWriter& writer)
{
    writer.writeTextElement("color", this->color);
}

void RundownAutoPlayGatewayWidget::setCompactView(bool compactView)
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

bool RundownAutoPlayGatewayWidget::isGroup() const
{
    return false;
}

bool RundownAutoPlayGatewayWidget::isInGroup() const
{
    return this->inGroup;
}

AbstractCommand* RundownAutoPlayGatewayWidget::getCommand()
{
    return &this->command;
}

LibraryModel* RundownAutoPlayGatewayWidget::getLibraryModel()
{
    return &this->model;
}

QString RundownAutoPlayGatewayWidget::getColor() const
{
    return this->color;
}

void RundownAutoPlayGatewayWidget::setColor(const QString& color)
{
    this->color = color;

    if (this->color.isEmpty())
        this->setStyleSheet(QString("#frameItem, #frameStatus { background-color: %1; }").arg(Color::DEFAULT_AUTOPLAYGATEWAY_COLOR));
    else
        this->setStyleSheet(QString("#frameItem, #frameStatus { background-color: %1; }").arg(color));
}

void RundownAutoPlayGatewayWidget::setSelected(bool selected)
{
    this->selected = selected;
}

void RundownAutoPlayGatewayWidget::setActive(bool active)
{
    this->active = active;

    this->animation->stop();

    if (this->active)
        RundownWidgetHelper::setActiveColorPalette(this->labelActiveColor, this->command.getChannel());
    else
        RundownWidgetHelper::clearActiveColorPalette(this->labelActiveColor);
}

void RundownAutoPlayGatewayWidget::setInGroup(bool inGroup)
{
    this->inGroup = inGroup;
    this->labelGroupColor->setVisible(this->inGroup);
}

bool RundownAutoPlayGatewayWidget::executeCommand(Playout::PlayoutType type)
{
    Q_UNUSED(type);

    if (this->active)
    {
        this->animation->setChannel(this->command.getChannel());
        this->animation->start(1);
    }

    return false;
}
