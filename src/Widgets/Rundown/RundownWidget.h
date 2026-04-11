#pragma once

#include "../Shared.h"
#include "ui_RundownWidget.h"

#include "Global.h"
#include "Playout.h"

#include "Events/Rundown/ActiveRundownChangedEvent.h"
#include "Events/Rundown/AllowRemoteTriggeringEvent.h"
#include "Events/Rundown/LockRundownEvent.h"
#include "Events/Rundown/CompactViewEvent.h"
#include "Events/Rundown/RepositoryRundownEvent.h"
#include "Events/Rundown/CloseRundownEvent.h"
#include "Events/Rundown/DeleteRundownEvent.h"
#include "Events/Rundown/NewRundownEvent.h"
#include "Events/Rundown/NewRundownMenuEvent.h"
#include "Events/Rundown/MarkItemAsUsedEvent.h"
#include "Events/Rundown/MarkItemAsUnusedEvent.h"
#include "Events/Rundown/MarkAllItemsAsUsedEvent.h"
#include "Events/Rundown/MarkAllItemsAsUnusedEvent.h"
#include "Events/Rundown/OpenRundownEvent.h"
#include "Events/Rundown/OpenRundownFromUrlEvent.h"
#include "Events/Rundown/OpenRundownMenuEvent.h"
#include "Events/Rundown/OpenRundownFromUrlMenuEvent.h"
#include "Events/Rundown/ReloadRundownEvent.h"
#include "Events/Rundown/SaveRundownEvent.h"
#include "Events/Rundown/ReloadRundownMenuEvent.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QObject>

#include <QtGui/QKeyEvent>
#include <QtGui/QShortcut>

#include <QtGui/QAction>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTreeWidgetItem>
#include <QtGui/QUndoGroup>
#include <QtWidgets/QWidget>

class RundownTreeWidget;

class WIDGETS_EXPORT RundownWidget : public QWidget, Ui::RundownWidget
{
    Q_OBJECT

    public:
        explicit RundownWidget(QWidget* parent = 0);

        bool checkForSaveBeforeQuit();
        QUndoGroup* undoGroup() const { return m_undoGroup; }

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        QMenu* contextMenuRundownDropdown;
        QMenu* contextMenuMark;
        QMenu* openRecentMenu;

        QAction* newRundownAction;
        QAction* openRundownAction;
        QAction* saveAction;
        QAction* saveAsAction;
        QAction* openRundownFromUrlAction;
        QAction* compactViewAction;
        QAction* allowRemoteTriggeringAction;
        QAction* lockRundownAction;
        bool userOverrideLock = false;
        QAction* insertRepositoryChangesAction;
        QAction* reloadRundownAction;
        QAction* openRecentMenuAction;

        QUndoGroup* m_undoGroup = nullptr;

        // Split view
        QSplitter* splitterRundown;
        QTabWidget* tabWidgetRundownSecondary;
        bool splitViewActive;
        QTabWidget* focusedTabWidget;

        QAction* splitHorizontalAction;
        QAction* splitVerticalAction;
        QAction* closeSplitAction;

        // Double-tap arrow key tracking for pane switching.
        QElapsedTimer lastArrowKeyTimer;
        Qt::Key lastArrowKey;

        // Search bar.
        struct SearchResult {
            QTabWidget* pane;
            RundownTreeWidget* tab;
            QTreeWidgetItem* item;
        };

        QWidget* searchBar = nullptr;
        QLineEdit* searchLineEdit = nullptr;
        QLabel* searchCountLabel = nullptr;
        QPushButton* searchPrevButton = nullptr;
        QPushButton* searchNextButton = nullptr;
        QPushButton* searchCloseButton = nullptr;
        QList<SearchResult> searchResults;
        int searchCurrentIndex = -1;

        void setupMenus();
        void setupSearchBar();
        void navigateToSearchResult(int index);
        void setupTabWidget(QTabWidget* tabWidget);
        void setFocusedPane(QTabWidget* pane);
        QTabWidget* otherPane(QTabWidget* pane);
        int totalTabCount();
        void toggleSplitView(Qt::Orientation orientation);
        void updateSplitMenuState();
        void deleteTabFromPane(QTabWidget* pane, int index);
        void moveTabToOtherPane(QTabWidget* sourcePane, int tabIndex);
        void connectRundownTreeSignals(RundownTreeWidget* widget);

        Q_SLOT void handleCrossTabGateway(const QString& gatewayId, const QString& exitLabel);
        Q_SLOT void handleCrossTabGatewayRelay(const QString& gatewayId, Playout::PlayoutType type, const QString& exitLabel);
        Q_SLOT void handleCrossTabFocusGateway(const QString& gatewayId, bool fromIsExit, const QString& exitLabel);
        Q_SLOT void refreshOpenRecent();
        Q_SLOT void openRecentMenuActionTriggered(QAction*);
        Q_SLOT void clearOpenRecent();
        Q_SLOT void openRundownFromDisk();
        Q_SLOT void openRundownFromRepo();
        Q_SLOT void reloadCurrentRundown();
        Q_SLOT void closeCurrentRundown();
        Q_SLOT void markItemAsUsedInRundown();
        Q_SLOT void markItemAsUnusedInRundown();
        Q_SLOT void markAllItemsAsUsedInRundown();
        Q_SLOT void markAllItemsAsUnusedInRundown();
        Q_SLOT void markItemAsUsed(const MarkItemAsUsedEvent&);
        Q_SLOT void markItemAsUnused(const MarkItemAsUnusedEvent&);
        Q_SLOT void markAllItemsAsUsed(const MarkAllItemsAsUsedEvent&);
        Q_SLOT void markAllItemsAsUnused(const MarkAllItemsAsUnusedEvent&);
        Q_SLOT void createNewRundown();
        Q_SLOT void saveRundownToDisk();
        Q_SLOT void saveAsRundownToDisk();
        Q_SLOT void copyItemProperties();
        Q_SLOT void pasteItemProperties();
        Q_SLOT void pasteItemPropertiesNoData();
        Q_SLOT bool selectTab(int index);
        Q_SLOT void gpiBindingChanged(int, Playout::PlayoutType);
        Q_SLOT void compactView(bool);
        Q_SLOT void remoteTriggering(bool);
        Q_SLOT void lockRundown(bool);
        Q_SLOT void lockRundown(const LockRundownEvent&);
        Q_SLOT void insertRepositoryChanges();
        Q_SLOT void newRundownMenu(const NewRundownMenuEvent&);
        Q_SLOT void openRundownMenu(const OpenRundownMenuEvent&);
        Q_SLOT void openRundownFromUrlMenu(const OpenRundownFromUrlMenuEvent&);
        Q_SLOT void newRundown(const NewRundownEvent&);
        Q_SLOT void compactView(const CompactViewEvent&);
        Q_SLOT void allowRemoteTriggering(const AllowRemoteTriggeringEvent&);
        Q_SLOT void repositoryRundown(const RepositoryRundownEvent&);
        Q_SLOT void closeRundown(const CloseRundownEvent&);
        Q_SLOT void deleteRundown(const DeleteRundownEvent&);
        Q_SLOT void openRundown(const OpenRundownEvent&);
        Q_SLOT void openRundownFromUrl(const OpenRundownFromUrlEvent&);
        Q_SLOT void saveRundown(const SaveRundownEvent&);
        Q_SLOT void activeRundownChanged(const ActiveRundownChangedEvent&);
        Q_SLOT void reloadRundown(const ReloadRundownEvent&);
        Q_SLOT void reloadRundownMenu(const ReloadRundownMenuEvent&);
        Q_SLOT void splitHorizontal();
        Q_SLOT void splitVertical();
        Q_SLOT void closeSplitView();
        Q_SLOT void showSearch();
        Q_SLOT void hideSearch();
        Q_SLOT void searchTextChanged(const QString& text);
        Q_SLOT void searchNext();
        Q_SLOT void searchPrev();
};
