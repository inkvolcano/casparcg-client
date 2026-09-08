#pragma once

#include "Shared.h"
#include "ui_TriggerBanksPanelWidget.h"

#include "Global.h"

#include "Events/Rundown/BankAssignmentChangedEvent.h"

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

// Which item is bound to which trigger bank, and the B1-B9 indicators.
//
// The smallest of the three panels that used to share one file, and the one with
// the least to say for itself: it reacts to bank assignments and to the display
// setting the Activity panel also honours, and does nothing else.
class WIDGETS_EXPORT TriggerBanksPanelWidget : public QWidget, Ui::TriggerBanksPanelWidget
{
    Q_OBJECT

    public:
        explicit TriggerBanksPanelWidget(QWidget* parent = 0);

        QTabWidget* tabWidget() { return tabWidgetBanks; }

    private:
        bool banksPlaced = true;
        bool banksCollapsed;

        QToolButton* banksMenuButton = nullptr;
        QMenu* banksMenu = nullptr;
        QAction* banksExpandCollapseAction = nullptr;

        bool showBankIcons;

        // Row size follows the same display setting the Activity panel uses, so
        // the two panels do not disagree about how large a row is. Each keeps its
        // own copy rather than sharing one, because both are driven by the same
        // global signal and neither needs to know the other exists.
        bool bigBoldMode = false;

        // Bank icons (B1-B9 indicators)
        static const int BANK_COUNT = 9;
        QLabel* bankIcons[BANK_COUNT];
        QWidget* bankIconsContainer;

        // Bank entries (assigned banks)
        struct BankEntry
        {
            QWidget* row;
            QLabel* labelBank;
            QLabel* labelItem;
            QLabel* labelChannel;
        };

        QVBoxLayout* banksEntriesLayout;
        QMap<int, BankEntry> bankEntries;

        void setupMenus();
        void updatePlacement();

        void setupBanksPanel();
        void updateBankIcons();
        void updateBankDisplay(int bankId);
        void removeBankEntry(int bankId);
        void rebuildAllBankEntries();

        Q_SLOT void toggleBanksCollapse();
        Q_SLOT void bankAssignmentChanged(const BankAssignmentChangedEvent&);
};
