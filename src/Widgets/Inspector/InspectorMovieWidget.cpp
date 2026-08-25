#include "InspectorMovieWidget.h"
#include "Rundown/AbstractRundownWidget.h"
#include "Rundown/RundownGroupWidget.h"

#include "Global.h"

#include "DatabaseManager.h"
#include "EventManager.h"
#include "Commands/GroupCommand.h"
#include "Models/DirectionModel.h"
#include "Models/TransitionModel.h"
#include "Models/TweenModel.h"

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

InspectorMovieWidget::InspectorMovieWidget(QWidget* parent)
    : QWidget(parent),
      model(NULL), command(NULL), enableOscInputControl(false)
{
    setupUi(this);

    this->enableOscInputControl = (DatabaseManager::getInstance().getConfigurationByName("EnableOscInputControl").getValue() == "true") ? true : false;

    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));

    loadDirection();
    loadTransition();
    loadTween();

    // Programmatically append Auto-Loop row to the grid layout (row 12).
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
        this->spinBoxAutoLoopDelay->setValue(Movie::DEFAULT_AUTO_LOOP_DELAY);

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

void InspectorMovieWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->command = nullptr;
    this->model = event.getLibraryModel();

    blockAllSignals(true);

    if (dynamic_cast<MovieCommand*>(event.getCommand()))
    {
        this->command = dynamic_cast<MovieCommand*>(event.getCommand());

        this->comboBoxTransition->setCurrentIndex(this->comboBoxTransition->findText(this->command->getTransition()));
        this->spinBoxTransitionDuration->setValue(this->command->getTransitionDuration());
        this->comboBoxTween->setCurrentIndex(this->comboBoxTween->findText(this->command->getTween()));
        this->comboBoxDirection->setCurrentIndex(this->comboBoxDirection->findText(this->command->getDirection()));
        this->spinBoxSeek->setValue(this->command->getSeek());
        this->spinBoxLength->setValue(this->command->getLength());
        this->checkBoxLoop->setChecked(this->command->getLoop());
        this->checkBoxFreezeOnLoad->setChecked(this->command->getFreezeOnLoad());
        this->checkBoxTriggerOnNext->setChecked(this->command->getTriggerOnNext());

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

void InspectorMovieWidget::blockAllSignals(bool block)
{
    this->comboBoxTransition->blockSignals(block);
    this->spinBoxTransitionDuration->blockSignals(block);
    this->comboBoxTween->blockSignals(block);
    this->comboBoxDirection->blockSignals(block);
    this->spinBoxSeek->blockSignals(block);
    this->spinBoxLength->blockSignals(block);
    this->checkBoxLoop->blockSignals(block);
    this->checkBoxFreezeOnLoad->blockSignals(block);
    this->checkBoxTriggerOnNext->blockSignals(block);
    this->checkBoxAutoPlay->blockSignals(block);
    if (this->checkBoxAutoLoop != nullptr)
        this->checkBoxAutoLoop->blockSignals(block);
    if (this->spinBoxAutoLoopDelay != nullptr)
        this->spinBoxAutoLoopDelay->blockSignals(block);
}

void InspectorMovieWidget::loadDirection()
{
    // We do not have a command object, block the signals.
    // Events will not be triggered while we update the values.
    this->comboBoxDirection->blockSignals(true);

    QList<DirectionModel> models = DatabaseManager::getInstance().getDirection();
    foreach (DirectionModel model, models)
        this->comboBoxDirection->addItem(model.getValue());

    this->comboBoxDirection->blockSignals(false);
}

void InspectorMovieWidget::loadTransition()
{
    // We do not have a command object, block the signals.
    // Events will not be triggered while we update the values.
    this->comboBoxTransition->blockSignals(true);

    QList<TransitionModel> models = DatabaseManager::getInstance().getTransition();
    foreach (TransitionModel model, models)
        this->comboBoxTransition->addItem(model.getValue());

    this->comboBoxTransition->blockSignals(false);
}

void InspectorMovieWidget::loadTween()
{
    // We do not have a command object, block the signals.
    // Events will not be triggered while we update the values.
    this->comboBoxTween->blockSignals(true);

    QList<TweenModel> models = DatabaseManager::getInstance().getTween();
    foreach (TweenModel model, models)
        this->comboBoxTween->addItem(model.getValue());

    this->comboBoxTween->blockSignals(false);
}

void InspectorMovieWidget::transitionChanged(QString transition)
{
    this->command->setTransition(transition);
}

void InspectorMovieWidget::transitionDurationChanged(int transitionDuration)
{
    this->command->setTransitionDuration(transitionDuration);
}

void InspectorMovieWidget::directionChanged(QString direction)
{
    this->command->setDirection(direction);
}

void InspectorMovieWidget::tweenChanged(QString tween)
{
    this->command->setTween(tween);
}

void InspectorMovieWidget::loopChanged(int state)
{
    this->command->setLoop((state == Qt::Checked) ? true : false);
}

void InspectorMovieWidget::freezeOnLoadChanged(int state)
{
    this->command->setFreezeOnLoad((state == Qt::Checked) ? true : false);
}

void InspectorMovieWidget::triggerOnNextChanged(int state)
{
    this->command->setTriggerOnNext((state == Qt::Checked) ? true : false);
}

void InspectorMovieWidget::seekChanged(int seek)
{
    this->command->setSeek(seek);
}

void InspectorMovieWidget::lengthChanged(int length)
{
    this->command->setLength(length);
}

void InspectorMovieWidget::autoPlayChanged(int state)
{
    this->command->setAutoPlay((state == Qt::Checked) ? true : false);
}

void InspectorMovieWidget::autoLoopChanged(int state)
{
    if (this->command == nullptr)
        return;
    this->command->setAutoLoop(state == Qt::Checked);
}

void InspectorMovieWidget::autoLoopDelayChanged(int delay)
{
    if (this->command == nullptr)
        return;
    this->command->setAutoLoopDelay(delay);
}
