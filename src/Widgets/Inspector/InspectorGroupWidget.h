#pragma once

#include "../Shared.h"
#include "ui_InspectorGroupWidget.h"

#include "Commands/GroupCommand.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Models/LibraryModel.h"

#include <QtCore/QEvent>
#include <QtCore/QObject>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT InspectorGroupWidget : public QWidget, Ui::InspectorGroupWidget
{
    Q_OBJECT

    public:
        explicit InspectorGroupWidget(QWidget* parent = 0);

    private:
        LibraryModel* model = nullptr;
        GroupCommand* command = nullptr;
        bool enableOscInputControl = false;

        QCheckBox* checkBoxAutoLoop = nullptr;
        QSpinBox* spinBoxAutoLoopDelay = nullptr;

        // Watch folder (build 317).
        QCheckBox* checkBoxWatch = nullptr;
        QComboBox* comboWatchServer = nullptr;
        QComboBox* comboWatchFolder = nullptr;
        QSpinBox* spinWatchChannel = nullptr;
        QSpinBox* spinWatchLayer = nullptr;
        QCheckBox* checkBoxWatchExisting = nullptr;
        QLabel* labelWatchStatus = nullptr;
        void buildWatchRows(QGridLayout* grid);
        void loadWatch();
        void fillWatchFolders();
        void watchChanged();
        void showWatchStatus();

        void blockAllSignals(bool block);

        Q_SLOT void notesChanged();
        Q_SLOT void resetNotes(QString);
        Q_SLOT void autoPlayChanged(int);
        Q_SLOT void loopChanged(int);
        Q_SLOT void autoLoopChanged(int);
        Q_SLOT void autoLoopDelayChanged(int);
        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
};
