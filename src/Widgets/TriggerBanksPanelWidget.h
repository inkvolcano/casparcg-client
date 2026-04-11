#pragma once

#include "Shared.h"
#include "ui_TriggerBanksPanelWidget.h"

#include "Global.h"

#include "DatabaseManager.h"
#include "Events/Rundown/BankAssignmentChangedEvent.h"

#include <QtCore/QObject>
#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT TriggerBanksPanelWidget : public QWidget, Ui::TriggerBanksPanelWidget
{
    Q_OBJECT

    public:
        explicit TriggerBanksPanelWidget(QWidget* parent = 0);

    protected:
        bool eventFilter(QObject* obj, QEvent* event) override;

    private:
        bool contentCollapsed;
        bool bigBoldMode = false;

        QLabel* bankLabels[TriggerBank::BANK_COUNT] = {};
        QLabel* itemLabels[TriggerBank::BANK_COUNT] = {};
        QLabel* channelLabels[TriggerBank::BANK_COUNT] = {};

        void buildBankRows();
        void updateBankRow(int bankId);
        void updateHeader();

        Q_SLOT void toggleCollapse();
        Q_SLOT void bankAssignmentChanged(const BankAssignmentChangedEvent&);
        Q_SLOT void bigBoldModeChangedSlot(bool active);
};
