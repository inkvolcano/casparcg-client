#include "InspectorGatewayWidget.h"

#include "Global.h"

#include "EventManager.h"

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>

InspectorGatewayWidget::InspectorGatewayWidget(QWidget* parent)
    : QWidget(parent),
      model(NULL), command(NULL)
{
    QGridLayout* layout = new QGridLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(4);

    this->labelLinkGateway = new QLabel("Link to Gateway:", this);
    this->comboBoxLinkGateway = new QComboBox(this);
    this->comboBoxLinkGateway->setFocusPolicy(Qt::ClickFocus);

    this->labelExitLabel = new QLabel("Exit Label:", this);
    this->lineEditExitLabel = new QLineEdit(this);
    this->lineEditExitLabel->setFocusPolicy(Qt::ClickFocus);

    this->labelSelectedExit = new QLabel("Selected Exit:", this);
    this->comboBoxSelectedExit = new QComboBox(this);
    this->comboBoxSelectedExit->setFocusPolicy(Qt::ClickFocus);

    // Condition controls (all gateway entrance types).
    this->conditionSeparator = new QFrame(this);
    this->conditionSeparator->setFrameShape(QFrame::HLine);
    this->conditionSeparator->setFrameShadow(QFrame::Sunken);

    this->checkBoxConditionEnabled = new QCheckBox("Enable time condition", this);

    this->labelConditionHeader = new QLabel("If time at gateway", this);
    this->labelConditionTime = new QLabel("this time", this);
    this->comboBoxConditionOperator = new QComboBox(this);
    this->comboBoxConditionOperator->setFocusPolicy(Qt::ClickFocus);
    this->comboBoxConditionOperator->addItems({"is after", "is before", "is at or after", "is at or before", "equals"});
    this->spinBoxConditionHour = new QSpinBox(this);
    this->spinBoxConditionHour->setRange(0, 23);
    this->spinBoxConditionHour->setFocusPolicy(Qt::ClickFocus);
    this->spinBoxConditionMinute = new QSpinBox(this);
    this->spinBoxConditionMinute->setRange(0, 59);
    this->spinBoxConditionMinute->setFocusPolicy(Qt::ClickFocus);
    this->spinBoxConditionSecond = new QSpinBox(this);
    this->spinBoxConditionSecond->setRange(0, 59);
    this->spinBoxConditionSecond->setFocusPolicy(Qt::ClickFocus);

    this->labelConditionExit = new QLabel("Then use:", this);
    this->comboBoxConditionExit = new QComboBox(this);
    this->comboBoxConditionExit->setFocusPolicy(Qt::ClickFocus);

    this->labelConditionElse = new QLabel("Else use:", this);
    this->comboBoxConditionElse = new QComboBox(this);
    this->comboBoxConditionElse->setFocusPolicy(Qt::ClickFocus);

    // Time row sub-layout: [hour] : [minute] : [second]
    QHBoxLayout* timeLayout = new QHBoxLayout();
    timeLayout->setContentsMargins(0, 0, 0, 0);
    timeLayout->setSpacing(4);
    timeLayout->addWidget(this->spinBoxConditionHour);
    timeLayout->addWidget(new QLabel(":", this));
    timeLayout->addWidget(this->spinBoxConditionMinute);
    timeLayout->addWidget(new QLabel(":", this));
    timeLayout->addWidget(this->spinBoxConditionSecond);
    timeLayout->addStretch();

    layout->addWidget(this->labelLinkGateway, 0, 0);
    layout->addWidget(this->comboBoxLinkGateway, 0, 1);
    layout->addWidget(this->labelExitLabel, 1, 0);
    layout->addWidget(this->lineEditExitLabel, 1, 1);
    layout->addWidget(this->labelSelectedExit, 2, 0);
    layout->addWidget(this->comboBoxSelectedExit, 2, 1);

    // Spacer + condition section.
    layout->addWidget(this->conditionSeparator, 3, 0, 1, 2);
    layout->addWidget(this->checkBoxConditionEnabled, 4, 0, 1, 2);
    layout->addWidget(this->labelConditionHeader, 5, 0, 1, 2);
    layout->addWidget(this->comboBoxConditionOperator, 6, 0, 1, 2);
    layout->addWidget(this->labelConditionTime, 7, 0);
    layout->addLayout(timeLayout, 7, 1);
    layout->addWidget(this->labelConditionExit, 8, 0);
    layout->addWidget(this->comboBoxConditionExit, 8, 1);
    layout->addWidget(this->labelConditionElse, 9, 0);
    layout->addWidget(this->comboBoxConditionElse, 9, 1);

    QObject::connect(this->comboBoxLinkGateway, SIGNAL(currentIndexChanged(int)), this, SLOT(linkGatewayChanged(int)));
    QObject::connect(this->lineEditExitLabel, SIGNAL(editingFinished()), this, SLOT(exitLabelChanged()));
    QObject::connect(this->comboBoxSelectedExit, SIGNAL(currentTextChanged(const QString&)), this, SLOT(selectedExitChanged(const QString&)));
    QObject::connect(this->checkBoxConditionEnabled, SIGNAL(stateChanged(int)), this, SLOT(conditionEnabledChanged(int)));
    QObject::connect(this->comboBoxConditionOperator, SIGNAL(currentIndexChanged(int)), this, SLOT(conditionOperatorChanged(int)));
    QObject::connect(this->spinBoxConditionHour, SIGNAL(valueChanged(int)), this, SLOT(conditionHourChanged(int)));
    QObject::connect(this->spinBoxConditionMinute, SIGNAL(valueChanged(int)), this, SLOT(conditionMinuteChanged(int)));
    QObject::connect(this->spinBoxConditionSecond, SIGNAL(valueChanged(int)), this, SLOT(conditionSecondChanged(int)));
    QObject::connect(this->comboBoxConditionExit, SIGNAL(currentTextChanged(const QString&)), this, SLOT(conditionExitChanged(const QString&)));
    QObject::connect(this->comboBoxConditionElse, SIGNAL(currentTextChanged(const QString&)), this, SLOT(conditionElseChanged(const QString&)));

    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));
}

void InspectorGatewayWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->command = nullptr;
    this->model = event.getLibraryModel();

    blockAllSignals(true);

    if (dynamic_cast<GatewayCommand*>(event.getCommand()))
    {
        this->command = dynamic_cast<GatewayCommand*>(event.getCommand());
        this->itemType = this->model->getType();

        if (this->command->getIsExit())
        {
            // Exit item: show exit label editor and link-to-gateway combo.
            this->labelExitLabel->setVisible(true);
            this->lineEditExitLabel->setVisible(true);
            this->lineEditExitLabel->setText(this->command->getExitLabel());

            this->labelSelectedExit->setVisible(false);
            this->comboBoxSelectedExit->setVisible(false);

            // Show link-to-gateway combo for exits.
            this->labelLinkGateway->setVisible(true);
            this->comboBoxLinkGateway->setVisible(true);
            this->comboBoxLinkGateway->clear();

            // Populate with gateway entrances of the same type.
            QList<QPair<QString, QString>> entrances = EventManager::getInstance().getGatewayEntrances(this->itemType);
            this->comboBoxLinkGateway->addItem("(Unlinked)", QString());
            for (const auto& pair : entrances)
                this->comboBoxLinkGateway->addItem(pair.second, pair.first);

            // Select the current gatewayId if linked.
            QString currentId = this->command->getGatewayId();
            if (!currentId.isEmpty())
            {
                int idx = this->comboBoxLinkGateway->findData(currentId);
                if (idx >= 0)
                    this->comboBoxLinkGateway->setCurrentIndex(idx);
            }
        }
        else
        {
            // Entrance item: hide exit label and link-to-gateway, show selected exit combo.
            this->labelExitLabel->setVisible(false);
            this->lineEditExitLabel->setVisible(false);

            this->labelLinkGateway->setVisible(false);
            this->comboBoxLinkGateway->setVisible(false);

            QStringList exitLabels = EventManager::getInstance().getGatewayExitLabels(this->command->getGatewayId());
            if (exitLabels.isEmpty())
                exitLabels << this->command->getSelectedExitLabel();

            // "Selected Exit" is shown when condition is off.
            this->comboBoxSelectedExit->clear();
            this->comboBoxSelectedExit->addItems(exitLabels);
            int idx = this->comboBoxSelectedExit->findText(this->command->getSelectedExitLabel());
            if (idx >= 0)
                this->comboBoxSelectedExit->setCurrentIndex(idx);

            // Condition controls (all gateway entrance types).
            this->checkBoxConditionEnabled->setChecked(this->command->getConditionEnabled());
            this->comboBoxConditionOperator->setCurrentIndex(this->command->getConditionOperator());
            this->spinBoxConditionHour->setValue(this->command->getConditionHour());
            this->spinBoxConditionMinute->setValue(this->command->getConditionMinute());
            this->spinBoxConditionSecond->setValue(this->command->getConditionSecond());

            this->comboBoxConditionExit->clear();
            this->comboBoxConditionExit->addItems(exitLabels);
            int condIdx = this->comboBoxConditionExit->findText(this->command->getConditionExitLabel());
            if (condIdx >= 0)
                this->comboBoxConditionExit->setCurrentIndex(condIdx);

            this->comboBoxConditionElse->clear();
            this->comboBoxConditionElse->addItems(exitLabels);
            int elseIdx = this->comboBoxConditionElse->findText(this->command->getSelectedExitLabel());
            if (elseIdx >= 0)
                this->comboBoxConditionElse->setCurrentIndex(elseIdx);
        }

        updateConditionVisibility();
    }

    blockAllSignals(false);
}

void InspectorGatewayWidget::blockAllSignals(bool block)
{
    this->comboBoxLinkGateway->blockSignals(block);
    this->lineEditExitLabel->blockSignals(block);
    this->comboBoxSelectedExit->blockSignals(block);
    this->checkBoxConditionEnabled->blockSignals(block);
    this->comboBoxConditionOperator->blockSignals(block);
    this->spinBoxConditionHour->blockSignals(block);
    this->spinBoxConditionMinute->blockSignals(block);
    this->spinBoxConditionSecond->blockSignals(block);
    this->comboBoxConditionExit->blockSignals(block);
    this->comboBoxConditionElse->blockSignals(block);
}

void InspectorGatewayWidget::updateConditionVisibility()
{
    bool isGatewayEntrance = (this->command != nullptr && !this->command->getIsExit()
                               && (this->itemType == Rundown::AUTOPLAYGATEWAY || this->itemType == Rundown::COMMANDGATEWAY || this->itemType == Rundown::FOCUSGATEWAY));
    bool isEntrance = (this->command != nullptr && !this->command->getIsExit());

    this->conditionSeparator->setVisible(isGatewayEntrance);
    this->checkBoxConditionEnabled->setVisible(isGatewayEntrance);

    bool conditionOn = isGatewayEntrance && this->command->getConditionEnabled();

    // When condition is on, hide "Selected Exit" (replaced by "Then/Else use:").
    // When condition is off, show "Selected Exit" for any entrance type.
    this->labelSelectedExit->setVisible(isEntrance && !conditionOn);
    this->comboBoxSelectedExit->setVisible(isEntrance && !conditionOn);

    this->labelConditionHeader->setVisible(conditionOn);
    this->labelConditionTime->setVisible(conditionOn);
    this->comboBoxConditionOperator->setVisible(conditionOn);
    this->spinBoxConditionHour->setVisible(conditionOn);
    this->spinBoxConditionMinute->setVisible(conditionOn);
    this->spinBoxConditionSecond->setVisible(conditionOn);
    this->labelConditionExit->setVisible(conditionOn);
    this->comboBoxConditionExit->setVisible(conditionOn);
    this->labelConditionElse->setVisible(conditionOn);
    this->comboBoxConditionElse->setVisible(conditionOn);

    // Show active condition indicator: bold the currently effective exit label.
    if (conditionOn)
    {
        QString effective = this->command->getEffectiveExitLabel();
        QString condExit = this->command->getConditionExitLabel();
        bool conditionMet = (!condExit.isEmpty() && effective == condExit);
        this->labelConditionExit->setStyleSheet(conditionMet ? "font-weight: bold;" : "color: rgba(150,150,150,200);");
        this->labelConditionElse->setStyleSheet(conditionMet ? "color: rgba(150,150,150,200);" : "font-weight: bold;");
    }
}

void InspectorGatewayWidget::linkGatewayChanged(int index)
{
    if (this->command == nullptr || index < 0)
        return;

    QString gatewayId = this->comboBoxLinkGateway->itemData(index).toString();
    this->command->setGatewayId(gatewayId);
}

void InspectorGatewayWidget::exitLabelChanged()
{
    if (this->command == nullptr)
        return;

    this->command->setExitLabel(this->lineEditExitLabel->text());
}

void InspectorGatewayWidget::selectedExitChanged(const QString& text)
{
    if (this->command == nullptr || text.isEmpty())
        return;

    this->command->setSelectedExitLabel(text);
}

void InspectorGatewayWidget::conditionEnabledChanged(int state)
{
    if (this->command == nullptr)
        return;

    this->command->setConditionEnabled(state == Qt::Checked);
    updateConditionVisibility();
}

void InspectorGatewayWidget::conditionOperatorChanged(int index)
{
    if (this->command == nullptr)
        return;

    this->command->setConditionOperator(index);
    updateConditionVisibility();
}

void InspectorGatewayWidget::conditionHourChanged(int value)
{
    if (this->command == nullptr)
        return;

    this->command->setConditionHour(value);
    updateConditionVisibility();
}

void InspectorGatewayWidget::conditionMinuteChanged(int value)
{
    if (this->command == nullptr)
        return;

    this->command->setConditionMinute(value);
    updateConditionVisibility();
}

void InspectorGatewayWidget::conditionSecondChanged(int value)
{
    if (this->command == nullptr)
        return;

    this->command->setConditionSecond(value);
    updateConditionVisibility();
}

void InspectorGatewayWidget::conditionExitChanged(const QString& text)
{
    if (this->command == nullptr || text.isEmpty())
        return;

    this->command->setConditionExitLabel(text);
    updateConditionVisibility();
}

void InspectorGatewayWidget::conditionElseChanged(const QString& text)
{
    if (this->command == nullptr || text.isEmpty())
        return;

    // "Else use:" sets the selectedExitLabel (the default when condition is false).
    this->command->setSelectedExitLabel(text);
    updateConditionVisibility();
}
