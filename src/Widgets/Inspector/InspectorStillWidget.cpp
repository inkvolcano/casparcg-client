#include "InspectorStillWidget.h"

#include "Global.h"

#include "DatabaseManager.h"
#include "EventManager.h"
#include "Models/DirectionModel.h"
#include "Models/TransitionModel.h"
#include "Models/TweenModel.h"

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

InspectorStillWidget::InspectorStillWidget(QWidget* parent)
    : QWidget(parent),
      model(NULL), command(NULL), enableOscInputControl(false)
{
    setupUi(this);

    this->enableOscInputControl = (DatabaseManager::getInstance().getConfigurationByName("EnableOscInputControl").getValue() == "true") ? true : false;

    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));

    loadDirection();
    loadTransition();
    loadTween();

    QGridLayout* grid = qobject_cast<QGridLayout*>(this->layout());
    if (grid != nullptr)
    {
        QLabel* labelAutoLoop = new QLabel(tr("Auto-Loop"), this);
        this->checkBoxAutoLoop = new QCheckBox(this);
        this->checkBoxAutoLoop->setLayoutDirection(Qt::RightToLeft);
        this->spinBoxAutoLoopDelay = new QSpinBox(this);
        this->spinBoxAutoLoopDelay->setMinimum(1);
        this->spinBoxAutoLoopDelay->setMaximum(3600);
        this->spinBoxAutoLoopDelay->setSuffix(tr(" sec"));
        this->spinBoxAutoLoopDelay->setValue(Still::DEFAULT_AUTO_LOOP_DELAY);

        QHBoxLayout* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(this->checkBoxAutoLoop);
        row->addWidget(this->spinBoxAutoLoopDelay, 1);

        int newRow = grid->rowCount();
        grid->addWidget(labelAutoLoop, newRow, 0);
        grid->addLayout(row, newRow, 1, 1, 2);

        QObject::connect(this->checkBoxAutoLoop, SIGNAL(stateChanged(int)), this, SLOT(autoLoopChanged(int)));
        QObject::connect(this->spinBoxAutoLoopDelay, SIGNAL(valueChanged(int)), this, SLOT(autoLoopDelayChanged(int)));
    }
}

void InspectorStillWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->command = nullptr;
    this->model = event.getLibraryModel();

    blockAllSignals(true);

    if (dynamic_cast<StillCommand*>(event.getCommand()))
    {
        this->command = dynamic_cast<StillCommand*>(event.getCommand());

        this->comboBoxTransition->setCurrentIndex(this->comboBoxTransition->findText(this->command->getTransition()));
        this->spinBoxTransitionDuration->setValue(this->command->getTransitionDuration());
        this->comboBoxTween->setCurrentIndex(this->comboBoxTween->findText(this->command->getTween()));
        this->comboBoxDirection->setCurrentIndex(this->comboBoxDirection->findText(this->command->getDirection()));
        this->checkBoxTriggerOnNext->setChecked(this->command->getTriggerOnNext());
        this->checkBoxUseAuto->setChecked(this->command->getUseAuto());

        // Show auto play option when OSC input control is enabled.
        if (this->enableOscInputControl)
        {
            this->labelAutoPlay->setEnabled(true);
            this->checkBoxAutoPlay->setEnabled(true);
            this->checkBoxAutoPlay->setChecked(this->command->getAutoPlay());
        }
        else
        {
            this->labelAutoPlay->setEnabled(false);
            this->checkBoxAutoPlay->setEnabled(false);
            this->checkBoxAutoPlay->setChecked(false);
        }

        if (this->checkBoxAutoLoop != nullptr)
            this->checkBoxAutoLoop->setChecked(this->command->getAutoLoop());
        if (this->spinBoxAutoLoopDelay != nullptr)
            this->spinBoxAutoLoopDelay->setValue(this->command->getAutoLoopDelay());
    }

    blockAllSignals(false);
}

void InspectorStillWidget::blockAllSignals(bool block)
{
    this->comboBoxTransition->blockSignals(block);
    this->spinBoxTransitionDuration->blockSignals(block);
    this->comboBoxTween->blockSignals(block);
    this->comboBoxDirection->blockSignals(block);
    this->checkBoxTriggerOnNext->blockSignals(block);
    this->checkBoxUseAuto->blockSignals(block);
    this->checkBoxAutoPlay->blockSignals(block);
    if (this->checkBoxAutoLoop != nullptr)
        this->checkBoxAutoLoop->blockSignals(block);
    if (this->spinBoxAutoLoopDelay != nullptr)
        this->spinBoxAutoLoopDelay->blockSignals(block);
}

void InspectorStillWidget::loadDirection()
{
    // We do not have a command object, block the signals.
    // Events will not be triggered while we update the values.
    this->comboBoxDirection->blockSignals(true);

    QList<DirectionModel> models = DatabaseManager::getInstance().getDirection();
    foreach (DirectionModel model, models)
        this->comboBoxDirection->addItem(model.getValue());

    this->comboBoxDirection->blockSignals(false);
}

void InspectorStillWidget::loadTransition()
{
    // We do not have a command object, block the signals.
    // Events will not be triggered while we update the values.
    this->comboBoxTransition->blockSignals(true);

    QList<TransitionModel> models = DatabaseManager::getInstance().getTransition();
    foreach (TransitionModel model, models)
        this->comboBoxTransition->addItem(model.getValue());

    this->comboBoxTransition->blockSignals(false);
}

void InspectorStillWidget::loadTween()
{
    // We do not have a command object, block the signals.
    // Events will not be triggered while we update the values.
    this->comboBoxTween->blockSignals(true);

    QList<TweenModel> models = DatabaseManager::getInstance().getTween();
    foreach (TweenModel model, models)
        this->comboBoxTween->addItem(model.getValue());

    this->comboBoxTween->blockSignals(false);
}

void InspectorStillWidget::transitionChanged(QString transition)
{
    this->command->setTransition(transition);
}

void InspectorStillWidget::transitionDurationChanged(int transitionDuration)
{
    this->command->setTransitionDuration(transitionDuration);
}

void InspectorStillWidget::directionChanged(QString direction)
{
    this->command->setDirection(direction);
}

void InspectorStillWidget::tweenChanged(QString tween)
{
    this->command->setTween(tween);
}

void InspectorStillWidget::useAutoChanged(int state)
{
    this->command->setUseAuto((state == Qt::Checked) ? true : false);
}

void InspectorStillWidget::triggerOnNextChanged(int state)
{
    this->command->setTriggerOnNext((state == Qt::Checked) ? true : false);
}

void InspectorStillWidget::autoPlayChanged(int state)
{
    this->command->setAutoPlay((state == Qt::Checked) ? true : false);
}

void InspectorStillWidget::autoLoopChanged(int state)
{
    if (this->command == nullptr)
        return;
    this->command->setAutoLoop(state == Qt::Checked);
}

void InspectorStillWidget::autoLoopDelayChanged(int delay)
{
    if (this->command == nullptr)
        return;
    this->command->setAutoLoopDelay(delay);
}
