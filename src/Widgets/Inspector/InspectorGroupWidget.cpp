#include "InspectorGroupWidget.h"

#include "Global.h"

#include "DatabaseManager.h"
#include "EventManager.h"
#include "Events/PreviewEvent.h"
#include "Events/Inspector/AutoPlayChangedEvent.h"
#include "Models/TweenModel.h"

#include <QtCore/QDebug>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

InspectorGroupWidget::InspectorGroupWidget(QWidget* parent)
    : QWidget(parent),
      model(NULL), command(NULL), enableOscInputControl(true)
{
    setupUi(this);

    this->enableOscInputControl = (DatabaseManager::getInstance().getConfigurationByName("EnableOscInputControl").getValue() == "true") ? true : false;

    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));

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
        this->spinBoxAutoLoopDelay->setValue(Group::DEFAULT_AUTO_LOOP_DELAY);

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

void InspectorGroupWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->command = nullptr;
    this->model = event.getLibraryModel();

    blockAllSignals(true);

    if (dynamic_cast<GroupCommand*>(event.getCommand()))
    {
        this->command = dynamic_cast<GroupCommand*>(event.getCommand());

        this->plainTextEditNotes->setPlainText(this->command->getNotes());

        if (this->enableOscInputControl)
        {
            this->labelAutoPlay->setVisible(true);
            this->checkBoxAutoPlay->setVisible(true);
            this->checkBoxAutoPlay->setChecked(this->command->getAutoPlay());

            this->labelLoop->setVisible(true);
            this->checkBoxLoop->setVisible(true);
            this->checkBoxLoop->setChecked(this->command->getLoop());
        }
        else
        {
            this->labelAutoPlay->setVisible(false);
            this->checkBoxAutoPlay->setVisible(false);
            this->checkBoxAutoPlay->setChecked(false);

            this->labelLoop->setVisible(false);
            this->checkBoxLoop->setVisible(false);
            this->checkBoxLoop->setChecked(false);
        }

        if (this->checkBoxAutoLoop != nullptr)
            this->checkBoxAutoLoop->setChecked(this->command->getAutoLoop());
        if (this->spinBoxAutoLoopDelay != nullptr)
            this->spinBoxAutoLoopDelay->setValue(this->command->getAutoLoopDelay());
    }

    blockAllSignals(false);
}

void InspectorGroupWidget::blockAllSignals(bool block)
{
    this->plainTextEditNotes->blockSignals(block);
    this->checkBoxAutoPlay->blockSignals(block);
    this->checkBoxLoop->blockSignals(block);
    if (this->checkBoxAutoLoop != nullptr)
        this->checkBoxAutoLoop->blockSignals(block);
    if (this->spinBoxAutoLoopDelay != nullptr)
        this->spinBoxAutoLoopDelay->blockSignals(block);
}

void InspectorGroupWidget::notesChanged()
{
    this->command->setNotes(this->plainTextEditNotes->toPlainText());
}

void InspectorGroupWidget::resetNotes(QString note)
{
    Q_UNUSED(note);

    this->plainTextEditNotes->setPlainText(Group::DEFAULT_NOTE);
    this->command->setNotes(this->plainTextEditNotes->toPlainText());
}

void InspectorGroupWidget::autoPlayChanged(int state)
{
    this->command->setAutoPlay((state == Qt::Checked) ? true : false);

    EventManager::getInstance().fireAutoPlayChangedEvent(AutoPlayChangedEvent(this->checkBoxAutoPlay->isChecked()));
}

void InspectorGroupWidget::loopChanged(int state)
{
    this->command->setLoop((state == Qt::Checked) ? true : false);
}

void InspectorGroupWidget::autoLoopChanged(int state)
{
    if (this->command == nullptr)
        return;
    this->command->setAutoLoop(state == Qt::Checked);
}

void InspectorGroupWidget::autoLoopDelayChanged(int delay)
{
    if (this->command == nullptr)
        return;
    this->command->setAutoLoopDelay(delay);
}
