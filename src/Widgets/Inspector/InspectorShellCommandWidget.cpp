#include "InspectorShellCommandWidget.h"

#include "Global.h"

#include "DatabaseManager.h"
#include "EventManager.h"

InspectorShellCommandWidget::InspectorShellCommandWidget(QWidget* parent)
    : QWidget(parent),
      model(NULL), command(NULL)
{
    setupUi(this);

    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)),
                     this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));

    QObject::connect(this->lineEditCommandLine, SIGNAL(textChanged(const QString&)), this, SLOT(commandLineChanged(const QString&)));
    QObject::connect(this->lineEditWorkingDirectory, SIGNAL(textChanged(const QString&)), this, SLOT(workingDirectoryChanged(const QString&)));
    QObject::connect(this->checkBoxTriggerOnNext, SIGNAL(stateChanged(int)), this, SLOT(triggerOnNextChanged(int)));
    QObject::connect(this->checkBoxWaitForFinish, SIGNAL(stateChanged(int)), this, SLOT(waitForFinishChanged(int)));
    QObject::connect(this->spinBoxTimeout, SIGNAL(valueChanged(int)), this, SLOT(timeoutChanged(int)));
}

void InspectorShellCommandWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->command = nullptr;
    this->model = event.getLibraryModel();

    blockAllSignals(true);

    if (dynamic_cast<ShellCommand*>(event.getCommand()))
    {
        this->command = dynamic_cast<ShellCommand*>(event.getCommand());

        this->lineEditCommandLine->setText(this->command->getCommandLine());
        this->lineEditWorkingDirectory->setText(this->command->getWorkingDirectory());
        this->checkBoxTriggerOnNext->setChecked(this->command->getTriggerOnNext());
        this->checkBoxWaitForFinish->setChecked(this->command->getWaitForFinish());
        this->spinBoxTimeout->setValue(this->command->getTimeout());
    }

    // The warning is read every time rather than cached, because the setting can
    // be changed in another window while an item is selected here.
    bool allowed = DatabaseManager::getInstance().getConfigurationByName("AllowShellCommands").getValue() == "true";
    this->labelDisabledWarning->setVisible(!allowed);

    // The timeout only means anything when the rundown is actually waiting.
    bool waiting = this->checkBoxWaitForFinish->isChecked();
    this->labelTimeout->setEnabled(waiting);
    this->spinBoxTimeout->setEnabled(waiting);

    checkEmptyCommandLine();

    blockAllSignals(false);
}

void InspectorShellCommandWidget::checkEmptyCommandLine()
{
    // An item with no command does nothing when it is fired, which is worth
    // showing in the inspector rather than at air time.
    if (this->lineEditCommandLine->text().trimmed().isEmpty())
        this->lineEditCommandLine->setStyleSheet("border-color: firebrick;");
    else
        this->lineEditCommandLine->setStyleSheet("");
}

void InspectorShellCommandWidget::blockAllSignals(bool block)
{
    this->lineEditCommandLine->blockSignals(block);
    this->lineEditWorkingDirectory->blockSignals(block);
    this->checkBoxTriggerOnNext->blockSignals(block);
    this->checkBoxWaitForFinish->blockSignals(block);
    this->spinBoxTimeout->blockSignals(block);
}

void InspectorShellCommandWidget::commandLineChanged(const QString& commandLine)
{
    checkEmptyCommandLine();

    if (this->command.isNull())
        return;

    this->command->setCommandLine(commandLine);
}

void InspectorShellCommandWidget::workingDirectoryChanged(const QString& workingDirectory)
{
    if (this->command.isNull())
        return;

    this->command->setWorkingDirectory(workingDirectory);
}

void InspectorShellCommandWidget::triggerOnNextChanged(int state)
{
    if (this->command.isNull())
        return;

    this->command->setTriggerOnNext(state == Qt::Checked);
}

void InspectorShellCommandWidget::waitForFinishChanged(int state)
{
    bool waiting = (state == Qt::Checked);

    this->labelTimeout->setEnabled(waiting);
    this->spinBoxTimeout->setEnabled(waiting);

    if (this->command.isNull())
        return;

    this->command->setWaitForFinish(waiting);
}

void InspectorShellCommandWidget::timeoutChanged(int timeout)
{
    if (this->command.isNull())
        return;

    this->command->setTimeout(timeout);
}
