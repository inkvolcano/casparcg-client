#pragma once

#include "../Shared.h"
#include "ui_InspectorGroupWidget.h"

#include "Commands/GroupCommand.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Models/LibraryModel.h"

#include <QtCore/QEvent>
#include <QtCore/QObject>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT InspectorGroupWidget : public QWidget, Ui::InspectorGroupWidget
{
    Q_OBJECT

    public:
        explicit InspectorGroupWidget(QWidget* parent = 0);

    private:
        LibraryModel* model;
        GroupCommand* command;
        bool enableOscInputControl;

        QCheckBox* checkBoxAutoLoop = nullptr;
        QSpinBox* spinBoxAutoLoopDelay = nullptr;

        void blockAllSignals(bool block);

        Q_SLOT void notesChanged();
        Q_SLOT void resetNotes(QString);
        Q_SLOT void autoPlayChanged(int);
        Q_SLOT void loopChanged(int);
        Q_SLOT void autoLoopChanged(int);
        Q_SLOT void autoLoopDelayChanged(int);
        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
};
