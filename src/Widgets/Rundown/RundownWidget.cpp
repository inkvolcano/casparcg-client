#include "RundownWidget.h"
#include "RundownTreeWidget.h"
#include "AbstractRundownWidget.h"
#include "OpenRundownFromUrlDialog.h"

#include "EventManager.h"
#include "DatabaseManager.h"
#include "Events/Rundown/CompactViewEvent.h"
#include "Events/Rundown/CopyItemPropertiesEvent.h"
#include "Events/Rundown/PasteItemPropertiesEvent.h"
#include "Events/Rundown/AllowRemoteTriggeringEvent.h"
#include "Events/Rundown/InsertRepositoryChangesEvent.h"

#include "AutoSaveNaming.h"

#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>
#include <QtCore/QUuid>

#include <QtCore/QMimeData>

#include <QtGui/QDrag>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QIcon>
#include <QtGui/QMouseEvent>

#include <QtWidgets/QApplication>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTabBar>

RundownWidget::RundownWidget(QWidget* parent)
    : QWidget(parent),
      splitViewActive(false), tabWidgetRundownSecondary(nullptr), focusedTabWidget(nullptr),
      splitHorizontalAction(nullptr), splitVerticalAction(nullptr), closeSplitAction(nullptr),
      lastArrowKey(Qt::Key_unknown)
{
    setupUi(this);

    this->m_undoGroup = new QUndoGroup(this);

    // Wrap the tab widget in a QSplitter so we can add a second pane later.
    this->splitterRundown = new QSplitter(Qt::Horizontal, this);
    this->verticalLayout->removeWidget(this->tabWidgetRundown);
    this->splitterRundown->addWidget(this->tabWidgetRundown);
    this->verticalLayout->addWidget(this->splitterRundown, 1); // stretch=1: splitter takes all remaining space

    this->focusedTabWidget = this->tabWidgetRundown;
    this->tabWidgetRundown->setProperty("focused", true);

    setupMenus();
    setupSearchBar();
    setupTabWidget(this->tabWidgetRundown);

    // Create the initial default rundown tab.
    RundownTreeWidget* widget = new RundownTreeWidget(this);
    connectRundownTreeSignals(widget);
    int index = this->tabWidgetRundown->addTab(widget, Rundown::DEFAULT_NAME);
    this->tabWidgetRundown->setTabToolTip(index, Rundown::DEFAULT_NAME);
    this->tabWidgetRundown->setCurrentIndex(index);

    // Tab cycling shortcuts.
    QAction* nextTabAction = new QAction(this);
    nextTabAction->setShortcut(QKeySequence("Ctrl+]"));
    nextTabAction->setShortcutContext(Qt::ApplicationShortcut);
    this->addAction(nextTabAction);
    QObject::connect(nextTabAction, &QAction::triggered, [this]() {
        if (this->focusedTabWidget == nullptr || this->focusedTabWidget->count() <= 1)
            return;
        int next = (this->focusedTabWidget->currentIndex() + 1) % this->focusedTabWidget->count();
        this->focusedTabWidget->setCurrentIndex(next);
    });

    QAction* previousTabAction = new QAction(this);
    previousTabAction->setShortcut(QKeySequence("Ctrl+["));
    previousTabAction->setShortcutContext(Qt::ApplicationShortcut);
    this->addAction(previousTabAction);
    QObject::connect(previousTabAction, &QAction::triggered, [this]() {
        if (this->focusedTabWidget == nullptr || this->focusedTabWidget->count() <= 1)
            return;
        int count = this->focusedTabWidget->count();
        int prev = (this->focusedTabWidget->currentIndex() - 1 + count) % count;
        this->focusedTabWidget->setCurrentIndex(prev);
    });

    // Install application-level event filter for focus tracking in split view.
    qApp->installEventFilter(this);

    // Register gateway entrance provider (scans all panes for cross-tab support).
    EventManager::getInstance().setGatewayEntranceProvider([this](const QString& type) -> QList<QPair<QString, QString>> {
        QList<QPair<QString, QString>> result;
        auto scanPane = [&](QTabWidget* pane) {
            if (pane == nullptr) return;
            for (int t = 0; t < pane->count(); t++)
            {
                RundownTreeWidget* tab = dynamic_cast<RundownTreeWidget*>(pane->widget(t));
                if (tab == nullptr) continue;
                QList<QPair<QString, QString>> entrances = tab->getGatewayEntrances(type);
                result.append(entrances);
            }
        };
        scanPane(this->tabWidgetRundown);
        scanPane(this->tabWidgetRundownSecondary);
        return result;
    });

    // Register gateway exit label provider (scans all panes for cross-tab support).
    EventManager::getInstance().setGatewayExitLabelProvider([this](const QString& gatewayId) -> QStringList {
        QStringList labels;
        auto scanPane = [&](QTabWidget* pane) {
            if (pane == nullptr) return;
            for (int t = 0; t < pane->count(); t++)
            {
                RundownTreeWidget* tab = dynamic_cast<RundownTreeWidget*>(pane->widget(t));
                if (tab == nullptr) continue;
                labels.append(tab->getGatewayExitLabels(gatewayId));
            }
        };
        scanPane(this->tabWidgetRundown);
        scanPane(this->tabWidgetRundownSecondary);
        return labels;
    });

    QObject::connect(&EventManager::getInstance(), SIGNAL(newRundownMenu(const NewRundownMenuEvent&)), this, SLOT(newRundownMenu(const NewRundownMenuEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(openRundownMenu(const OpenRundownMenuEvent&)), this, SLOT(openRundownMenu(const OpenRundownMenuEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(openRundownFromUrlMenu(const OpenRundownFromUrlMenuEvent&)), this, SLOT(openRundownFromUrlMenu(const OpenRundownFromUrlMenuEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(newRundown(const NewRundownEvent&)), this, SLOT(newRundown(const NewRundownEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(compactView(const CompactViewEvent&)), this, SLOT(compactView(const CompactViewEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(allowRemoteTriggering(const AllowRemoteTriggeringEvent&)), this, SLOT(allowRemoteTriggering(const AllowRemoteTriggeringEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(lockRundown(const LockRundownEvent&)), this, SLOT(lockRundown(const LockRundownEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(repositoryRundown(const RepositoryRundownEvent&)), this, SLOT(repositoryRundown(const RepositoryRundownEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(closeRundown(const CloseRundownEvent&)), this, SLOT(closeRundown(const CloseRundownEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(deleteRundown(const DeleteRundownEvent&)), this, SLOT(deleteRundown(const DeleteRundownEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(openRundown(const OpenRundownEvent&)), this, SLOT(openRundown(const OpenRundownEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(openRundownFromUrl(const OpenRundownFromUrlEvent&)), this, SLOT(openRundownFromUrl(const OpenRundownFromUrlEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(saveRundown(const SaveRundownEvent&)), this, SLOT(saveRundown(const SaveRundownEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(activeRundownChanged(const ActiveRundownChangedEvent&)), this, SLOT(activeRundownChanged(const ActiveRundownChangedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(reloadRundown(const ReloadRundownEvent&)), this, SLOT(reloadRundown(const ReloadRundownEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(markItemAsUsed(const MarkItemAsUsedEvent&)), this, SLOT(markItemAsUsed(const MarkItemAsUsedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(markItemAsUnused(const MarkItemAsUnusedEvent&)), this, SLOT(markItemAsUnused(const MarkItemAsUnusedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(markAllItemsAsUsed(const MarkAllItemsAsUsedEvent&)), this, SLOT(markAllItemsAsUsed(const MarkAllItemsAsUsedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(markAllItemsAsUnused(const MarkAllItemsAsUnusedEvent&)), this, SLOT(markAllItemsAsUnused(const MarkAllItemsAsUnusedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(reloadRundownMenu(const ReloadRundownMenuEvent&)), this, SLOT(reloadRundownMenu(const ReloadRundownMenuEvent&)));

    this->autoSaveTimer = new QTimer(this);
    QObject::connect(this->autoSaveTimer, SIGNAL(timeout()), this, SLOT(autoSaveTick()));
    applyAutoSaveSettings();
}

QString RundownWidget::autoSaveDirectory()
{
    return QString("%1/.CasparCG/Client/AutoSave").arg(QDir::homePath());
}

bool RundownWidget::autoSaveEnabled()
{
    // On unless it has been turned off. A recovery copy costs nothing and never
    // touches the rundown's own file, so the safe default is the helpful one.
    return DatabaseManager::getInstance().getConfigurationByName("AutoSaveEnabled").getValue() != "false";
}

int RundownWidget::autoSaveMinutes()
{
    int minutes = DatabaseManager::getInstance().getConfigurationByName("AutoSaveMinutes").getValue().toInt();
    if (minutes < 1)
        minutes = 3;
    if (minutes > 60)
        minutes = 60;

    return minutes;
}

void RundownWidget::applyAutoSaveSettings()
{
    if (this->autoSaveTimer == nullptr)
        return;

    if (!autoSaveEnabled())
    {
        this->autoSaveTimer->stop();

        // Turning it off leaves nothing behind to be offered at the next launch.
        clearAutoSaves();
        return;
    }

    this->autoSaveTimer->start(autoSaveMinutes() * 60 * 1000);
}

QStringList RundownWidget::pendingAutoSaves()
{
    QDir directory(autoSaveDirectory());
    if (!directory.exists())
        return QStringList();

    QStringList paths;
    foreach (const QString& name, directory.entryList(QStringList("*.xml"), QDir::Files, QDir::Name))
        paths.append(directory.filePath(name));

    return paths;
}

QString RundownWidget::autoSaveOriginalPath(const QString& autoSavePath)
{
    QFile file(autoSavePath);
    if (!file.open(QFile::ReadOnly))
        return QString();

    // The marker is the first line, so there is no reason to read a whole
    // rundown to answer this.
    QByteArray firstLine = file.readLine(4096);
    file.close();

    return AutoSaveNaming::originalPathFromLine(firstLine);
}

void RundownWidget::clearAutoSaves()
{
    QDir directory(autoSaveDirectory());
    if (!directory.exists())
        return;

    // .part files are half-written copies from a crash mid-write; they go too.
    foreach (const QString& name, directory.entryList(QStringList() << "*.xml" << "*.xml.part", QDir::Files))
        directory.remove(name);
}

void RundownWidget::autoSaveTick()
{
    if (!autoSaveEnabled())
        return;

    QString directory = autoSaveDirectory();

    QList<QTabWidget*> panes;
    panes << this->tabWidgetRundown;
    if (this->tabWidgetRundownSecondary != nullptr)
        panes << this->tabWidgetRundownSecondary;

    int written = 0;
    for (QTabWidget* pane : panes)
    {
        for (int i = 0; i < pane->count(); i++)
        {
            RundownTreeWidget* tab = dynamic_cast<RundownTreeWidget*>(pane->widget(i));
            if (tab == nullptr)
                continue;

            if (tab->writeAutoSaveCopy(directory))
                written++;
        }
    }

    // Only say something when something happened. A tick that found every rundown
    // already saved should not be putting messages in front of an operator.
    if (written > 0)
    {
        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent(QString("Auto-saved %1 rundown%2").arg(written).arg(written == 1 ? "" : "s")));
    }
}

void RundownWidget::setupTabWidget(QTabWidget* tabWidget)
{
    tabWidget->setTabsClosable(true);
    tabWidget->setMovable(true);

    // So a tab dragged across from the other pane can land here - on the bar,
    // or anywhere on the pane. The drop itself is handled in eventFilter.
    tabWidget->setAcceptDrops(true);
    tabWidget->tabBar()->setAcceptDrops(true);

    // Connect signals via lambdas so we always know which pane fired.
    QObject::connect(tabWidget, &QTabWidget::currentChanged, [this, tabWidget](int index) {
        if (index < 0)
            return;

        // Deactivate all tabs in this pane.
        for (int i = 0; i < tabWidget->count(); i++)
            dynamic_cast<RundownTreeWidget*>(tabWidget->widget(i))->setActive(false);

        // Only activate if this is the focused pane.
        if (tabWidget == this->focusedTabWidget)
        {
            dynamic_cast<RundownTreeWidget*>(tabWidget->widget(index))->setActive(true);
            this->m_undoGroup->setActiveStack(dynamic_cast<RundownTreeWidget*>(tabWidget->widget(index))->undoStack());

            bool allowRemoteTriggering = dynamic_cast<RundownTreeWidget*>(tabWidget->widget(index))->getAllowRemoteTriggering();
            this->allowRemoteTriggeringAction->blockSignals(true);
            this->allowRemoteTriggeringAction->setChecked(allowRemoteTriggering);
            this->allowRemoteTriggeringAction->blockSignals(false);

            bool locked = dynamic_cast<RundownTreeWidget*>(tabWidget->widget(index))->isLocked();
            this->lockRundownAction->blockSignals(true);
            this->lockRundownAction->setChecked(locked);
            this->lockRundownAction->blockSignals(false);
        }
    });

    QObject::connect(tabWidget, &QTabWidget::tabCloseRequested, [this, tabWidget](int index) {
        // If this is the last tab in the secondary pane, close the split instead.
        if (tabWidget == this->tabWidgetRundownSecondary && tabWidget->count() <= 1)
        {
            closeSplitView();
            return;
        }

        // If this is the last tab in the primary pane and secondary exists, close the split.
        if (tabWidget == this->tabWidgetRundown && tabWidget->count() <= 1 && this->tabWidgetRundownSecondary != nullptr)
        {
            closeSplitView();
            return;
        }

        // Minimum 1 tab overall.
        if (totalTabCount() <= 1)
            return;

        deleteTabFromPane(tabWidget, index);
    });

    // Corner widget (QToolButton with dropdown menu).
    QToolButton* cornerButton = new QToolButton(tabWidget);
    cornerButton->setObjectName("toolButtonRundownDropdown");
    cornerButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    cornerButton->setFixedSize(22, 22);
    cornerButton->setMenu(this->contextMenuRundownDropdown);
    cornerButton->setPopupMode(QToolButton::InstantPopup);
    tabWidget->setCornerWidget(cornerButton);

    // Tab bar context menu for reordering and cross-pane moves.
    tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(tabWidget->tabBar(), &QWidget::customContextMenuRequested, [this, tabWidget](const QPoint& pos) {
        int tabIndex = tabWidget->tabBar()->tabAt(pos);
        if (tabIndex < 0)
            return;

        QMenu menu;
        QAction* moveLeftAction = nullptr;
        QAction* moveRightAction = nullptr;
        int tabCount = tabWidget->count();

        if (tabCount > 1)
        {
            if (tabIndex > 0)
                moveLeftAction = menu.addAction("Move Left");
            if (tabIndex < tabCount - 1)
                moveRightAction = menu.addAction("Move Right");
        }

        QAction* moveToOtherPaneAction = nullptr;
        if (this->splitViewActive)
        {
            if (!menu.isEmpty())
                menu.addSeparator();
            moveToOtherPaneAction = menu.addAction("Move to Other Pane");
        }

        if (menu.isEmpty())
            return;

        QAction* result = menu.exec(tabWidget->tabBar()->mapToGlobal(pos));
        if (result == nullptr)
            return;

        if (result == moveLeftAction)
            tabWidget->tabBar()->moveTab(tabIndex, tabIndex - 1);
        else if (result == moveRightAction)
            tabWidget->tabBar()->moveTab(tabIndex, tabIndex + 1);
        else if (result == moveToOtherPaneAction)
            this->moveTabToOtherPane(tabWidget, tabIndex);
    });
}

bool RundownWidget::eventFilter(QObject* watched, QEvent* event)
{
    // Dragging a rundown's tab to the other pane.
    if (this->splitViewActive && this->tabWidgetRundownSecondary != nullptr)
    {
        QWidget* w = qobject_cast<QWidget*>(watched);
        const bool onABar = (w != nullptr)
            && (w == this->tabWidgetRundown->tabBar() || w == this->tabWidgetRundownSecondary->tabBar());

        if (event->type() == QEvent::MouseButtonPress && onABar)
        {
            QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
            QTabBar* bar = static_cast<QTabBar*>(w);

            if (mouse->button() == Qt::LeftButton && bar->tabAt(mouse->pos()) >= 0)
            {
                this->tabDragBar = bar;
                this->tabDragIndex = bar->tabAt(mouse->pos());
                this->tabDragStart = mouse->pos();
            }
        }
        else if (event->type() == QEvent::MouseMove && w != nullptr && w == this->tabDragBar && this->tabDragIndex >= 0)
        {
            // Inside the bar QTabBar reorders on its own. Out of it, with a tab
            // in hand, is where its move stops making sense and ours starts.
            QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
            if ((mouse->buttons() & Qt::LeftButton)
                && !this->tabDragBar->rect().contains(mouse->pos())
                && (mouse->pos() - this->tabDragStart).manhattanLength() >= QApplication::startDragDistance())
            {
                startTabDrag(this->tabDragBar, mouse);
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease && w != nullptr && w == this->tabDragBar)
        {
            this->tabDragBar = nullptr;
            this->tabDragIndex = -1;
        }
        else if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove || event->type() == QEvent::Drop)
        {
            // QDragEnterEvent and QDragMoveEvent are both QDropEvents.
            QDropEvent* drop = static_cast<QDropEvent*>(event);

            if (drop->mimeData()->hasFormat("application/rundown-tab"))
            {
                const QStringList parts = QString::fromUtf8(drop->mimeData()->data("application/rundown-tab")).split(':');
                QTabWidget* source = (parts.value(0) == "secondary") ? this->tabWidgetRundownSecondary
                                                                       : this->tabWidgetRundown;
                QTabWidget* target = paneOf(w);

                // Only the other pane takes it. Over its own pane, or anything
                // else, the cursor says no and letting go does nothing.
                if (target == nullptr || target == source)
                {
                    drop->ignore();
                    return true;
                }

                if (event->type() == QEvent::Drop)
                {
                    moveTabToOtherPane(source, parts.value(1).toInt());

                    // Moving the last tab out closes the split, and the panes
                    // with it; only follow the tab while there are still two.
                    if (this->splitViewActive)
                        setFocusedPane(target);
                }

                drop->acceptProposedAction();
                return true;
            }
        }
    }

    if (event->type() == QEvent::MouseButtonPress && this->splitViewActive && this->tabWidgetRundownSecondary != nullptr)
    {
        QWidget* w = qobject_cast<QWidget*>(watched);
        if (w != nullptr)
        {
            // Walk up the parent chain to find which pane this widget belongs to.
            QWidget* parent = w;
            while (parent != nullptr)
            {
                if (parent == this->tabWidgetRundown)
                {
                    setFocusedPane(this->tabWidgetRundown);
                    break;
                }
                if (parent == this->tabWidgetRundownSecondary)
                {
                    setFocusedPane(this->tabWidgetRundownSecondary);
                    break;
                }
                parent = parent->parentWidget();
            }
        }
    }

    // Double-tap Left/Right arrow to move the selection indicator between panes.
    if (event->type() == QEvent::KeyPress && this->splitViewActive && this->tabWidgetRundownSecondary != nullptr)
    {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        Qt::Key key = static_cast<Qt::Key>(keyEvent->key());

        if (key == Qt::Key_Left || key == Qt::Key_Right)
        {
            if (key == this->lastArrowKey && this->lastArrowKeyTimer.isValid() && this->lastArrowKeyTimer.elapsed() < 400)
            {
                // Double-tap detected: switch to the other pane.
                setFocusedPane(otherPane(this->focusedTabWidget));
                this->lastArrowKey = Qt::Key_unknown;
                this->lastArrowKeyTimer.invalidate();
                return true; // Consume the event.
            }

            this->lastArrowKey = key;
            this->lastArrowKeyTimer.start();
        }
        else
        {
            // Any other key resets the double-tap tracking.
            this->lastArrowKey = Qt::Key_unknown;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void RundownWidget::setFocusedPane(QTabWidget* pane)
{
    if (pane == nullptr || pane == this->focusedTabWidget)
        return;

    QTabWidget* oldPane = this->focusedTabWidget;

    // Deactivate all tabs in the old focused pane.
    if (oldPane != nullptr)
    {
        for (int i = 0; i < oldPane->count(); i++)
            dynamic_cast<RundownTreeWidget*>(oldPane->widget(i))->setActive(false);

        oldPane->setProperty("focused", false);
        oldPane->style()->unpolish(oldPane);
        oldPane->style()->polish(oldPane);
    }

    this->focusedTabWidget = pane;

    // Activate the current tab in the new focused pane.
    if (pane->count() > 0 && pane->currentIndex() >= 0)
    {
        RundownTreeWidget* currentTab = dynamic_cast<RundownTreeWidget*>(pane->currentWidget());
        currentTab->setActive(true);

        bool allowRemoteTriggering = currentTab->getAllowRemoteTriggering();
        this->allowRemoteTriggeringAction->blockSignals(true);
        this->allowRemoteTriggeringAction->setChecked(allowRemoteTriggering);
        this->allowRemoteTriggeringAction->blockSignals(false);

        bool locked = currentTab->isLocked();
        this->lockRundownAction->blockSignals(true);
        this->lockRundownAction->setChecked(locked);
        this->lockRundownAction->blockSignals(false);

        // Update undo group to track the active tab's undo stack.
        this->m_undoGroup->setActiveStack(currentTab->undoStack());

        // Set Qt keyboard focus to the tree widget so arrow keys work in the correct pane.
        QWidget* tree = pane->currentWidget()->findChild<QWidget*>("treeWidgetRundown");
        if (tree != nullptr)
            tree->setFocus();
    }

    pane->setProperty("focused", true);
    pane->style()->unpolish(pane);
    pane->style()->polish(pane);
}

QTabWidget* RundownWidget::otherPane(QTabWidget* pane)
{
    if (pane == this->tabWidgetRundown)
        return this->tabWidgetRundownSecondary;
    return this->tabWidgetRundown;
}

int RundownWidget::totalTabCount()
{
    int count = this->tabWidgetRundown->count();
    if (this->tabWidgetRundownSecondary != nullptr)
        count += this->tabWidgetRundownSecondary->count();
    return count;
}

void RundownWidget::splitHorizontal()
{
    toggleSplitView(Qt::Horizontal);
}

void RundownWidget::splitVertical()
{
    toggleSplitView(Qt::Vertical);
}

void RundownWidget::toggleSplitView(Qt::Orientation orientation)
{
    if (this->splitViewActive)
    {
        if (this->splitterRundown->orientation() == orientation)
        {
            // Same orientation: toggle off (close the split).
            closeSplitView();
        }
        else
        {
            // Different orientation: just switch the direction.
            this->splitterRundown->setOrientation(orientation);
        }
        return;
    }

    // Enable split view.
    this->tabWidgetRundownSecondary = new QTabWidget(this);
    this->tabWidgetRundownSecondary->setObjectName("tabWidgetRundownSecondary");
    setupTabWidget(this->tabWidgetRundownSecondary);
    this->tabWidgetRundownSecondary->setProperty("focused", false);

    this->splitterRundown->setOrientation(orientation);
    this->splitterRundown->addWidget(this->tabWidgetRundownSecondary);

    // If primary has 2+ tabs, move the tab right of current into the secondary pane.
    if (this->tabWidgetRundown->count() >= 2)
    {
        int moveIndex = this->tabWidgetRundown->currentIndex() + 1;
        if (moveIndex >= this->tabWidgetRundown->count())
            moveIndex = this->tabWidgetRundown->count() - 1;

        QWidget* widget = this->tabWidgetRundown->widget(moveIndex);
        QString text = this->tabWidgetRundown->tabText(moveIndex);
        QString tooltip = this->tabWidgetRundown->tabToolTip(moveIndex);
        QIcon icon = this->tabWidgetRundown->tabIcon(moveIndex);

        this->tabWidgetRundown->removeTab(moveIndex);
        int newIndex = this->tabWidgetRundownSecondary->addTab(widget, icon, text);
        this->tabWidgetRundownSecondary->setTabToolTip(newIndex, tooltip);
        this->tabWidgetRundownSecondary->setCurrentIndex(newIndex);
    }
    else
    {
        // Only 1 tab in primary: create a new empty rundown in secondary.
        RundownTreeWidget* newWidget = new RundownTreeWidget(this);
        connectRundownTreeSignals(newWidget);
        int index = this->tabWidgetRundownSecondary->addTab(newWidget, Rundown::DEFAULT_NAME);
        this->tabWidgetRundownSecondary->setTabToolTip(index, Rundown::DEFAULT_NAME);
        this->tabWidgetRundownSecondary->setCurrentIndex(index);
    }

    // Set equal splitter sizes.
    int totalSize = (orientation == Qt::Horizontal)
        ? this->splitterRundown->width()
        : this->splitterRundown->height();
    this->splitterRundown->setSizes(QList<int>() << totalSize / 2 << totalSize / 2);

    this->splitViewActive = true;
    updateSplitMenuState();

    // Refresh cross-tab search results since tabs moved between panes.
    if (this->searchBar != nullptr && this->searchBar->isVisible() && !this->searchLineEdit->text().isEmpty())
        searchTextChanged(this->searchLineEdit->text());
}

void RundownWidget::closeSplitView()
{
    if (!this->splitViewActive || this->tabWidgetRundownSecondary == nullptr)
        return;

    // Move all tabs from secondary back to primary.
    while (this->tabWidgetRundownSecondary->count() > 0)
    {
        QWidget* widget = this->tabWidgetRundownSecondary->widget(0);
        QString text = this->tabWidgetRundownSecondary->tabText(0);
        QString tooltip = this->tabWidgetRundownSecondary->tabToolTip(0);
        QIcon icon = this->tabWidgetRundownSecondary->tabIcon(0);

        this->tabWidgetRundownSecondary->removeTab(0);
        int newIndex = this->tabWidgetRundown->addTab(widget, icon, text);
        this->tabWidgetRundown->setTabToolTip(newIndex, tooltip);
    }

    // Reset focus BEFORE destroying the secondary pane so nothing accesses the
    // stale pointer. Use deleteLater() because this can be called from within
    // the secondary tab widget's own tabCloseRequested signal handler.
    this->focusedTabWidget = this->tabWidgetRundown;
    this->splitViewActive = false;

    this->tabWidgetRundownSecondary->deleteLater();
    this->tabWidgetRundownSecondary = nullptr;

    // Ensure the current tab in primary is activated.
    if (this->tabWidgetRundown->count() > 0 && this->tabWidgetRundown->currentIndex() >= 0)
        dynamic_cast<RundownTreeWidget*>(this->tabWidgetRundown->currentWidget())->setActive(true);

    this->tabWidgetRundown->setProperty("focused", true);
    this->tabWidgetRundown->style()->unpolish(this->tabWidgetRundown);
    this->tabWidgetRundown->style()->polish(this->tabWidgetRundown);

    updateSplitMenuState();

    // Refresh cross-tab search results since pane structure changed.
    if (this->searchBar != nullptr && this->searchBar->isVisible() && !this->searchLineEdit->text().isEmpty())
        searchTextChanged(this->searchLineEdit->text());
}

void RundownWidget::updateSplitMenuState()
{
    if (this->splitHorizontalAction != nullptr)
        this->splitHorizontalAction->setVisible(!this->splitViewActive);
    if (this->splitVerticalAction != nullptr)
        this->splitVerticalAction->setVisible(!this->splitViewActive);
    if (this->closeSplitAction != nullptr)
        this->closeSplitAction->setVisible(this->splitViewActive);
}

void RundownWidget::deleteTabFromPane(QTabWidget* pane, int index)
{
    if (pane == nullptr || index < 0 || index >= pane->count())
        return;

    if (dynamic_cast<RundownTreeWidget*>(pane->widget(index))->checkForSave())
    {
        QMessageBox box(this);
        box.setWindowTitle("Close Rundown");
        box.setWindowIcon(QIcon(":/Graphics/Images/CasparCG.png"));
        box.setText(QString("You have unsaved changes in your %1 rundown. Do you want to save before you close?").arg(pane->tabText(index)));
        box.setIconPixmap(QPixmap(":/Graphics/Images/Attention.png"));
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        box.buttons().at(0)->setIcon(QIcon());
        box.buttons().at(0)->setFocusPolicy(Qt::NoFocus);
        box.buttons().at(1)->setIcon(QIcon());
        box.buttons().at(1)->setFocusPolicy(Qt::NoFocus);
        box.buttons().at(2)->setIcon(QIcon());
        box.buttons().at(2)->setFocusPolicy(Qt::NoFocus);

        int result = box.exec();
        if (result == QMessageBox::Yes)
            dynamic_cast<RundownTreeWidget*>(pane->widget(index))->saveRundown(false);
        else if (result == QMessageBox::Cancel)
            return;
    }

    delete pane->widget(index);

    if (totalTabCount() <= Rundown::MAX_NUMBER_OF_RUNDONWS)
    {
        EventManager::getInstance().fireNewRundownMenuEvent(NewRundownMenuEvent(true));
        EventManager::getInstance().fireOpenRundownMenuEvent(OpenRundownMenuEvent(true));
        EventManager::getInstance().fireOpenRundownFromUrlMenuEvent(OpenRundownFromUrlMenuEvent(true));
    }
}

void RundownWidget::startTabDrag(QTabBar* bar, QMouseEvent* event)
{
    // End the bar's own move first, or it is left holding a tab that has gone.
    // The release lands where the cursor is, so the tab settles at whatever
    // index the bar had moved it to - and that, being current, is the index
    // carried across.
    QMouseEvent release(QEvent::MouseButtonRelease, event->position(), event->scenePosition(),
                        event->globalPosition(), Qt::LeftButton, Qt::NoButton, event->modifiers());
    QApplication::sendEvent(bar, &release);

    const int index = bar->currentIndex();

    this->tabDragBar = nullptr;
    this->tabDragIndex = -1;

    if (index < 0)
        return;

    const QString pane = (bar == this->tabWidgetRundownSecondary->tabBar()) ? "secondary" : "primary";

    QMimeData* mime = new QMimeData();
    mime->setData("application/rundown-tab", QString("%1:%2").arg(pane).arg(index).toUtf8());

    // The tab itself travels with the cursor, so it no longer vanishes at the
    // edge of its bar.
    QDrag* drag = new QDrag(bar);
    drag->setMimeData(mime);
    drag->setPixmap(bar->grab(bar->tabRect(index)));
    drag->setHotSpot(QPoint(drag->pixmap().width() / 2, drag->pixmap().height() / 2));
    drag->exec(Qt::MoveAction);
}

QTabWidget* RundownWidget::paneOf(QWidget* widget) const
{
    for (QWidget* parent = widget; parent != nullptr; parent = parent->parentWidget())
    {
        if (parent == this->tabWidgetRundown)
            return this->tabWidgetRundown;
        if (parent == this->tabWidgetRundownSecondary)
            return this->tabWidgetRundownSecondary;
    }

    return nullptr;
}

void RundownWidget::moveTabToOtherPane(QTabWidget* sourcePane, int tabIndex)
{
    if (sourcePane == nullptr || tabIndex < 0 || tabIndex >= sourcePane->count())
        return;

    QTabWidget* targetPane = otherPane(sourcePane);
    if (targetPane == nullptr)
        return;

    QWidget* widget = sourcePane->widget(tabIndex);
    QString text = sourcePane->tabText(tabIndex);
    QString tooltip = sourcePane->tabToolTip(tabIndex);
    QIcon icon = sourcePane->tabIcon(tabIndex);

    sourcePane->removeTab(tabIndex);

    int newIndex = targetPane->addTab(widget, icon, text);
    targetPane->setTabToolTip(newIndex, tooltip);
    targetPane->setCurrentWidget(widget);

    // If source pane is now empty, close the split.
    if (sourcePane->count() == 0)
        closeSplitView();
}

void RundownWidget::setupMenus()
{
    this->openRecentMenu = new QMenu(this);
    this->openRecentMenu->setTitle("Open Recent Rundown");

    QObject::connect(this->openRecentMenu, SIGNAL(aboutToShow()), this, SLOT(refreshOpenRecent()));
    QObject::connect(this->openRecentMenu, SIGNAL(triggered(QAction*)), this, SLOT(openRecentMenuActionTriggered(QAction*)));

    this->contextMenuMark = new QMenu(this);
    this->contextMenuMark->setTitle("Mark Item");
    this->contextMenuMark->addAction("As Used", this, SLOT(markItemAsUsedInRundown()));
    this->contextMenuMark->addAction("As Unused", this, SLOT(markItemAsUnusedInRundown()));
    this->contextMenuMark->addAction("All as Used", this, SLOT(markAllItemsAsUsedInRundown()));
    this->contextMenuMark->addAction("All as Unused", this, SLOT(markAllItemsAsUnusedInRundown()));

    this->contextMenuRundownDropdown = new QMenu(this);
    this->contextMenuRundownDropdown->setObjectName("panelMenu");
    this->contextMenuRundownDropdown->setTitle("Dropdown");
    this->newRundownAction = this->contextMenuRundownDropdown->addAction("New Rundown", this, SLOT(createNewRundown()));
    this->openRundownAction = this->contextMenuRundownDropdown->addAction("Open Rundown...", this, SLOT(openRundownFromDisk()));
    this->openRundownFromUrlAction = this->contextMenuRundownDropdown->addAction("Open Rundown from repository...", this, SLOT(openRundownFromRepo()));
    this->contextMenuRundownDropdown->addSeparator();
    this->openRecentMenuAction = this->contextMenuRundownDropdown->addMenu(this->openRecentMenu);
    this->contextMenuRundownDropdown->addSeparator();
    this->saveAction = this->contextMenuRundownDropdown->addAction("Save", this, SLOT(saveRundownToDisk()));
    this->saveAsAction = this->contextMenuRundownDropdown->addAction("Save As...", this, SLOT(saveAsRundownToDisk()));
    this->contextMenuRundownDropdown->addSeparator();
    this->contextMenuRundownDropdown->addMenu(this->contextMenuMark);
    this->contextMenuRundownDropdown->addSeparator();
    this->contextMenuRundownDropdown->addAction("Copy Item Properties", this, SLOT(copyItemProperties()));
    this->contextMenuRundownDropdown->addAction("Paste Item Properties", this, SLOT(pasteItemProperties()));
    this->contextMenuRundownDropdown->addAction("Paste Item Properties (No Data)", this, SLOT(pasteItemPropertiesNoData()));
    this->contextMenuRundownDropdown->addSeparator();
    this->compactViewAction = this->contextMenuRundownDropdown->addAction("Compact View");
    this->compactViewAction->setCheckable(true);
    this->allowRemoteTriggeringAction = this->contextMenuRundownDropdown->addAction("Allow Remote Triggering");
    this->allowRemoteTriggeringAction->setCheckable(true);
    this->lockRundownAction = this->contextMenuRundownDropdown->addAction("Lock Rundown");
    this->lockRundownAction->setCheckable(true);
    this->contextMenuRundownDropdown->addSeparator();
    this->insertRepositoryChangesAction = this->contextMenuRundownDropdown->addAction("Insert Repository Changes", this, SLOT(insertRepositoryChanges()));
    this->insertRepositoryChangesAction->setEnabled(false);
    this->contextMenuRundownDropdown->addSeparator();
    this->reloadRundownAction = this->contextMenuRundownDropdown->addAction("Reload Rundown", this, SLOT(reloadCurrentRundown()));
    this->contextMenuRundownDropdown->addSeparator();
    this->contextMenuRundownDropdown->addAction("Close Rundown", this, SLOT(closeCurrentRundown()));

    // Split view controls in the dropdown.
    this->contextMenuRundownDropdown->addSeparator();
    this->splitHorizontalAction = this->contextMenuRundownDropdown->addAction("Split Horizontal", this, SLOT(splitHorizontal()));
    this->splitVerticalAction = this->contextMenuRundownDropdown->addAction("Split Vertical", this, SLOT(splitVertical()));
    this->closeSplitAction = this->contextMenuRundownDropdown->addAction("Close Split", this, SLOT(closeSplitView()));
    this->closeSplitAction->setVisible(false);

    QObject::connect(this->compactViewAction, SIGNAL(toggled(bool)), this, SLOT(compactView(bool)));
    QObject::connect(this->allowRemoteTriggeringAction, SIGNAL(toggled(bool)), this, SLOT(remoteTriggering(bool)));
    QObject::connect(this->lockRundownAction, SIGNAL(toggled(bool)), this, SLOT(lockRundown(bool)));
}

void RundownWidget::refreshOpenRecent()
{
    foreach (QAction* action, this->openRecentMenu->actions())
        this->openRecentMenu->removeAction(action);

    QList<QString> paths = DatabaseManager::getInstance().getOpenRecent();
    foreach (QString path, paths)
        this->openRecentMenu->addAction(path);

    if (this->openRecentMenu->actions().count() > 0)
    {
        this->openRecentMenu->addSeparator();
        this->openRecentMenu->addAction("Clear Menu", this, SLOT(clearOpenRecent()));
    }
}

void RundownWidget::openRecentMenuActionTriggered(QAction* action)
{
    if (action->text().contains("Clear"))
        return;

    EventManager::getInstance().fireOpenRundownEvent(OpenRundownEvent(action->text()));
}

void RundownWidget::clearOpenRecent()
{
   DatabaseManager::getInstance().deleteOpenRecent();
}

RundownTreeWidget* RundownWidget::activeTreeWidget() const
{
    QTabWidget* pane = this->focusedTabWidget;
    if (pane == nullptr)
        pane = this->tabWidgetRundown;
    if (pane == nullptr || pane->count() == 0)
        return nullptr;

    return dynamic_cast<RundownTreeWidget*>(pane->currentWidget());
}

bool RundownWidget::checkForSaveBeforeQuit()
{
    // Check both panes.
    QList<QTabWidget*> panes;
    panes << this->tabWidgetRundown;
    if (this->tabWidgetRundownSecondary != nullptr)
        panes << this->tabWidgetRundownSecondary;

    for (QTabWidget* pane : panes)
    {
        for (int i = 0; i < pane->count(); i++)
        {
            if (dynamic_cast<RundownTreeWidget*>(pane->widget(i))->checkForSave())
            {
                QMessageBox box(this);
                box.setWindowTitle("Quit Application");
                box.setWindowIcon(QIcon(":/Graphics/Images/CasparCG.png"));
                box.setText(QString("You have unsaved changes in your %1 rundown. Do you want to save it before you quit?").arg(pane->tabText(i)));
                box.setIconPixmap(QPixmap(":/Graphics/Images/Attention.png"));
                box.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
                box.buttons().at(0)->setIcon(QIcon());
                box.buttons().at(0)->setFocusPolicy(Qt::NoFocus);
                box.buttons().at(1)->setIcon(QIcon());
                box.buttons().at(1)->setFocusPolicy(Qt::NoFocus);
                box.buttons().at(2)->setIcon(QIcon());
                box.buttons().at(2)->setFocusPolicy(Qt::NoFocus);

                int result = box.exec();
                if (result == QMessageBox::Yes)
                    dynamic_cast<RundownTreeWidget*>(pane->widget(i))->saveRundown(false);
                else if (result == QMessageBox::Cancel)
                    return false;
            }
        }
    }

    // Every unsaved rundown has now been offered and either saved or knowingly
    // abandoned, so the recovery copies have nothing left to recover. Leaving them
    // would make the next launch offer back work the operator just chose to drop.
    clearAutoSaves();

    return true;
}

void RundownWidget::reloadRundownMenu(const ReloadRundownMenuEvent& event)
{
    this->reloadRundownAction->setEnabled(event.getEnabled());
}

void RundownWidget::newRundownMenu(const NewRundownMenuEvent& event)
{
    this->newRundownAction->setEnabled(event.getEnabled());
}

void RundownWidget::openRundownMenu(const OpenRundownMenuEvent& event)
{
    this->openRundownAction->setEnabled(event.getEnabled());
}

void RundownWidget::openRundownFromUrlMenu(const OpenRundownFromUrlMenuEvent& event)
{
    this->openRundownFromUrlAction->setEnabled(event.getEnabled());
}

void RundownWidget::newRundown(const NewRundownEvent& event)
{
    Q_UNUSED(event);

    RundownTreeWidget* widget = new RundownTreeWidget(this);
    connectRundownTreeSignals(widget);
    int index = this->focusedTabWidget->addTab(widget, Rundown::DEFAULT_NAME);
    this->focusedTabWidget->setCurrentIndex(index);

    if (totalTabCount() == Rundown::MAX_NUMBER_OF_RUNDONWS)
    {
        EventManager::getInstance().fireNewRundownMenuEvent(NewRundownMenuEvent(false));
        EventManager::getInstance().fireOpenRundownMenuEvent(OpenRundownMenuEvent(false));
        EventManager::getInstance().fireOpenRundownFromUrlMenuEvent(OpenRundownFromUrlMenuEvent(false));
    }
}

void RundownWidget::compactView(const CompactViewEvent& event)
{
    this->compactViewAction->blockSignals(true);
    this->compactViewAction->setChecked(event.getEnabled());
    this->compactViewAction->blockSignals(false);
}

void RundownWidget::allowRemoteTriggering(const AllowRemoteTriggeringEvent& event)
{
    this->allowRemoteTriggeringAction->blockSignals(true);
    this->allowRemoteTriggeringAction->setChecked(event.getEnabled());
    this->allowRemoteTriggeringAction->blockSignals(false);

    if (!event.getEnabled())
        this->focusedTabWidget->setTabIcon(this->focusedTabWidget->currentIndex(), QIcon());
    else
        this->focusedTabWidget->setTabIcon(this->focusedTabWidget->currentIndex(), QIcon(":/Graphics/Images/RemoteTriggeringSmall.png"));
}

void RundownWidget::repositoryRundown(const RepositoryRundownEvent& event)
{
    this->saveAction->setEnabled(!event.getRepositoryRundown());
    this->saveAsAction->setEnabled(!event.getRepositoryRundown());
    this->allowRemoteTriggeringAction->setEnabled(!event.getRepositoryRundown());
    this->insertRepositoryChangesAction->setEnabled(event.getRepositoryRundown());

    // Auto-lock repository rundowns, unless user has manually overridden.
    if (event.getRepositoryRundown() && !this->userOverrideLock)
    {
        this->lockRundownAction->blockSignals(true);
        this->lockRundownAction->setChecked(true);
        this->lockRundownAction->blockSignals(false);
        lockRundown(true);
        this->userOverrideLock = false; // Reset — auto-lock shouldn't set the override flag.
    }
}

void RundownWidget::lockRundown(bool locked)
{
    int index = this->focusedTabWidget->currentIndex();
    RundownTreeWidget* currentTab = dynamic_cast<RundownTreeWidget*>(this->focusedTabWidget->currentWidget());
    if (currentTab == nullptr)
        return;

    currentTab->setLocked(locked);
    this->userOverrideLock = true; // User explicitly toggled — override auto-lock.

    // Update tab text with lock indicator.
    QString tabText = this->focusedTabWidget->tabText(index);
    static const QString lockPrefix = QString::fromUtf8("\xf0\x9f\x94\x92 "); // 🔒 + space
    tabText.remove(lockPrefix);
    if (locked)
        tabText.prepend(lockPrefix);
    this->focusedTabWidget->setTabText(index, tabText);
}

void RundownWidget::lockRundown(const LockRundownEvent& event)
{
    // Sync the tab dropdown action and apply.
    this->lockRundownAction->blockSignals(true);
    this->lockRundownAction->setChecked(event.getLocked());
    this->lockRundownAction->blockSignals(false);

    lockRundown(event.getLocked());
}

void RundownWidget::closeRundown(const CloseRundownEvent& event)
{
    Q_UNUSED(event);

    if (totalTabCount() > 1)
    {
        // Close the current tab in the focused pane.
        if (this->focusedTabWidget->count() <= 1 && this->splitViewActive)
        {
            closeSplitView();
            return;
        }
        deleteTabFromPane(this->focusedTabWidget, this->focusedTabWidget->currentIndex());
    }
}

void RundownWidget::deleteRundown(const DeleteRundownEvent& event)
{
    deleteTabFromPane(this->focusedTabWidget, event.getIndex());
}

void RundownWidget::openRundown(const OpenRundownEvent& event)
{
    QString path = "";

    if (event.getPath().isEmpty()){
        QList<QString> paths = DatabaseManager::getInstance().getOpenRecent();
        if(paths.count() > 0){
                path = paths.at(0);
                QFileInfo fi(path);
                path = fi.absolutePath();
        }else{
                path = QDir::homePath();
        }
        path = QFileDialog::getOpenFileName(this, "Open Rundown", path , "Rundown (*.xml)");
    }else
        path = event.getPath();

    if (!path.isEmpty())
    {
        RundownTreeWidget* widget = new RundownTreeWidget(this);
        connectRundownTreeSignals(widget);

        int index = this->focusedTabWidget->addTab(widget, Rundown::DEFAULT_NAME);
        this->focusedTabWidget->setTabToolTip(index, path);
        this->focusedTabWidget->setCurrentIndex(index);

        EventManager::getInstance().fireActiveRundownChangedEvent(ActiveRundownChangedEvent(path));

        widget->openRundown(path);

        if (totalTabCount() == Rundown::MAX_NUMBER_OF_RUNDONWS)
        {
            EventManager::getInstance().fireNewRundownMenuEvent(NewRundownMenuEvent(false));
            EventManager::getInstance().fireOpenRundownMenuEvent(OpenRundownMenuEvent(false));
            EventManager::getInstance().fireOpenRundownFromUrlMenuEvent(OpenRundownFromUrlMenuEvent(false));
        }
    }
}

void RundownWidget::openRundownFromUrl(const OpenRundownFromUrlEvent& event)
{
    QString path = "";

    if (event.getPath().isEmpty())
    {
        OpenRundownFromUrlDialog* dialog = new OpenRundownFromUrlDialog(this);
        if (dialog->exec() == QDialog::Accepted)
            path = dialog->getPath();
    }

    if (!path.isEmpty())
    {
        RundownTreeWidget* widget = new RundownTreeWidget(this);
        connectRundownTreeSignals(widget);

        int index = this->focusedTabWidget->addTab(widget, Rundown::DEFAULT_NAME);
        this->focusedTabWidget->setTabToolTip(index, path);
        this->focusedTabWidget->setCurrentIndex(index);

        EventManager::getInstance().fireActiveRundownChangedEvent(ActiveRundownChangedEvent(path));

        widget->openRundownFromUrl(path);

        if (totalTabCount() == Rundown::MAX_NUMBER_OF_RUNDONWS)
        {
            EventManager::getInstance().fireNewRundownMenuEvent(NewRundownMenuEvent(false));
            EventManager::getInstance().fireOpenRundownMenuEvent(OpenRundownMenuEvent(false));
            EventManager::getInstance().fireOpenRundownFromUrlMenuEvent(OpenRundownFromUrlMenuEvent(false));
        }
    }
}

void RundownWidget::markItemAsUsed(const MarkItemAsUsedEvent& event)
{
    Q_UNUSED(event);
    dynamic_cast<RundownTreeWidget*>(this->focusedTabWidget->currentWidget())->setUsed(true);
}

void RundownWidget::markItemAsUnused(const MarkItemAsUnusedEvent& event)
{
    Q_UNUSED(event);
    dynamic_cast<RundownTreeWidget*>(this->focusedTabWidget->currentWidget())->setUsed(false);
}

void RundownWidget::markAllItemsAsUsed(const MarkAllItemsAsUsedEvent& event)
{
    Q_UNUSED(event);
    dynamic_cast<RundownTreeWidget*>(this->focusedTabWidget->currentWidget())->setAllUsed(true);
}

void RundownWidget::markAllItemsAsUnused(const MarkAllItemsAsUnusedEvent& event)
{
    Q_UNUSED(event);
    dynamic_cast<RundownTreeWidget*>(this->focusedTabWidget->currentWidget())->setAllUsed(false);
}

void RundownWidget::reloadRundown(const ReloadRundownEvent& event)
{
    Q_UNUSED(event);

    EventManager::getInstance().fireStatusbarEvent(StatusbarEvent("Reloading rundown..."));

    dynamic_cast<RundownTreeWidget*>(this->focusedTabWidget->currentWidget())->reloadRundown();

    EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(""));
}

void RundownWidget::saveRundown(const SaveRundownEvent& event)
{
    dynamic_cast<RundownTreeWidget*>(this->focusedTabWidget->currentWidget())->saveRundown(event.getSaveAs());
}

void RundownWidget::activeRundownChanged(const ActiveRundownChangedEvent& event)
{
    QFileInfo info(event.getPath());
    QString tabText = (event.getPath().startsWith("http") == true) ? event.getPath().split("/").last() : info.baseName();

    // Preserve lock prefix if the tab is locked.
    RundownTreeWidget* currentTab = dynamic_cast<RundownTreeWidget*>(this->focusedTabWidget->currentWidget());
    if (currentTab != nullptr && currentTab->isLocked())
        tabText.prepend(QString::fromUtf8("\xf0\x9f\x94\x92 "));

    this->focusedTabWidget->setTabText(this->focusedTabWidget->currentIndex(), tabText);
}

void RundownWidget::createNewRundown()
{
    EventManager::getInstance().fireNewRundownEvent(NewRundownEvent());
}

void RundownWidget::openRundownFromDisk()
{
    EventManager::getInstance().fireOpenRundownEvent(OpenRundownEvent());
}

void RundownWidget::openRundownFromRepo()
{
    EventManager::getInstance().fireOpenRundownFromUrlEvent(OpenRundownFromUrlEvent());
}

void RundownWidget::saveRundownToDisk()
{
    EventManager::getInstance().fireSaveRundownEvent(SaveRundownEvent(false));
}

void RundownWidget::saveAsRundownToDisk()
{
    EventManager::getInstance().fireSaveRundownEvent(SaveRundownEvent(true));
}

void RundownWidget::copyItemProperties()
{
    EventManager::getInstance().fireCopyItemPropertiesEvent(CopyItemPropertiesEvent());
}

void RundownWidget::pasteItemProperties()
{
    EventManager::getInstance().firePasteItemPropertiesEvent(PasteItemPropertiesEvent());
}

void RundownWidget::pasteItemPropertiesNoData()
{
    EventManager::getInstance().firePasteItemPropertiesEvent(PasteItemPropertiesEvent(true));
}

void RundownWidget::reloadCurrentRundown()
{
    EventManager::getInstance().fireReloadRundownEvent(ReloadRundownEvent());
}

void RundownWidget::closeCurrentRundown()
{
    EventManager::getInstance().fireCloseRundownEvent(CloseRundownEvent());
}

void RundownWidget::markItemAsUsedInRundown()
{
    EventManager::getInstance().fireMarkItemAsUsedEvent(MarkItemAsUsedEvent());
}

void RundownWidget::markItemAsUnusedInRundown()
{
    EventManager::getInstance().fireMarkItemAsUnusedEvent(MarkItemAsUnusedEvent());
}

void RundownWidget::markAllItemsAsUsedInRundown()
{
    EventManager::getInstance().fireMarkAllItemsAsUsedEvent(MarkAllItemsAsUsedEvent());
}

void RundownWidget::markAllItemsAsUnusedInRundown()
{
    EventManager::getInstance().fireMarkAllItemsAsUnusedEvent(MarkAllItemsAsUnusedEvent());
}

void RundownWidget::compactView(bool enabled)
{
    EventManager::getInstance().fireCompactViewEvent(CompactViewEvent(enabled));
}

void RundownWidget::remoteTriggering(bool enabled)
{
    EventManager::getInstance().fireAllowRemoteTriggeringEvent(AllowRemoteTriggeringEvent(enabled));
}

void RundownWidget::insertRepositoryChanges()
{
    EventManager::getInstance().fireInsertRepositoryChangesEvent(InsertRepositoryChangesEvent());
}

bool RundownWidget::selectTab(int index)
{
    if (index <= this->focusedTabWidget->count())
        this->focusedTabWidget->setCurrentIndex(index - 1);

    return true;
}

void RundownWidget::gpiBindingChanged(int gpiPort, Playout::PlayoutType binding)
{
    // Apply to all tabs in all panes.
    for (int i = 0; i < this->tabWidgetRundown->count(); i++)
        dynamic_cast<RundownTreeWidget*>(this->tabWidgetRundown->widget(i))->gpiBindingChanged(gpiPort, binding);

    if (this->tabWidgetRundownSecondary != nullptr)
    {
        for (int i = 0; i < this->tabWidgetRundownSecondary->count(); i++)
            dynamic_cast<RundownTreeWidget*>(this->tabWidgetRundownSecondary->widget(i))->gpiBindingChanged(gpiPort, binding);
    }
}

void RundownWidget::connectRundownTreeSignals(RundownTreeWidget* widget)
{
    QObject::connect(widget, &RundownTreeWidget::requestCrossTabGateway,
                     this, &RundownWidget::handleCrossTabGateway);
    QObject::connect(widget, &RundownTreeWidget::requestCrossTabGatewayRelay,
                     this, &RundownWidget::handleCrossTabGatewayRelay);
    QObject::connect(widget, &RundownTreeWidget::requestCrossTabFocusGateway,
                     this, &RundownWidget::handleCrossTabFocusGateway);

    // Register this tab's undo stack with the undo group.
    this->m_undoGroup->addStack(widget->undoStack());
    this->m_undoGroup->setActiveStack(widget->undoStack());
}

void RundownWidget::handleCrossTabGateway(const QString& gatewayId, const QString& exitLabel)
{
    RundownTreeWidget* senderTab = qobject_cast<RundownTreeWidget*>(sender());

    // Search all tabs in all panes (except sender) for the matching exit.
    auto searchPane = [&](QTabWidget* pane) -> bool {
        if (pane == nullptr) return false;
        for (int i = 0; i < pane->count(); i++)
        {
            RundownTreeWidget* tab = dynamic_cast<RundownTreeWidget*>(pane->widget(i));
            if (tab == senderTab || !tab->hasGatewayExit(gatewayId))
                continue;

            // Found. Switch to this tab, then start autoplay.
            setFocusedPane(pane);
            pane->setCurrentIndex(i);
            tab->startAutoPlayFromGatewayExit(gatewayId, exitLabel);
            return true;
        }
        return false;
    };

    if (searchPane(this->tabWidgetRundown))
        return;
    if (searchPane(this->tabWidgetRundownSecondary))
        return;

    qDebug("Cross-tab gateway: no matching exit found in any tab");
}

void RundownWidget::handleCrossTabGatewayRelay(const QString& gatewayId, Playout::PlayoutType type, const QString& exitLabel)
{
    RundownTreeWidget* senderTab = qobject_cast<RundownTreeWidget*>(sender());

    auto searchPane = [&](QTabWidget* pane) -> bool {
        if (pane == nullptr) return false;
        for (int i = 0; i < pane->count(); i++)
        {
            RundownTreeWidget* tab = dynamic_cast<RundownTreeWidget*>(pane->widget(i));
            if (tab == senderTab || !tab->hasGatewayExit(gatewayId))
                continue;

            setFocusedPane(pane);
            pane->setCurrentIndex(i);
            tab->relayCommandToGatewayExit(gatewayId, type, exitLabel);
            return true;
        }
        return false;
    };

    if (searchPane(this->tabWidgetRundown))
        return;
    if (searchPane(this->tabWidgetRundownSecondary))
        return;

    qDebug("Cross-tab gateway relay: no matching exit found in any tab");
}

void RundownWidget::handleCrossTabFocusGateway(const QString& gatewayId, bool fromIsExit, const QString& exitLabel)
{
    RundownTreeWidget* senderTab = qobject_cast<RundownTreeWidget*>(sender());

    auto findInPane = [&](QTabWidget* pane) -> QPair<RundownTreeWidget*, int> {
        if (pane == nullptr) return {nullptr, -1};
        for (int i = 0; i < pane->count(); i++)
        {
            RundownTreeWidget* tab = dynamic_cast<RundownTreeWidget*>(pane->widget(i));
            if (tab == nullptr || tab == senderTab)
                continue;

            if (tab->hasFocusGatewayPartner(gatewayId, fromIsExit, exitLabel))
                return {tab, i};
        }
        return {nullptr, -1};
    };

    QTabWidget* targetPane = nullptr;
    RundownTreeWidget* targetTab = nullptr;
    int targetIndex = -1;

    auto result = findInPane(this->tabWidgetRundown);
    if (result.first != nullptr)
    {
        targetPane = this->tabWidgetRundown;
        targetTab = result.first;
        targetIndex = result.second;
    }
    else
    {
        result = findInPane(this->tabWidgetRundownSecondary);
        if (result.first != nullptr)
        {
            targetPane = this->tabWidgetRundownSecondary;
            targetTab = result.first;
            targetIndex = result.second;
        }
    }

    if (targetTab == nullptr)
        return;

    // Defer the focus jump to after the current event finishes propagating.
    // The global executePlayoutCommand event is delivered to ALL RundownTreeWidgets.
    // If we activate the target tab synchronously, its executePlayoutCommand slot
    // will also fire (now active), re-processing the same F2 on the newly selected
    // gateway item and bouncing focus back to the original pane.
    QTimer::singleShot(0, this, [this, targetPane, targetIndex, targetTab, gatewayId, fromIsExit, exitLabel]() {
        targetPane->setCurrentIndex(targetIndex);
        setFocusedPane(targetPane);
        targetTab->executeFocusJump(gatewayId, fromIsExit, exitLabel);
    });
}

void RundownWidget::setupSearchBar()
{
    this->searchBar = new QWidget(this);
    this->searchBar->setFixedHeight(26);
    this->searchBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QHBoxLayout* layout = new QHBoxLayout(this->searchBar);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);

    this->searchLineEdit = new QLineEdit(this->searchBar);
    this->searchLineEdit->setPlaceholderText("Find in all rundowns...");
    this->searchLineEdit->setClearButtonEnabled(true);
    this->searchLineEdit->setFixedHeight(20);

    this->searchPrevButton = new QPushButton(QString::fromUtf8("\xe2\x96\xb2"), this->searchBar);
    this->searchPrevButton->setFixedSize(28, 20);
    this->searchNextButton = new QPushButton(QString::fromUtf8("\xe2\x96\xbc"), this->searchBar);
    this->searchNextButton->setFixedSize(28, 20);

    this->searchCountLabel = new QLabel("", this->searchBar);
    this->searchCountLabel->setFixedSize(70, 20);
    this->searchCountLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(this->searchLineEdit, 1);
    layout->addWidget(this->searchPrevButton);
    layout->addWidget(this->searchNextButton);
    layout->addWidget(this->searchCountLabel);

    // Insert at top of vertical layout (before splitter) with zero stretch.
    this->verticalLayout->insertWidget(0, this->searchBar, 0);

    // Ctrl+F focuses the search field.
    QShortcut* findShortcut = new QShortcut(QKeySequence::Find, this);
    QObject::connect(findShortcut, &QShortcut::activated, this, &RundownWidget::showSearch);

    // Escape returns focus to the rundown tree.
    QShortcut* escShortcut = new QShortcut(Qt::Key_Escape, this->searchLineEdit);
    QObject::connect(escShortcut, &QShortcut::activated, this, &RundownWidget::hideSearch);

    QObject::connect(this->searchLineEdit, &QLineEdit::textChanged, this, &RundownWidget::searchTextChanged);
    QObject::connect(this->searchLineEdit, &QLineEdit::returnPressed, this, &RundownWidget::searchNext);
    QObject::connect(this->searchNextButton, &QPushButton::clicked, this, &RundownWidget::searchNext);
    QObject::connect(this->searchPrevButton, &QPushButton::clicked, this, &RundownWidget::searchPrev);
}

void RundownWidget::showSearch()
{
    this->searchLineEdit->setFocus();
    this->searchLineEdit->selectAll();
}

void RundownWidget::hideSearch()
{
    this->searchLineEdit->clear();
    this->searchResults.clear();
    this->searchCurrentIndex = -1;
    this->searchCountLabel->setText("");

    // Return focus to the rundown tree.
    RundownTreeWidget* tab = dynamic_cast<RundownTreeWidget*>(this->focusedTabWidget->currentWidget());
    if (tab != nullptr)
        tab->treeWidget()->setFocus();
}

void RundownWidget::searchTextChanged(const QString& text)
{
    this->searchResults.clear();
    this->searchCurrentIndex = -1;

    if (text.isEmpty())
    {
        this->searchCountLabel->setText("");
        return;
    }

    // Collect matches from every tab in the given pane.
    auto collectFromPane = [&](QTabWidget* pane) {
        if (pane == nullptr)
            return;

        for (int i = 0; i < pane->count(); i++)
        {
            RundownTreeWidget* tab = dynamic_cast<RundownTreeWidget*>(pane->widget(i));
            if (tab == nullptr)
                continue;

            RundownTreeBaseWidget* tree = tab->treeWidget();

            std::function<void(QTreeWidgetItem*)> collectMatches = [&](QTreeWidgetItem* parent) {
                for (int j = 0; j < parent->childCount(); j++)
                {
                    QTreeWidgetItem* item = parent->child(j);
                    AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(tree->itemWidget(item, 0));
                    if (widget != nullptr && widget->getLibraryModel() != nullptr)
                    {
                        QString label = widget->getLibraryModel()->getLabel();
                        QString name = widget->getLibraryModel()->getName();
                        if (label.contains(text, Qt::CaseInsensitive) || name.contains(text, Qt::CaseInsensitive))
                            this->searchResults.append({pane, tab, item});
                    }
                    // Search children (groups).
                    if (item->childCount() > 0)
                        collectMatches(item);
                }
            };

            collectMatches(tree->invisibleRootItem());
        }
    };

    collectFromPane(this->tabWidgetRundown);
    collectFromPane(this->tabWidgetRundownSecondary);

    if (this->searchResults.isEmpty())
    {
        this->searchCountLabel->setText("0 of 0");
        return;
    }

    // Select first match.
    this->searchCurrentIndex = 0;
    navigateToSearchResult(0);
}

void RundownWidget::navigateToSearchResult(int index)
{
    if (index < 0 || index >= this->searchResults.size())
        return;

    const SearchResult& result = this->searchResults.at(index);

    // Verify the tab still exists in the pane.
    int tabIndex = result.pane->indexOf(result.tab);
    if (tabIndex < 0)
        return;

    // Switch pane if needed.
    if (result.pane != this->focusedTabWidget)
        setFocusedPane(result.pane);

    // Switch tab if needed.
    if (result.pane->currentIndex() != tabIndex)
        result.pane->setCurrentIndex(tabIndex);

    // Select and scroll to the item.
    RundownTreeBaseWidget* tree = result.tab->treeWidget();
    tree->setCurrentItem(result.item);
    tree->scrollToItem(result.item);

    this->searchCountLabel->setText(
        QString("%1 of %2").arg(index + 1).arg(this->searchResults.size()));

    // Keep focus on the search input so the user can keep typing.
    this->searchLineEdit->setFocus();
}

void RundownWidget::searchNext()
{
    if (this->searchResults.isEmpty())
        return;

    this->searchCurrentIndex = (this->searchCurrentIndex + 1) % this->searchResults.size();
    navigateToSearchResult(this->searchCurrentIndex);
}

void RundownWidget::searchPrev()
{
    if (this->searchResults.isEmpty())
        return;

    this->searchCurrentIndex = (this->searchCurrentIndex - 1 + this->searchResults.size()) % this->searchResults.size();
    navigateToSearchResult(this->searchCurrentIndex);
}
