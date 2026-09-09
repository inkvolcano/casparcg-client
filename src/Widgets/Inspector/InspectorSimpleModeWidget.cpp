#include "InspectorSimpleModeWidget.h"

#include "EventManager.h"
#include "Commands/GroupCommand.h"
#include "Commands/TemplateCommand.h"

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>

InspectorSimpleModeWidget::InspectorSimpleModeWidget(QWidget* parent)
    : QWidget(parent)
{
    QGridLayout* grid = new QGridLayout(this);
    grid->setContentsMargins(9, 6, 9, 6);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(4);

    auto addLabel = [this, grid](const QString& text, int row) {
        QLabel* label = new QLabel(text, this);
        label->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
        grid->addWidget(label, row, 0);
    };

    addLabel(tr("Button"), 0);

    this->checkBoxShowAsButton = new QCheckBox(tr("Show as button"), this);
    this->checkBoxShowAsButton->setToolTip("Show this item as a button on the Simple Mode grid");
    grid->addWidget(this->checkBoxShowAsButton, 0, 1);

    this->checkBoxNextButton = new QCheckBox(tr("Show next button"), this);
    this->checkBoxNextButton->setToolTip("Add a Next control to this item's button");
    grid->addWidget(this->checkBoxNextButton, 1, 1);

    addLabel(tr("Invokes"), 2);

    this->checkBoxGroupInvokes = new QCheckBox(tr("Show invokes on group button"), this);
    this->checkBoxGroupInvokes->setToolTip(
        "When this template sits inside a group, show its labeled invokes as sub-buttons on the group's button");
    grid->addWidget(this->checkBoxGroupInvokes, 2, 1);

    addLabel(tr("Dropdown"), 3);

    this->checkBoxTreatAsDropdown = new QCheckBox(tr("Treat as dropdown"), this);
    this->checkBoxTreatAsDropdown->setToolTip(
        "Treat this group as a list to choose from: playing the group fires only the selected child");
    grid->addWidget(this->checkBoxTreatAsDropdown, 3, 1);

    addLabel(tr("Shotbox"), 4);

    this->checkBoxTreatAsShotbox = new QCheckBox(tr("Treat as shotbox"), this);
    this->checkBoxTreatAsShotbox->setToolTip(
        "Show this group as one key holding a row per child: the child's label on the\n"
        "left, its own controls on the right.\n\n"
        "The group itself never fires. Each row fires its own child, on that child's\n"
        "channel and layer. Put an item in with the Move button on the grid.");
    grid->addWidget(this->checkBoxTreatAsShotbox, 4, 1);

    grid->setColumnStretch(1, 1);

    resetControls();

    QObject::connect(this->checkBoxShowAsButton, SIGNAL(stateChanged(int)), this, SLOT(showAsButtonChanged(int)));
    QObject::connect(this->checkBoxNextButton, SIGNAL(stateChanged(int)), this, SLOT(nextButtonChanged(int)));
    QObject::connect(this->checkBoxGroupInvokes, SIGNAL(stateChanged(int)), this, SLOT(groupInvokesChanged(int)));
    QObject::connect(this->checkBoxTreatAsDropdown, SIGNAL(stateChanged(int)), this, SLOT(treatAsDropdownChanged(int)));
    QObject::connect(this->checkBoxTreatAsShotbox, SIGNAL(stateChanged(int)), this, SLOT(treatAsShotboxChanged(int)));

    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)),
                     this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));
}

void InspectorSimpleModeWidget::blockAllSignals(bool block)
{
    this->checkBoxShowAsButton->blockSignals(block);
    this->checkBoxNextButton->blockSignals(block);
    this->checkBoxGroupInvokes->blockSignals(block);
    this->checkBoxTreatAsDropdown->blockSignals(block);
    this->checkBoxTreatAsShotbox->blockSignals(block);
}

void InspectorSimpleModeWidget::resetControls()
{
    blockAllSignals(true);

    this->checkBoxShowAsButton->setChecked(false);
    this->checkBoxShowAsButton->setEnabled(false);
    this->checkBoxNextButton->setChecked(false);
    this->checkBoxNextButton->setEnabled(false);
    this->checkBoxGroupInvokes->setChecked(false);
    this->checkBoxGroupInvokes->setEnabled(false);
    this->checkBoxTreatAsDropdown->setChecked(false);
    this->checkBoxTreatAsDropdown->setEnabled(false);
    this->checkBoxTreatAsShotbox->setChecked(false);
    this->checkBoxTreatAsShotbox->setEnabled(false);

    blockAllSignals(false);
}

void InspectorSimpleModeWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->command = nullptr;
    this->model = event.getLibraryModel();

    resetControls();

    if (event.getCommand() == NULL || event.getLibraryModel() == NULL)
        return;

    this->command = event.getCommand();

    blockAllSignals(true);

    // Any item can be a button.
    this->checkBoxShowAsButton->setEnabled(true);
    this->checkBoxShowAsButton->setChecked(this->command->getShowInSimpleMode());
    this->checkBoxNextButton->setEnabled(true);
    this->checkBoxNextButton->setChecked(this->command->getSimpleModeNextButton());

    // Only templates have invokes to lift onto a group's button.
    bool isTemplate = (dynamic_cast<TemplateCommand*>(event.getCommand()) != nullptr);
    this->checkBoxGroupInvokes->setEnabled(isTemplate);
    this->checkBoxGroupInvokes->setChecked(isTemplate && this->command->getSimpleModeGroupInvokes());

    // Only groups can act as a dropdown over their children.
    GroupCommand* groupCommand = dynamic_cast<GroupCommand*>(event.getCommand());
    this->checkBoxTreatAsDropdown->setEnabled(groupCommand != nullptr);
    this->checkBoxTreatAsDropdown->setChecked(groupCommand != nullptr && groupCommand->getTreatAsDropdown());
    this->checkBoxTreatAsShotbox->setEnabled(groupCommand != nullptr);
    this->checkBoxTreatAsShotbox->setChecked(groupCommand != nullptr && groupCommand->getTreatAsShotbox());

    blockAllSignals(false);
}

void InspectorSimpleModeWidget::showAsButtonChanged(int state)
{
    if (this->command.isNull())
        return;

    this->command->setShowInSimpleMode(state == Qt::Checked);
}

void InspectorSimpleModeWidget::nextButtonChanged(int state)
{
    if (this->command.isNull())
        return;

    this->command->setSimpleModeNextButton(state == Qt::Checked);
}

void InspectorSimpleModeWidget::groupInvokesChanged(int state)
{
    if (this->command.isNull())
        return;

    this->command->setSimpleModeGroupInvokes(state == Qt::Checked);
}

void InspectorSimpleModeWidget::treatAsDropdownChanged(int state)
{
    if (this->command.isNull())
        return;

    if (GroupCommand* groupCommand = dynamic_cast<GroupCommand*>(this->command.data()))
    {
        groupCommand->setTreatAsDropdown(state == Qt::Checked);

        // The two are exclusive, and the box that was already ticked has to be
        // seen to untick itself rather than just stop applying.
        if (state == Qt::Checked)
        {
            groupCommand->setTreatAsShotbox(false);
            blockAllSignals(true);
            this->checkBoxTreatAsShotbox->setChecked(false);
            blockAllSignals(false);
        }
    }
}

void InspectorSimpleModeWidget::treatAsShotboxChanged(int state)
{
    if (this->command.isNull())
        return;

    if (GroupCommand* groupCommand = dynamic_cast<GroupCommand*>(this->command.data()))
    {
        // The command's own setter clears the dropdown; this puts the checkbox in
        // step with it.
        groupCommand->setTreatAsShotbox(state == Qt::Checked);

        if (state == Qt::Checked)
        {
            blockAllSignals(true);
            this->checkBoxTreatAsDropdown->setChecked(false);
            blockAllSignals(false);
        }
    }
}
