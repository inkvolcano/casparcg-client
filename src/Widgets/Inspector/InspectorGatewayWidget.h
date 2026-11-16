#pragma once

#include "../Shared.h"

#include "Commands/GatewayCommand.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Models/LibraryModel.h"

#include <QtCore/QObject>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT InspectorGatewayWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit InspectorGatewayWidget(QWidget* parent = 0);

    private:
        LibraryModel* model = nullptr;
        GatewayCommand* command = nullptr;
        QString itemType;

        QLabel* labelLinkGateway = nullptr;
        QComboBox* comboBoxLinkGateway = nullptr;
        QLabel* labelExitLabel = nullptr;
        QLineEdit* lineEditExitLabel = nullptr;
        QLabel* labelSelectedExit = nullptr;
        QComboBox* comboBoxSelectedExit = nullptr;

        QCheckBox* checkBoxConditionEnabled = nullptr;
        QLabel* labelConditionHeader = nullptr;
        QLabel* labelConditionTime = nullptr;
        QComboBox* comboBoxConditionOperator = nullptr;
        QSpinBox* spinBoxConditionHour = nullptr;
        QSpinBox* spinBoxConditionMinute = nullptr;
        QSpinBox* spinBoxConditionSecond = nullptr;
        QLabel* labelConditionExit = nullptr;
        QComboBox* comboBoxConditionExit = nullptr;
        QLabel* labelConditionElse = nullptr;
        QComboBox* comboBoxConditionElse = nullptr;
        QFrame* conditionSeparator = nullptr;

        void blockAllSignals(bool block);
        void updateConditionVisibility();

        Q_SLOT void linkGatewayChanged(int index);
        Q_SLOT void exitLabelChanged();
        Q_SLOT void selectedExitChanged(const QString& text);
        Q_SLOT void conditionEnabledChanged(int state);
        Q_SLOT void conditionOperatorChanged(int index);
        Q_SLOT void conditionHourChanged(int value);
        Q_SLOT void conditionMinuteChanged(int value);
        Q_SLOT void conditionSecondChanged(int value);
        Q_SLOT void conditionExitChanged(const QString& text);
        Q_SLOT void conditionElseChanged(const QString& text);
        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
};
