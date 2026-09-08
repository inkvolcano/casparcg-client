#pragma once

#include "../Shared.h"

#include "Commands/AbstractCommand.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Models/LibraryModel.h"

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QWidget>

class GroupCommand;

// "Simple Mode" inspector section: everything that controls how an item appears
// on the Simple Mode button grid. Collapsed by default, like Embedded Transform.
class WIDGETS_EXPORT InspectorSimpleModeWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit InspectorSimpleModeWidget(QWidget* parent = 0);

    private:
        // QPointer: auto-nulls when the item's command is deleted or rebuilt.
        QPointer<AbstractCommand> command;
        LibraryModel* model = nullptr;

        QCheckBox* checkBoxShowAsButton = nullptr;
        QCheckBox* checkBoxNextButton = nullptr;
        QCheckBox* checkBoxGroupInvokes = nullptr;
        QCheckBox* checkBoxTreatAsDropdown = nullptr;

        void blockAllSignals(bool block);
        void resetControls();

        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
        Q_SLOT void showAsButtonChanged(int);
        Q_SLOT void nextButtonChanged(int);
        Q_SLOT void groupInvokesChanged(int);
        Q_SLOT void treatAsDropdownChanged(int);
};
