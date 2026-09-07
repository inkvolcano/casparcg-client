#pragma once

#include "../Shared.h"
#include "ui_InspectorShellCommandWidget.h"

#include "Commands/ShellCommand.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Models/LibraryModel.h"

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <QtWidgets/QWidget>

class WIDGETS_EXPORT InspectorShellCommandWidget : public QWidget, Ui::InspectorShellCommandWidget
{
    Q_OBJECT

    public:
        explicit InspectorShellCommandWidget(QWidget* parent = 0);

    private:
        LibraryModel* model;
        // QPointer: auto-nulls when the command's rundown item is deleted or
        // rebuilt, so an edit afterwards can't dereference a dead command.
        QPointer<ShellCommand> command;

        void blockAllSignals(bool block);
        void checkEmptyCommandLine();

        Q_SLOT void commandLineChanged(const QString&);
        Q_SLOT void workingDirectoryChanged(const QString&);
        Q_SLOT void triggerOnNextChanged(int);
        Q_SLOT void waitForFinishChanged(int);
        Q_SLOT void timeoutChanged(int);
        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
};
