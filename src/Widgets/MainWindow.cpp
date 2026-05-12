#include "MainWindow.h"
#include "AboutDialog.h"
#include "ClockWidget.h"
#include "HelpDialog.h"
#include "HttpResponsePanelWidget.h"
#include "NdiPanelWidget.h"
#include "PanelHelper.h"
#include "PanelResizeHandle.h"
#include "PerformancePanelWidget.h"
#include "Rundown/RundownWidget.h"
#include "SettingsDialog.h"
#include "StatusBarWidget.h"
#include "WhatsNewDialog.h"

#include "Version.h"
#include "Global.h"

#include "EventManager.h"
#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "Events/ExportPresetEvent.h"
#include "Events/ImportPresetEvent.h"
#include "Events/SaveAsPresetEvent.h"
#include "Events/ToggleFullscreenEvent.h"
#include "Events/Rundown/CloseRundownEvent.h"
#include "Events/Rundown/EmptyRundownEvent.h"
#include "Events/Rundown/NewRundownEvent.h"
#include "Events/Rundown/OpenRundownEvent.h"
#include "Events/Rundown/SaveRundownEvent.h"
#include "Events/Rundown/CopyItemPropertiesEvent.h"
#include "Events/Rundown/PasteItemPropertiesEvent.h"
#include "Events/Library/RefreshLibraryEvent.h"
#include "Events/Rundown/AllowRemoteTriggeringMenuEvent.h"
#include "Events/Rundown/CompactViewEvent.h"
#include "Events/Rundown/ExecutePlayoutCommandEvent.h"
#include "Events/Rundown/AllowRemoteTriggeringEvent.h"
#include "Events/Rundown/LockRundownEvent.h"
#include "Events/Rundown/AssignBankEvent.h"

#include <algorithm>

#include <QtCore/QTimer>
#include <QtCore/QTime>
#include <QtWidgets/QStyle>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>

#include <QtGui/QIcon>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>

#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtGui/QShortcut>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidgetAction>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QVBoxLayout>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <windowsx.h>
#endif

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setupUi(this);
    setupMenu();
    loadHotkeys();
    setWindowIcon(QIcon(":/Graphics/Images/CasparCG.png"));

    this->applicationTitle = QString("%1 v%2.%3.%4 build %5")
        .arg(this->windowTitle())
        .arg(MAJOR_VERSION)
        .arg(MINOR_VERSION)
        .arg(REVISION_VERSION)
        .arg(DEV_BUILD_ID);
    this->setWindowTitle(this->applicationTitle);

    this->widgetAction->setVisible(false);

    this->splitterHorizontal->setSizes(QList<int>() << 1 << 0);

    // Hide the default status bar; create custom widgets placed by rebuildLayout.
    this->statusBar()->hide();
    this->widgetStatusBar = new StatusBarWidget(this);
    this->widgetClock = new ClockWidget(this);
    this->widgetNdi = new NdiPanelWidget(this);
    this->widgetPerformance = new PerformancePanelWidget(this);
    this->widgetHttpLog = new HttpResponsePanelWidget(this);

    rebuildLayout();

    // StatusPanelWidget stays as a hidden controller; its tab widgets are
    // placed individually by rebuildLayout().
    this->widgetStatusPanel->hide();

    QString showPreviewBorderValue = DatabaseManager::getInstance().getConfigurationByName("ShowPreviewBorder").getValue();
    this->showPreviewBorder = showPreviewBorderValue.isEmpty() || showPreviewBorderValue == "true";
    this->previewModeActive = false;
    this->previewModifierHeld = false;
    this->previewModifierKey = DatabaseManager::getInstance().getConfigurationByName("PreviewModifier").getValue();

    // Lightweight overlay for the preview border. Because this QFrame has no
    // children, setting a stylesheet on it does NOT trigger expensive style
    // recalculation across all widgets (unlike centralWidget->setStyleSheet()).
    this->previewBorderOverlay = new QFrame(Ui::MainWindow::centralWidget);
    this->previewBorderOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    this->previewBorderOverlay->setFrameShape(QFrame::NoFrame);
    this->previewBorderOverlay->hide();

    // Channel lock border overlay (red border + label showing locked channels).
    this->lockBorderOverlay = new QFrame(Ui::MainWindow::centralWidget);
    this->lockBorderOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    this->lockBorderOverlay->setFrameShape(QFrame::NoFrame);
    this->lockBorderOverlay->hide();

    this->lockBorderLabel = new QLabel(this->lockBorderOverlay);
    this->lockBorderLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    this->lockBorderLabel->setStyleSheet(
        "QLabel { background-color: rgba(198, 40, 40, 230); color: white; "
        "padding: 3px 10px; font-size: 11px; font-weight: bold; "
        "border: 0px; border-radius: 0px 0px 6px 6px; }");
    this->lockBorderLabel->setAlignment(Qt::AlignCenter);
    this->lockBorderLabel->hide();

    QObject::connect(&DeviceManager::getInstance(),
                     SIGNAL(channelLockChanged(const QString&, int, bool)),
                     this, SLOT(channelLockChanged(const QString&, int, bool)));

    qApp->installEventFilter(this);

    QShortcut* panicShortcut = new QShortcut(Qt::Key_Escape, this);
    panicShortcut->setContext(Qt::ApplicationShortcut);
    QObject::connect(panicShortcut, &QShortcut::activated, this, &MainWindow::panicClearAll);

    QObject::connect(&EventManager::getInstance(), SIGNAL(previewModeChanged(bool)), this, SLOT(previewModeActivated(bool)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(previewModifierHeld(bool)), this, SLOT(previewModifierActivated(bool)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(emptyRundown(const EmptyRundownEvent&)), this, SLOT(emptyRundown(const EmptyRundownEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(activeRundownChanged(const ActiveRundownChangedEvent&)), this, SLOT(activeRundownChanged(const ActiveRundownChangedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(newRundownMenu(const NewRundownMenuEvent&)), this, SLOT(newRundownMenu(const NewRundownMenuEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(openRundownMenu(const OpenRundownMenuEvent&)), this, SLOT(openRundownMenu(const OpenRundownMenuEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(openRundownFromUrlMenu(const OpenRundownFromUrlMenuEvent&)), this, SLOT(openRundownFromUrlMenu(const OpenRundownFromUrlMenuEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(compactView(const CompactViewEvent&)), this, SLOT(compactView(const CompactViewEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(allowRemoteTriggering(const AllowRemoteTriggeringEvent&)), this, SLOT(allowRemoteTriggering(const AllowRemoteTriggeringEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(lockRundown(const LockRundownEvent&)), this, SLOT(lockRundown(const LockRundownEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(repositoryRundown(const RepositoryRundownEvent&)), this, SLOT(repositoryRundown(const RepositoryRundownEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(exportPresetMenu(const ExportPresetMenuEvent&)), this, SLOT(exportPresetMenu(const ExportPresetMenuEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(saveAsPresetMenu(const SaveAsPresetMenuEvent&)), this, SLOT(saveAsPresetMenu(const SaveAsPresetMenuEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(reloadRundownMenu(const ReloadRundownMenuEvent&)), this, SLOT(reloadRundownMenu(const ReloadRundownMenuEvent&)));
    QObject::connect(&EventManager::getInstance(), &EventManager::rebuildLayout, this, &MainWindow::rebuildLayout);

    // Constrain the window to the available screen area so panel heights
    // can never push the window beyond the screen (including when maximized).
    QTimer::singleShot(0, this, [this]() { constrainToScreen(); });

    QTimer::singleShot(10000, this, [this]() { WhatsNewDialog::showOnStartupIfEnabled(this); });
}

void MainWindow::setupMenu()
{
    this->openRecentMenu = new QMenu(this);
    this->openRecentMenu->setTitle("Open Recent Rundown");
    QObject::connect(this->openRecentMenu, SIGNAL(triggered(QAction*)), this, SLOT(openRecentMenuActionTriggered(QAction*)));

    this->fileMenu = new QMenu(this);
    this->newRundownAction = this->fileMenu->addAction("New Rundown", this, SLOT(newRundown()), QKeySequence::fromString("Ctrl+N"));
    this->openRundownAction = this->fileMenu->addAction("Open Rundown...", this, SLOT(openRundown()), QKeySequence::fromString("Ctrl+O"));
    this->openRundownFromUrlAction = this->fileMenu->addAction("Open Rundown from repository...", this, SLOT(openRundownFromUrl()), QKeySequence::fromString("Ctrl+Shift+O"));
    this->fileMenu->addSeparator();
    this->openRecentMenuAction = this->fileMenu->addMenu(this->openRecentMenu);
    this->fileMenu->addSeparator();
    this->fileMenu->addAction("Import Preset...", this, SLOT(importPreset()));
    this->exportPresetAction = this->fileMenu->addAction("Export Preset...", this, SLOT(exportPreset()));
    this->saveAsPresetAction = this->fileMenu->addAction("Save as Preset...", this, SLOT(saveAsPreset()));
    this->fileMenu->addSeparator();
    this->saveAction = this->fileMenu->addAction("Save", this, SLOT(saveRundown()), QKeySequence::fromString("Ctrl+S"));
    this->saveAsAction = this->fileMenu->addAction("Save As...", this, SLOT(saveAsRundown()), QKeySequence::fromString("Ctrl+Shift+S"));
    this->fileMenu->addSeparator();
    this->fileMenu->addAction("Quit", this, SLOT(close()));
    this->saveAsPresetAction->setEnabled(false);
    QObject::connect(this->openRecentMenuAction, SIGNAL(hovered()), this, SLOT(openRecentMenuHovered()));

    this->editMenu = new QMenu(this);
    QAction* undoAction = this->widgetRundown->undoGroup()->createUndoAction(this, "Undo");
    undoAction->setShortcut(QKeySequence::fromString("Ctrl+Z"));
    this->editMenu->addAction(undoAction);
    QAction* redoAction = this->widgetRundown->undoGroup()->createRedoAction(this, "Redo");
    redoAction->setShortcut(QKeySequence::fromString("Ctrl+Y"));
    this->editMenu->addAction(redoAction);
    this->editMenu->addSeparator();
    this->editMenu->addAction("Settings...", this, SLOT(showSettingsDialog()));

    this->viewMenu = new QMenu(this);
    this->viewMenu->setObjectName("menuView");
    this->compactViewAction = this->viewMenu->addAction("Compact View");
    this->compactViewAction->setCheckable(true);
    this->viewMenu->addSeparator();
    this->viewMenu->addAction("Split Horizontal", this->widgetRundown, SLOT(splitHorizontal()), QKeySequence::fromString("Ctrl+\\"));
    this->viewMenu->addAction("Split Vertical", this->widgetRundown, SLOT(splitVertical()), QKeySequence::fromString("Ctrl+Shift+\\"));
    this->viewMenu->addSeparator();
    this->viewMenu->addAction("Toggle Fullscreen", this, SLOT(toggleFullscreen()));

    this->libraryMenu = new QMenu(this);
    this->libraryMenu->addAction("Refresh Library", this, SLOT(refreshLibrary()), QKeySequence::fromString("Ctrl+R"));

    this->markMenu = new QMenu(this);
    this->markMenu->setTitle("Mark Item");
    this->markMenu->addAction(/*QIcon(":/Graphics/Images/RenameRundown.png"),*/ "As Used", this, SLOT(markItemAsUsed()));
    this->markMenu->addAction(/*QIcon(":/Graphics/Images/RenameRundown.png"),*/ "As Unused", this, SLOT(markItemAsUnused()));
    this->markMenu->addAction(/*QIcon(":/Graphics/Images/RenameRundown.png"),*/ "All as Used", this, SLOT(markAllItemsAsUsed()));
    this->markMenu->addAction(/*QIcon(":/Graphics/Images/RenameRundown.png"),*/ "All as Unused", this, SLOT(markAllItemsAsUnused()));

    this->rundownMenu = new QMenu(this);
    this->rundownMenu->setObjectName("menuRundown");
    this->rundownMenu->addMenu(this->markMenu);
    this->rundownMenu->addSeparator();
    this->rundownMenu->addAction("Copy Item Properties", this, SLOT(copyItemProperties()), QKeySequence::fromString("Shift+C"));
    this->rundownMenu->addAction("Paste Item Properties", this, SLOT(pasteItemProperties()), QKeySequence::fromString("Shift+V"));
    this->allowRemoteTriggeringAction = this->rundownMenu->addAction("Allow Remote Triggering");
    this->allowRemoteTriggeringAction->setCheckable(true);
    this->lockRundownAction = this->rundownMenu->addAction("Lock Rundown");
    this->lockRundownAction->setCheckable(true);
    this->rundownMenu->addSeparator();
    this->insertRepositoryChangesAction = this->rundownMenu->addAction("Insert Repository Changes", this, SLOT(insertRepositoryChanges()), QKeySequence::fromString("Ins"));
    this->insertRepositoryChangesAction->setEnabled(false);
    this->rundownMenu->addSeparator();
    this->reloadRundownAction = this->rundownMenu->addAction("Reload Rundown", this, SLOT(reloadRundown()), QKeySequence::fromString("Ctrl+L"));
    this->rundownMenu->addSeparator();
    this->rundownMenu->addAction("Close Rundown", this, SLOT(closeRundown()), QKeySequence::fromString("Ctrl+W"));

    QObject::connect(this->compactViewAction, SIGNAL(toggled(bool)), this, SLOT(compactView(bool)));
    QObject::connect(this->allowRemoteTriggeringAction, SIGNAL(toggled(bool)), this, SLOT(allowRemoteTriggering(bool)));
    QObject::connect(this->lockRundownAction, SIGNAL(toggled(bool)), this, SLOT(lockRundown(bool)));

    this->playoutMenu = new QMenu(this);
    this->playoutActions[Playout::PlayoutType::Stop] = this->playoutMenu->addAction("Stop", this, SLOT(executeStop()));
    this->playoutActions[Playout::PlayoutType::Play] = this->playoutMenu->addAction("Play", this, SLOT(executePlay()));
    this->playoutActions[Playout::PlayoutType::Load] = this->playoutMenu->addAction("Load", this, SLOT(executeLoad()));
    this->playoutActions[Playout::PlayoutType::PauseResume] = this->playoutMenu->addAction("Pause / Resume", this, SLOT(executePause()));
    this->playoutMenu->addSeparator();
    this->playoutActions[Playout::PlayoutType::Next] = this->playoutMenu->addAction("Next", this, SLOT(executeNext()));
    this->playoutActions[Playout::PlayoutType::Update] = this->playoutMenu->addAction("Update", this, SLOT(executeUpdate()));
    this->playoutActions[Playout::PlayoutType::Invoke] = this->playoutMenu->addAction("Invoke", this, SLOT(executeInvoke()));
    this->playoutActions[Playout::PlayoutType::Preview] = this->playoutMenu->addAction("Preview", this, SLOT(executePreview()));
    this->playoutMenu->addSeparator();
    this->playoutActions[Playout::PlayoutType::Clear] = this->playoutMenu->addAction("Clear", this, SLOT(executeClear()));
    this->playoutActions[Playout::PlayoutType::ClearVideoLayer] = this->playoutMenu->addAction("Clear Video Layer", this, SLOT(executeClearVideolayer()));
    this->playoutActions[Playout::PlayoutType::ClearChannel] = this->playoutMenu->addAction("Clear Channel", this, SLOT(executeClearChannel()));
    this->playoutMenu->addSeparator();
    this->playoutActions[Playout::PlayoutType::PlayNow] = this->playoutMenu->addAction("Play Now", this, SLOT(executePlayNow()));

    this->otherMenu = new QMenu(this);
    this->otherMenu->setObjectName("menuOther");
    this->disableCommandAction = this->otherMenu->addAction("Disable Commands");
    this->disableCommandAction->setCheckable(true);
    QObject::connect(this->disableCommandAction, SIGNAL(toggled(bool)), this, SLOT(disableCommandToggled(bool)));
    this->otherMenu->addSeparator();
    this->otherMenu->addAction("Disconnect Stream", []() {
        EventManager::getInstance().fireDisconnectStreamEvent();
    });

    this->helpMenu = new QMenu(this);
    QAction* action = this->helpMenu->addAction("View Help", this, SLOT(showHelpDialog()), QKeySequence::fromString("Ctrl+H"));
    this->helpMenu->addSeparator();
    this->helpMenu->addAction("What's New...", this, SLOT(showWhatsNewDialog()));
    this->helpMenu->addAction("About CasparCG Client...", this, SLOT(showAboutDialog()));
    action->setEnabled(false);

    // Force all menus to reserve the indicator column for consistent left spacing.
    // Menus with real checkable items (viewMenu, rundownMenu, otherMenu) already have it.
    auto forceIndicatorColumn = [](QMenu* menu) {
        QWidgetAction* dummy = new QWidgetAction(menu);
        QWidget* w = new QWidget();
        w->setFixedHeight(0);
        w->setMaximumHeight(0);
        dummy->setDefaultWidget(w);
        dummy->setCheckable(true);
        menu->addAction(dummy);
    };
    forceIndicatorColumn(this->fileMenu);
    forceIndicatorColumn(this->editMenu);
    forceIndicatorColumn(this->libraryMenu);
    forceIndicatorColumn(this->playoutMenu);
    forceIndicatorColumn(this->helpMenu);

    // Create hidden bank assignment actions (hotkey-only, no menu entry).
    for (int i = 1; i <= TriggerBank::BANK_COUNT; i++)
    {
        QAction* bankAction = new QAction(this);
        this->bankActions[i] = bankAction;
        this->addAction(bankAction);
        int bankId = i;
        QObject::connect(bankAction, &QAction::triggered, [bankId]() {
            EventManager::getInstance().fireAssignBankEvent(AssignBankEvent(bankId));
        });
    }

    // Create hidden toggle preview action (hotkey-only, no menu entry).
    this->togglePreviewAction = new QAction(this);
    this->togglePreviewAction->setShortcutContext(Qt::ApplicationShortcut);
    this->addAction(this->togglePreviewAction);
    QObject::connect(this->togglePreviewAction, &QAction::triggered, this, &MainWindow::togglePreviewMode);

    // Create hidden toggle autostep action (hotkey-only, no menu entry).
    this->toggleAutostepAction = new QAction(this);
    this->toggleAutostepAction->setShortcutContext(Qt::ApplicationShortcut);
    this->addAction(this->toggleAutostepAction);
    QObject::connect(this->toggleAutostepAction, &QAction::triggered, this, &MainWindow::toggleAutostepMode);

    this->menuBar = new QMenuBar(this);
    this->menuBar->addMenu(this->fileMenu)->setText("File");
    this->menuBar->addMenu(this->editMenu)->setText("Edit");
    this->menuBar->addMenu(this->viewMenu)->setText("View");
    this->menuBar->addMenu(this->libraryMenu)->setText("Library");
    this->menuBar->addMenu(this->rundownMenu)->setText("Rundown");
    this->menuBar->addMenu(this->playoutMenu)->setText("Playout");
    this->menuBar->addMenu(this->otherMenu)->setText("Other");
    this->menuBar->addMenu(this->helpMenu)->setText("Help");

    setMenuBar(this->menuBar);
}

void MainWindow::openRecentMenuHovered()
{
    foreach (QAction* action, this->openRecentMenu->actions())
        this->openRecentMenu->removeAction(action);

    QList<QString> paths = DatabaseManager::getInstance().getOpenRecent();
    foreach (QString path, paths)
        this->openRecentMenu->addAction(/*QIcon(":/Graphics/Images/OpenRecent.png"),*/ path);

    if (this->openRecentMenu->actions().count() > 0)
    {
        this->openRecentMenu->addSeparator();
        this->openRecentMenu->addAction(/*QIcon(":/Graphics/Images/ClearOpenRecent.png"),*/ "Clear Menu", this, SLOT(clearOpenRecent()));
    }
}

void MainWindow::openRecentMenuActionTriggered(QAction* action)
{
    if (action->text().contains("Clear"))
        return;

    EventManager::getInstance().fireOpenRundownEvent(OpenRundownEvent(action->text()));
}

void MainWindow::clearOpenRecent()
{
   DatabaseManager::getInstance().deleteOpenRecent();
}

void MainWindow::reloadRundownMenu(const ReloadRundownMenuEvent& event)
{
    this->reloadRundownAction->setEnabled(event.getEnabled());
}

void MainWindow::emptyRundown(const EmptyRundownEvent& event)
{
    Q_UNUSED(event);

    this->saveAsPresetAction->setEnabled(false);
}

void MainWindow::activeRundownChanged(const ActiveRundownChangedEvent& event)
{
    QFileInfo info(event.getPath());
    if (info.baseName() == Rundown::DEFAULT_NAME)
        this->setWindowTitle(QString("%1").arg(this->applicationTitle));
    else
        this->setWindowTitle(QString("%1 - %2").arg(this->applicationTitle).arg(event.getPath()));
}

void MainWindow::newRundownMenu(const NewRundownMenuEvent& event)
{
    this->newRundownAction->setEnabled(event.getEnabled());
}

void MainWindow::openRundownMenu(const OpenRundownMenuEvent& event)
{
    this->openRundownAction->setEnabled(event.getEnabled());
}

void MainWindow::exportPresetMenu(const ExportPresetMenuEvent& event)
{
    this->exportPresetAction->setEnabled(event.getEnabled());
}

void MainWindow::saveAsPresetMenu(const SaveAsPresetMenuEvent& event)
{
    this->saveAsPresetAction->setEnabled(event.getEnabled());
}

void MainWindow::openRundownFromUrlMenu(const OpenRundownFromUrlMenuEvent& event)
{
    this->openRundownFromUrlAction->setEnabled(event.getEnabled());
}

void MainWindow::allowRemoteTriggering(const AllowRemoteTriggeringEvent& event)
{
    // We do not want to trigger check changed event.
    this->allowRemoteTriggeringAction->blockSignals(true);
    this->allowRemoteTriggeringAction->setChecked(event.getEnabled());
    this->allowRemoteTriggeringAction->blockSignals(false);
}

void MainWindow::lockRundown(const LockRundownEvent& event)
{
    this->lockRundownAction->blockSignals(true);
    this->lockRundownAction->setChecked(event.getLocked());
    this->lockRundownAction->blockSignals(false);
}

void MainWindow::compactView(const CompactViewEvent& event)
{
    // We do not want to trigger check changed event.
    this->compactViewAction->blockSignals(true);
    this->compactViewAction->setChecked(event.getEnabled());
    this->compactViewAction->blockSignals(false);
}

void MainWindow::repositoryRundown(const RepositoryRundownEvent& event)
{
    this->saveAction->setEnabled(!event.getRepositoryRundown());
    this->saveAsAction->setEnabled(!event.getRepositoryRundown());
    this->allowRemoteTriggeringAction->setEnabled(!event.getRepositoryRundown());
    this->insertRepositoryChangesAction->setEnabled(event.getRepositoryRundown());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (this->widgetRundown->checkForSaveBeforeQuit())
    {
        EventManager::getInstance().fireCloseApplicationEvent(CloseApplicationEvent());

        event->accept();
    }
    else
        event->ignore();
}

void MainWindow::importPreset()
{
    EventManager::getInstance().fireImportPresetEvent(ImportPresetEvent());
}

void MainWindow::exportPreset()
{
    EventManager::getInstance().fireExportPresetEvent(ExportPresetEvent());
}

void MainWindow::saveAsPreset()
{
    EventManager::getInstance().fireSaveAsPresetEvent(SaveAsPresetEvent());
}

void MainWindow::newRundown()
{
    EventManager::getInstance().fireNewRundownEvent(NewRundownEvent());
}

void MainWindow::openRundown()
{
    EventManager::getInstance().fireOpenRundownEvent(OpenRundownEvent());
}

void MainWindow::openRundownFromUrl()
{
    EventManager::getInstance().fireOpenRundownFromUrlEvent(OpenRundownFromUrlEvent());
}

void MainWindow::saveRundown()
{
    EventManager::getInstance().fireSaveRundownEvent(SaveRundownEvent(false));
}

void MainWindow::saveAsRundown()
{
    EventManager::getInstance().fireSaveRundownEvent(SaveRundownEvent(true));
}

void MainWindow::copyItemProperties()
{
    EventManager::getInstance().fireCopyItemPropertiesEvent(CopyItemPropertiesEvent());
}

void MainWindow::pasteItemProperties()
{
    EventManager::getInstance().firePasteItemPropertiesEvent(PasteItemPropertiesEvent());
}

void MainWindow::executeStop()
{
    EventManager::getInstance().fireExecutePlayoutCommandEvent(ExecutePlayoutCommandEvent(Playout::PlayoutType::Stop));
}

void MainWindow::executePlay()
{
    EventManager::getInstance().fireExecutePlayoutCommandEvent(ExecutePlayoutCommandEvent(Playout::PlayoutType::Play));
}

void MainWindow::executePlayNow()
{
    EventManager::getInstance().fireExecutePlayoutCommandEvent(ExecutePlayoutCommandEvent(Playout::PlayoutType::PlayNow));
}

void MainWindow::executeLoad()
{
    EventManager::getInstance().fireExecutePlayoutCommandEvent(ExecutePlayoutCommandEvent(Playout::PlayoutType::Load));
}

void MainWindow::executePause()
{
    EventManager::getInstance().fireExecutePlayoutCommandEvent(ExecutePlayoutCommandEvent(Playout::PlayoutType::PauseResume));
}

void MainWindow::executeNext()
{
    EventManager::getInstance().fireExecutePlayoutCommandEvent(ExecutePlayoutCommandEvent(Playout::PlayoutType::Next));
}

void MainWindow::executeUpdate()
{
    EventManager::getInstance().fireExecutePlayoutCommandEvent(ExecutePlayoutCommandEvent(Playout::PlayoutType::Update));
}

void MainWindow::executeInvoke()
{
    EventManager::getInstance().fireExecutePlayoutCommandEvent(ExecutePlayoutCommandEvent(Playout::PlayoutType::Invoke));
}

void MainWindow::executePreview()
{
    EventManager::getInstance().fireExecutePlayoutCommandEvent(ExecutePlayoutCommandEvent(Playout::PlayoutType::Preview));
}

void MainWindow::executeClear()
{
    EventManager::getInstance().fireExecutePlayoutCommandEvent(ExecutePlayoutCommandEvent(Playout::PlayoutType::Clear));
}

void MainWindow::executeClearVideolayer()
{
    EventManager::getInstance().fireExecutePlayoutCommandEvent(ExecutePlayoutCommandEvent(Playout::PlayoutType::ClearVideoLayer));
}

void MainWindow::executeClearChannel()
{
    EventManager::getInstance().fireExecutePlayoutCommandEvent(ExecutePlayoutCommandEvent(Playout::PlayoutType::ClearChannel));
}

void MainWindow::panicClearAll()
{
    if (!this->m_panicArmed)
    {
        this->m_panicArmed = true;
        this->m_panicTimer.start();
        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent("Press Esc again to CLEAR ALL outputs", 2000));

        // Auto-disarm after 500ms.
        QTimer::singleShot(500, this, [this]() { this->m_panicArmed = false; });
        return;
    }

    this->m_panicArmed = false;

    if (this->m_panicTimer.elapsed() > 500)
        return;

    QList<DeviceModel> devices = DeviceManager::getInstance().getDeviceModels();
    for (const DeviceModel& model : devices)
    {
        const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(model.getName());
        if (device == nullptr || !device->isConnected())
            continue;

        for (int ch = 1; ch <= model.getChannels(); ch++)
            device->clearChannel(ch);
    }

    EventManager::getInstance().fireStatusbarEvent(
        StatusbarEvent("PANIC: Cleared all outputs", 5000, true));
}

void MainWindow::markItemAsUsed()
{
    EventManager::getInstance().fireMarkItemAsUsedEvent(MarkItemAsUsedEvent());
}

void MainWindow::markItemAsUnused()
{
    EventManager::getInstance().fireMarkItemAsUnusedEvent(MarkItemAsUnusedEvent());
}

void MainWindow::markAllItemsAsUsed()
{
    EventManager::getInstance().fireMarkAllItemsAsUsedEvent(MarkAllItemsAsUsedEvent());
}

void MainWindow::markAllItemsAsUnused()
{
    EventManager::getInstance().fireMarkAllItemsAsUnusedEvent(MarkAllItemsAsUnusedEvent());
}

void MainWindow::compactView(bool enabled)
{
    EventManager::getInstance().fireCompactViewEvent(CompactViewEvent(enabled));
}

void MainWindow::allowRemoteTriggering(bool enabled)
{
    EventManager::getInstance().fireAllowRemoteTriggeringEvent(AllowRemoteTriggeringEvent(enabled));
}

void MainWindow::lockRundown(bool locked)
{
    EventManager::getInstance().fireLockRundownEvent(LockRundownEvent(locked));
}

void MainWindow::closeRundown()
{
    EventManager::getInstance().fireCloseRundownEvent(CloseRundownEvent());
}

void MainWindow::insertRepositoryChanges()
{
    EventManager::getInstance().fireInsertRepositoryChangesEvent(InsertRepositoryChangesEvent());
}

void MainWindow::reloadRundown()
{
    EventManager::getInstance().fireReloadRundownEvent(ReloadRundownEvent());
}

void MainWindow::showAboutDialog()
{
    AboutDialog* dialog = new AboutDialog(this);
    dialog->exec();
}

void MainWindow::showWhatsNewDialog()
{
    WhatsNewDialog* dialog = new WhatsNewDialog(this);
    dialog->exec();
    dialog->deleteLater();
}

void MainWindow::showHelpDialog()
{
    HelpDialog* dialog = new HelpDialog(this);
    dialog->exec();
}

void MainWindow::loadHotkeys()
{
    struct HotkeyMapping {
        Playout::PlayoutType type;
        QString configSuffix;
        QString displayName;
    };

    QList<HotkeyMapping> mappings = {
        { Playout::PlayoutType::Stop,            "Stop",             "Stop" },
        { Playout::PlayoutType::Play,            "Play",             "Play" },
        { Playout::PlayoutType::PlayNow,         "PlayNow",          "Play Now" },
        { Playout::PlayoutType::Load,            "Load",             "Load" },
        { Playout::PlayoutType::PauseResume,     "PauseResume",      "Pause / Resume" },
        { Playout::PlayoutType::Next,            "Next",             "Next" },
        { Playout::PlayoutType::Update,          "Update",           "Update" },
        { Playout::PlayoutType::Invoke,          "Invoke",           "Invoke" },
        { Playout::PlayoutType::Preview,         "Preview",          "Preview" },
        { Playout::PlayoutType::Clear,           "Clear",            "Clear" },
        { Playout::PlayoutType::ClearVideoLayer, "ClearVideoLayer",  "Clear Video Layer" },
        { Playout::PlayoutType::ClearChannel,    "ClearChannel",     "Clear Channel" },
    };

    // Bulk-load all config values in a single DB query (replaces ~47 individual SELECTs).
    QMap<QString, QString> cfg = DatabaseManager::getInstance().getAllConfigurations();
    auto cfgVal = [&cfg](const QString& key) -> QString { return cfg.value(key, QString()); };

    // Get the preview modifier prefix for registering modifier+hotkey variants.
    // When the user holds the preview modifier (e.g. Shift) and presses a hotkey (e.g. F2),
    // Qt sees a different key sequence (Shift+F2). By registering both F2 and Shift+F2
    // for the same action, the hotkey fires either way. shouldPreviewRedirect() detects
    // the held modifier via queryKeyboardModifiers() and routes to the preview channel.
    this->previewModifierKey = cfgVal("PreviewModifier");
    QString modPrefix;
    if (this->previewModifierKey == "Shift") modPrefix = "Shift+";
    else if (this->previewModifierKey == "Ctrl") modPrefix = "Ctrl+";
    else if (this->previewModifierKey == "Alt") modPrefix = "Alt+";

    for (const auto& mapping : mappings)
    {
        QAction* action = this->playoutActions.value(mapping.type, nullptr);
        if (!action)
            continue;

        QString primaryKey = cfgVal(QString("Hotkey%1").arg(mapping.configSuffix));
        QString altKey = cfgVal(QString("Hotkey%1Alt").arg(mapping.configSuffix));

        QList<QKeySequence> shortcuts;
        if (!primaryKey.isEmpty())
        {
            shortcuts << QKeySequence::fromString(primaryKey);
            if (!modPrefix.isEmpty())
                shortcuts << QKeySequence::fromString(modPrefix + primaryKey);
        }
        if (!altKey.isEmpty())
        {
            shortcuts << QKeySequence::fromString(altKey);
            if (!modPrefix.isEmpty())
                shortcuts << QKeySequence::fromString(modPrefix + altKey);
        }

        action->setShortcuts(shortcuts);

        // Show both shortcuts in the menu text when an alt is set.
        if (!primaryKey.isEmpty() && !altKey.isEmpty())
            action->setText(QString("%1 (%2)").arg(mapping.displayName, altKey));
        else
            action->setText(mapping.displayName);
    }

    // Load bank hotkeys.
    for (int i = 1; i <= TriggerBank::BANK_COUNT; i++)
    {
        QAction* bankAction = this->bankActions.value(i, nullptr);
        if (!bankAction)
            continue;

        QString primaryKey = cfgVal(QString("HotkeyBank%1").arg(i));
        QString altKey = cfgVal(QString("HotkeyBank%1Alt").arg(i));

        QList<QKeySequence> shortcuts;
        if (!primaryKey.isEmpty())
            shortcuts << QKeySequence::fromString(primaryKey);
        if (!altKey.isEmpty())
            shortcuts << QKeySequence::fromString(altKey);

        bankAction->setShortcuts(shortcuts);
    }

    // Load toggle preview hotkey.
    {
        QString primaryKey = cfgVal("HotkeyTogglePreview");
        QString altKey = cfgVal("HotkeyTogglePreviewAlt");

        QList<QKeySequence> shortcuts;
        if (!primaryKey.isEmpty())
            shortcuts << QKeySequence::fromString(primaryKey);
        if (!altKey.isEmpty())
            shortcuts << QKeySequence::fromString(altKey);

        this->togglePreviewAction->setShortcuts(shortcuts);
    }

    // Load toggle autostep hotkey.
    {
        QString primaryKey = cfgVal("HotkeyToggleAutostep");
        QString altKey = cfgVal("HotkeyToggleAutostepAlt");

        QList<QKeySequence> shortcuts;
        if (!primaryKey.isEmpty())
            shortcuts << QKeySequence::fromString(primaryKey);
        if (!altKey.isEmpty())
            shortcuts << QKeySequence::fromString(altKey);

        this->toggleAutostepAction->setShortcuts(shortcuts);
    }
}

void MainWindow::hotkeyChanged()
{
    loadHotkeys();
}

void MainWindow::showSettingsDialog()
{
    // Reset inspector panel.
    EventManager::getInstance().fireEmptyRundownEvent(EmptyRundownEvent());

    SettingsDialog* dialog = new SettingsDialog(this);
    QObject::connect(dialog, SIGNAL(gpiBindingChanged(int, Playout::PlayoutType)), this->widgetRundown, SLOT(gpiBindingChanged(int, Playout::PlayoutType)));
    QObject::connect(dialog, SIGNAL(hotkeyChanged()), this, SLOT(hotkeyChanged()));

    dialog->exec();
}

void MainWindow::toggleFullscreen()
{
    isFullScreen() ? setWindowState(Qt::WindowNoState) : setWindowState(Qt::WindowFullScreen);

    EventManager::getInstance().fireToggleFullscreenEvent(ToggleFullscreenEvent());
}

void MainWindow::disableCommandToggled(bool enabled)
{
    foreach (const DeviceModel& model, DeviceManager::getInstance().getDeviceModels())
    {
        const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(model.getName());
        if (device != NULL)
            device->setDisableCommands(enabled);
    }
}

void MainWindow::togglePreviewMode()
{
    bool current = EventManager::getInstance().getPreviewMode();
    EventManager::getInstance().setPreviewMode(!current);
    EventManager::getInstance().fireStatusbarEvent(
        StatusbarEvent(QString("PVW toggle: %1 → %2").arg(current).arg(!current)));
}

void MainWindow::toggleAutostepMode()
{
    bool current = EventManager::getInstance().getAutostepMode();
    EventManager::getInstance().setAutostepMode(!current);
    EventManager::getInstance().fireStatusbarEvent(
        StatusbarEvent(QString("STEP toggle: %1 → %2").arg(current).arg(!current)));
}

void MainWindow::previewModeActivated(bool active)
{
    this->previewModeActive = active;
    updatePreviewBorder();
}

void MainWindow::previewModifierActivated(bool held)
{
    this->previewModifierHeld = held;
    updatePreviewBorder();
}

void MainWindow::updatePreviewBorder()
{
    bool effective = (this->previewModeActive || this->previewModifierHeld) && this->showPreviewBorder;

    if (effective)
    {
        const QString& val = ColorCache::previewBorder();
        QColor color(val);
        if (!color.isValid() || val.isEmpty())
            color = QColor(230, 200, 40, 220);

        this->previewBorderOverlay->setStyleSheet(
            QString("border: 3px solid rgba(%1,%2,%3,%4); background: transparent;")
            .arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha()));
        this->previewBorderOverlay->setGeometry(Ui::MainWindow::centralWidget->rect());
        this->previewBorderOverlay->raise();
        this->previewBorderOverlay->show();
    }
    else
    {
        this->previewBorderOverlay->hide();
    }
}

void MainWindow::channelLockChanged(const QString& deviceName, int channel, bool locked)
{
    Q_UNUSED(deviceName);
    Q_UNUSED(channel);
    Q_UNUSED(locked);
    updateLockBorder();
}

void MainWindow::updateLockBorder()
{
    if (this->lockBorderOverlay == nullptr)
        return;

    // Collect all locked channels across devices and globals (deduped per channel).
    QMap<int, QStringList> lockedByChannel; // channel -> list of device names ("All" for global)
    foreach (const DeviceModel& dm, DeviceManager::getInstance().getDeviceModels())
    {
        QSet<int> chs = DeviceManager::getInstance().getLockedChannels(dm.getName());
        foreach (int ch, chs)
            lockedByChannel[ch].append(dm.getName());
    }
    foreach (int ch, DeviceManager::getInstance().getGlobalLockedChannels())
    {
        if (!lockedByChannel[ch].contains("All"))
            lockedByChannel[ch].prepend("All");
    }

    if (lockedByChannel.isEmpty())
    {
        this->lockBorderOverlay->hide();
        return;
    }

    // Build label text: "Locked: Ch1, Ch3, Ch5".
    QStringList parts;
    QList<int> channels = lockedByChannel.keys();
    std::sort(channels.begin(), channels.end());
    for (int ch : channels)
        parts.append(QString("Ch%1").arg(ch));
    QString labelText = QString::fromUtf8("\xf0\x9f\x94\x92 Locked: %1").arg(parts.join(", "));

    this->lockBorderOverlay->setStyleSheet(
        "background: transparent; border: 3px solid rgba(198, 40, 40, 230);");
    this->lockBorderOverlay->setGeometry(Ui::MainWindow::centralWidget->rect());

    this->lockBorderLabel->setText(labelText);
    this->lockBorderLabel->adjustSize();
    int labelW = this->lockBorderLabel->width();
    int labelH = this->lockBorderLabel->height();
    int borderW = this->lockBorderOverlay->width();
    // Position at top-center, just below the top border line.
    this->lockBorderLabel->setGeometry((borderW - labelW) / 2, 3, labelW, labelH);
    this->lockBorderLabel->show();

    this->lockBorderOverlay->raise();
    this->lockBorderOverlay->show();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    // Keep the preview/lock border overlays sized to centralWidget.
    if (obj == Ui::MainWindow::centralWidget && event->type() == QEvent::Resize)
    {
        if (this->previewBorderOverlay && this->previewBorderOverlay->isVisible())
            this->previewBorderOverlay->setGeometry(Ui::MainWindow::centralWidget->rect());
        if (this->lockBorderOverlay && this->lockBorderOverlay->isVisible())
        {
            this->lockBorderOverlay->setGeometry(Ui::MainWindow::centralWidget->rect());
            // Re-center the label.
            int labelW = this->lockBorderLabel->width();
            int labelH = this->lockBorderLabel->height();
            int borderW = this->lockBorderOverlay->width();
            this->lockBorderLabel->setGeometry((borderW - labelW) / 2, 3, labelW, labelH);
        }
    }

    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
    {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (!keyEvent->isAutoRepeat())
        {
            int key = keyEvent->key();
            if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt)
            {
                bool isConfiguredModifier =
                    (this->previewModifierKey == "Shift" && key == Qt::Key_Shift) ||
                    (this->previewModifierKey == "Ctrl" && key == Qt::Key_Control) ||
                    (this->previewModifierKey == "Alt" && key == Qt::Key_Alt);

                if (isConfiguredModifier)
                    EventManager::getInstance().setPreviewModifierHeld(event->type() == QEvent::KeyPress);
            }
        }
    }
    else if (event->type() == QEvent::WindowDeactivate)
    {
        // Clear modifier held state when the application window loses focus.
        EventManager::getInstance().setPreviewModifierHeld(false);
    }

    return QMainWindow::eventFilter(obj, event);
}

#if defined(Q_OS_WIN)
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_NCHITTEST)
    {
        // Extend the top resize zone so the window can be resized from the
        // top and top-left/right corners on Windows 11's thin borders.
        RECT rect;
        GetWindowRect(reinterpret_cast<HWND>(winId()), &rect);
        int x = GET_X_LPARAM(msg->lParam) - rect.left;
        int y = GET_Y_LPARAM(msg->lParam) - rect.top;
        int w = rect.right - rect.left;
        const int border = 6;

        if (y < border)
        {
            if (x < border)
                *result = HTTOPLEFT;
            else if (x >= w - border)
                *result = HTTOPRIGHT;
            else
                *result = HTTOP;
            return true;
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

static int defaultPanelHeight(const QString& id)
{
    if (id == "Preview") return Panel::DEFAULT_PREVIEW_HEIGHT;
    if (id == "Live" || id == "NDI") return Panel::DEFAULT_LIVE_HEIGHT;
    if (id == "AudioLevels") return Panel::DEFAULT_AUDIOLEVELS_HEIGHT;
    if (id == "Clock") return Panel::DEFAULT_CLOCK_HEIGHT;
    if (id == "Performance") return Panel::DEFAULT_PERFORMANCE_HEIGHT;
    if (id == "HttpLog") return Panel::DEFAULT_HTTPLOG_HEIGHT;
    return 0;
}

static int defaultCompactHeight(const QString& id)
{
    if (id == "Preview") return Panel::COMPACT_PREVIEW_HEIGHT;
    if (id == "Live") return Panel::COMPACT_LIVE_HEIGHT;
    if (id == "AudioLevels") return Panel::COMPACT_AUDIOLEVELS_HEIGHT;
    if (id == "Clock") return Panel::COMPACT_CLOCK_HEIGHT;
    if (id == "Performance") return Panel::COMPACT_PERFORMANCE_HEIGHT;
    if (id == "HttpLog") return Panel::COMPACT_HTTPLOG_HEIGHT;
    if (id == "Library") return 25;
    if (id == "NDI") return 25;
    return 25; // fallback: just the tab header
}

void MainWindow::rebuildLayout()
{
    // Suppress repaints during the full layout rebuild.
    this->setUpdatesEnabled(false);

    // Bulk-load all configuration into a local cache (single DB query instead of 50+).
    QMap<QString, QString> cfg = DatabaseManager::getInstance().getAllConfigurations();
    auto cfgVal = [&cfg](const QString& key) -> QString { return cfg.value(key, QString()); };

    // Local equivalents of PanelHelper functions using the cache.
    auto localPanelMode = [&cfgVal](const QString& id) -> QString {
        QString mode = cfgVal("PanelSizeMode_" + id);
        return mode.isEmpty() ? "fixed" : mode;
    };
    auto localPanelAnchor = [&cfgVal](const QString& id) -> QString {
        QString anchor = cfgVal("PanelAnchor_" + id);
        return anchor.isEmpty() ? "up" : anchor;
    };
    auto localPanelSpanCount = [&cfgVal](const QString& id) -> int {
        QString val = cfgVal("PanelSpan_" + id);
        if (val.isEmpty()) return 0;
        if (val == "right") return 1;
        bool ok = false;
        int count = val.toInt(&ok);
        return (ok && count > 0) ? count : 0;
    };
    auto localPanelSpanDirection = [&cfgVal](const QString& id) -> QString {
        QString val = cfgVal("PanelSpanDir_" + id);
        return (val == "left") ? "left" : "right";
    };

    // Read column order from cache.
    QString orderStr = cfgVal("LayoutColumnOrder");
    if (orderStr.isEmpty())
        orderStr = "panel1,mainwindow,panel2";
    QStringList columns = orderStr.split(",", Qt::SkipEmptyParts);

    // Ensure mainwindow is always present.
    if (!columns.contains("mainwindow"))
        columns.prepend("mainwindow");

    // Local resolveSpanColumns using the cached column order (avoids DB read).
    auto localResolveSpanColumns = [&columns](const QString& col, int spanCount,
                                              const QString& direction) -> QStringList {
        int idx = columns.indexOf(col);
        if (idx < 0 || spanCount <= 0)
            return {col};

        QList<int> group;
        group.append(idx);
        int added = 0;

        auto isBarrier = [&columns](int i) {
            return columns[i] == "mainwindow" || PanelHelper::columnDbKey(columns[i]).isEmpty();
        };

        if (direction == "left")
        {
            for (int i = idx - 1; i >= 0 && added < spanCount; i--)
            { if (isBarrier(i)) break; group.prepend(i); added++; }
            for (int i = idx + 1; i < columns.size() && added < spanCount; i++)
            { if (isBarrier(i)) break; group.append(i); added++; }
        }
        else
        {
            for (int i = idx + 1; i < columns.size() && added < spanCount; i++)
            { if (isBarrier(i)) break; group.append(i); added++; }
            for (int i = idx - 1; i >= 0 && added < spanCount; i++)
            { if (isBarrier(i)) break; group.prepend(i); added++; }
        }

        QStringList result;
        for (int ci : group)
            result.append(columns[ci]);
        return result;
    };

    // Access the 3 independent sub-panels from StatusPanelWidget.
    QWidget* serverTab = widgetStatusPanel->serverTabWidget();
    QWidget* activityTab = widgetStatusPanel->activityTabWidget();
    QWidget* banksTab = widgetStatusPanel->banksTabWidget();

    // Properly remove movable widgets from their current layouts before reparenting.
    QList<QWidget*> movable = {
        widgetAudioLevels, widgetPreview, widgetLibrary,
        widgetDuration, serverTab, activityTab, banksTab,
        widgetLive, widgetNdi, widgetPerformance, widgetHttpLog, widgetInspector,
        widgetStatusBar, widgetClock
    };
    for (auto* w : movable)
    {
        if (w->parentWidget() && w->parentWidget()->layout())
            w->parentWidget()->layout()->removeWidget(w);
        w->setParent(this);
        w->hide();
    }

    // Remove splitterHorizontal from its layout before reparenting.
    if (splitterHorizontal->parentWidget() && splitterHorizontal->parentWidget()->layout())
        splitterHorizontal->parentWidget()->layout()->removeWidget(splitterHorizontal);
    splitterHorizontal->setParent(this);

    // Keep widgetStatusPanel alive — it owns the serverTab/activityTab/banksTab
    // sub-widgets. Without this, deleting layoutWidget3 (its .ui parent) would
    // destroy it and cause a use-after-free crash.
    widgetStatusPanel->setParent(this);

    // Delete old column containers from the splitter immediately.
    while (splitterVertical->count() > 0)
    {
        QWidget* w = splitterVertical->widget(0);
        w->setParent(nullptr);
        delete w;
    }

    // DB key for each panel.
    QMap<QString, QString> panelDbKeys;
    panelDbKeys["panel1"] = "LayoutPanel1";
    panelDbKeys["panel2"] = "LayoutPanel2";
    panelDbKeys["panel3"] = "LayoutPanel3";
    panelDbKeys["panel4"] = "LayoutPanel4";

    QList<int> sizes;
    panelContainers.clear();

    // Helper: add a list of widgets to a column layout with sizing/anchor logic.
    // Returns (hasExpanding, hasAnchorDown).
    auto addWidgetsToColumn = [this, &cfgVal, &localPanelAnchor, &localPanelMode](
        QVBoxLayout* layout, const QStringList& widgetIds,
        QWidget* container) -> QPair<bool, bool>
    {
        bool hasExpanding = false;
        bool hasAnchorDown = false;
        for (int i = 0; i < widgetIds.size(); i++)
        {
            QString id = widgetIds[i].trimmed();
            QWidget* w = widgetById(id);
            if (!w)
                continue;

            if (localPanelAnchor(id) == "down" && !hasAnchorDown)
            {
                layout->addStretch();
                hasAnchorDown = true;
            }

            // If panel is collapsed, force compact height regardless of size mode.
            bool isCollapsed = cfgVal("PanelCollapsed_" + id) == "true";
            if (isCollapsed)
            {
                int compactH = defaultCompactHeight(id);
                if (compactH > 0)
                    w->setFixedHeight(compactH);
                layout->addWidget(w, 0);
            }
            else
            {
                QString mode = localPanelMode(id);

                if (mode == "expanding")
                {
                    w->setMinimumHeight(0);
                    w->setMaximumHeight(QWIDGETSIZE_MAX);
                    QSizePolicy sp = w->sizePolicy();
                    sp.setVerticalPolicy(QSizePolicy::Preferred);
                    w->setSizePolicy(sp);
                    layout->addWidget(w, 1);
                    hasExpanding = true;
                }
                else if (mode == "resizable")
                {
                    QString hStr = cfgVal(id + "PanelHeight");
                    if (!hStr.isEmpty())
                        w->setFixedHeight(hStr.toInt());
                    layout->addWidget(w, 0);

                    auto* handle = new PanelResizeHandle(w, id, w->height(), container);
                    layout->addWidget(handle);
                }
                else
                {
                    int h = w->property("panelFixedHeight").toInt();
                    if (h <= 0)
                        h = defaultPanelHeight(id);
                    if (h > 0)
                        w->setFixedHeight(h);
                    layout->addWidget(w, 0);
                }

                int minH = cfgVal("MinHeight_" + id).toInt();
                if (minH > 0)
                    w->setMinimumHeight(minH);
            }

            w->show();
        }
        return {hasExpanding, hasAnchorDown};
    };

    // Helper: create a side panel column widget from a list of widget IDs.
    auto buildSideColumn = [&](const QStringList& widgetIds) -> QWidget*
    {
        if (widgetIds.isEmpty())
            return nullptr;
        QWidget* container = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
#if defined(Q_OS_MAC)
        layout->setSpacing(5);
#else
        layout->setSpacing(4);
#endif
        auto [hasExpanding, hasAnchorDown] = addWidgetsToColumn(layout, widgetIds, container);
        if (!hasAnchorDown)
            layout->addStretch();
        return container;
    };

    // ── Pre-scan for span groups ─────────────────────────────
    // A span group merges N contiguous side panel columns into one compound
    // container. Span widgets go full-width; other widgets from all
    // columns fill sub-columns in sections between span widgets.
    struct SpanEntry {
        QString widgetId;
        int indexInCol;    // position in its column's widget list
        int colIdx;        // absolute column index
        QList<int> coveredGroupCols; // group-relative column indices this span covers
    };
    struct SpanInfo {
        QList<SpanEntry> spanWidgets;  // sorted by indexInCol
        QList<int> columnIndices;      // all column indices in this compound group (ordered)
    };
    // ── Phase 1: Determine column groupings (with merge) ──────
    // Walk all panel columns. Each span widget's resolved columns either
    // create a new group or merge into (possibly bridging) existing groups.
    QMap<int, QList<int>> groupMap; // primary (smallest idx) → ordered column indices

    for (int colIdx = 0; colIdx < columns.size(); colIdx++)
    {
        const QString& col = columns[colIdx];
        if (col == "mainwindow" || !panelDbKeys.contains(col))
            continue;

        QString dbKey = panelDbKeys[col];
        QString widgetStr = cfgVal(dbKey);
        QStringList widgetIds = widgetStr.split(",", Qt::SkipEmptyParts);
        for (const QString& rawId : widgetIds)
        {
            QString id = rawId.trimmed();
            int spanCount = localPanelSpanCount(id);
            if (spanCount <= 0)
                continue;

            QString spanDir = localPanelSpanDirection(id);
            QStringList spanCols = localResolveSpanColumns(col, spanCount, spanDir);
            if (spanCols.size() < 2)
                continue;

            QSet<int> newIndices;
            for (const QString& sc : spanCols)
            {
                int idx = columns.indexOf(sc);
                if (idx >= 0)
                    newIndices.insert(idx);
            }

            // Find all existing groups that overlap with newIndices.
            QList<int> overlappingPrimaries;
            for (auto it = groupMap.constBegin(); it != groupMap.constEnd(); ++it)
            {
                for (int ci : it.value())
                {
                    if (newIndices.contains(ci))
                    {
                        overlappingPrimaries.append(it.key());
                        break;
                    }
                }
            }

            if (!overlappingPrimaries.isEmpty())
            {
                // Merge all overlapping groups + new indices into one.
                QSet<int> merged = newIndices;
                for (int primary : overlappingPrimaries)
                {
                    for (int ci : groupMap[primary])
                        merged.insert(ci);
                    groupMap.remove(primary);
                }
                QList<int> mergedList(merged.begin(), merged.end());
                std::sort(mergedList.begin(), mergedList.end());
                groupMap[mergedList.first()] = mergedList;
            }
            else
            {
                // New group.
                QList<int> indices(newIndices.begin(), newIndices.end());
                std::sort(indices.begin(), indices.end());
                groupMap[indices.first()] = indices;
            }
        }
    }

    // ── Phase 2: Scan groups for span widgets & compute coverage ──
    QMap<int, SpanInfo> spanGroups;

    for (auto it = groupMap.constBegin(); it != groupMap.constEnd(); ++it)
    {
        SpanInfo info;
        info.columnIndices = it.value();

        // Scan all columns in the group for span widgets.
        for (int ci : info.columnIndices)
        {
            QString colWidgetStr = cfgVal(panelDbKeys[columns[ci]]);
            QStringList colWidgets = colWidgetStr.split(",", Qt::SkipEmptyParts);
            for (int j = 0; j < colWidgets.size(); j++)
            {
                QString wid = colWidgets[j].trimmed();
                if (localPanelSpanCount(wid) > 0)
                    info.spanWidgets.append({wid, j, ci});
            }
        }

        // Compute coveredGroupCols for each span widget.
        for (SpanEntry& se : info.spanWidgets)
        {
            int sc = localPanelSpanCount(se.widgetId);
            QString seDir = localPanelSpanDirection(se.widgetId);
            QStringList resolved = localResolveSpanColumns(columns[se.colIdx], sc, seDir);
            for (const QString& rc : resolved)
            {
                int absIdx = columns.indexOf(rc);
                int groupIdx = info.columnIndices.indexOf(absIdx);
                if (groupIdx >= 0)
                    se.coveredGroupCols.append(groupIdx);
            }
            std::sort(se.coveredGroupCols.begin(), se.coveredGroupCols.end());
        }

        std::sort(info.spanWidgets.begin(), info.spanWidgets.end(),
            [](const SpanEntry& a, const SpanEntry& b) { return a.indexInCol < b.indexInCol; });

        spanGroups[it.key()] = info;
    }

    // ── Build columns ────────────────────────────────────────
    QSet<int> skipColumns;
    for (auto it = spanGroups.constBegin(); it != spanGroups.constEnd(); ++it)
    {
        // Skip all non-primary columns in the group.
        for (int ci : it.value().columnIndices)
        {
            if (ci != it.key())
                skipColumns.insert(ci);
        }
    }

    for (int colIdx = 0; colIdx < columns.size(); colIdx++)
    {
        if (skipColumns.contains(colIdx))
            continue;

        const QString& col = columns[colIdx];

        if (col == "mainwindow")
        {
            // Main window column: contains the rundown / action splitter.
            QWidget* container = new QWidget();
            QVBoxLayout* layout = new QVBoxLayout(container);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(4);
            layout->addWidget(splitterHorizontal);
            splitterHorizontal->show();

            splitterVertical->addWidget(container);
            this->mainWindowContainer = container;
            sizes.append(1038);
        }
        else if (panelDbKeys.contains(col) && spanGroups.contains(colIdx))
        {
            // ── Compound span column (merges N contiguous columns) ──
            const SpanInfo& info = spanGroups[colIdx];

            // Read widget lists for all columns in the group.
            QList<QStringList> colWidgetLists;
            for (int ci : info.columnIndices)
            {
                colWidgetLists.append(
                    cfgVal(panelDbKeys[columns[ci]])
                        .split(",", Qt::SkipEmptyParts));
            }

            // Collect span widget IDs and build lookup map.
            QSet<QString> spanWidgetIdSet;
            QMap<QString, const SpanEntry*> spanEntryMap;
            for (const SpanEntry& se : info.spanWidgets)
            {
                spanWidgetIdSet.insert(se.widgetId);
                spanEntryMap[se.widgetId] = &se;
            }

            int numCols = colWidgetLists.size();

            // Helper: create a cell container with proper sizing and resize handle.
            auto createWidgetCell = [&](const QString& wid, QWidget* parent, bool isSpanWidget = false) -> QWidget*
            {
                QWidget* w = widgetById(wid);
                if (!w) return nullptr;

                QWidget* cell = new QWidget(parent);
                QVBoxLayout* cellLayout = new QVBoxLayout(cell);
                cellLayout->setContentsMargins(0, 0, 0, 0);
                cellLayout->setSpacing(0);

                if (isSpanWidget)
                {
                    // Span widgets: expanding inside cell + resize handle targets cell.
                    w->setMinimumHeight(0);
                    w->setMaximumHeight(QWIDGETSIZE_MAX);
                    QSizePolicy sp = w->sizePolicy();
                    sp.setVerticalPolicy(QSizePolicy::Preferred);
                    w->setSizePolicy(sp);
                    cellLayout->addWidget(w, 1);

                    auto* handle = new PanelResizeHandle(cell, wid, 300, cell);
                    cellLayout->addWidget(handle);

                    // Restore saved span height on the cell container.
                    QString hStr = cfgVal(wid + "PanelHeight");
                    if (!hStr.isEmpty())
                        cell->setFixedHeight(hStr.toInt());
                }
                else
                {
                    QString mode = localPanelMode(wid);
                    if (mode == "expanding")
                    {
                        w->setMinimumHeight(0);
                        w->setMaximumHeight(QWIDGETSIZE_MAX);
                        QSizePolicy sp = w->sizePolicy();
                        sp.setVerticalPolicy(QSizePolicy::Preferred);
                        w->setSizePolicy(sp);
                        cellLayout->addWidget(w, 1);
                    }
                    else if (mode == "resizable")
                    {
                        QString hStr = cfgVal(wid + "PanelHeight");
                        if (!hStr.isEmpty())
                            w->setFixedHeight(hStr.toInt());
                        cellLayout->addWidget(w, 0);

                        auto* handle = new PanelResizeHandle(w, wid, w->height(), cell);
                        cellLayout->addWidget(handle);
                    }
                    else
                    {
                        int h = w->property("panelFixedHeight").toInt();
                        if (h <= 0)
                            h = defaultPanelHeight(wid);
                        if (h > 0)
                            w->setFixedHeight(h);
                        cellLayout->addWidget(w, 0);
                    }
                }

                int minH = cfgVal("MinHeight_" + wid).toInt();
                if (minH > 0)
                    w->setMinimumHeight(minH);

                w->show();
                return cell;
            };

            // ── Hybrid grid: span rows + column segment containers ──
            // Span widgets get their own grid row with colSpan.
            // Non-span widgets are grouped into column containers (buildSideColumn)
            // so they flow naturally like normal side panel columns.
            QWidget* compound = new QWidget();
            QGridLayout* gridLayout = new QGridLayout(compound);
            gridLayout->setContentsMargins(0, 0, 0, 0);
            gridLayout->setVerticalSpacing(4);
            gridLayout->setHorizontalSpacing(4);

            for (int c = 0; c < numCols; c++)
                gridLayout->setColumnStretch(c, 1);

            QList<int> cursorPos(numCols, 0);
            int gridRow = 0;
            bool anyRowHasStretch = false;
            QList<QSplitter*> segSplitters;

            while (true)
            {
                bool anyRemaining = false;
                for (int c = 0; c < numCols; c++)
                    if (cursorPos[c] < colWidgetLists[c].size())
                        anyRemaining = true;
                if (!anyRemaining) break;

                // ── Check for span widget at any cursor ──
                const SpanEntry* foundSpan = nullptr;
                for (int c = 0; c < numCols; c++)
                {
                    if (cursorPos[c] >= colWidgetLists[c].size()) continue;
                    QString wid = colWidgetLists[c][cursorPos[c]].trimmed();
                    const SpanEntry* se = spanEntryMap.value(wid, nullptr);
                    if (se) { foundSpan = se; break; }
                }

                if (foundSpan)
                {
                    // ── Span row ──
                    int startCol = foundSpan->coveredGroupCols.first();
                    int colSpan = foundSpan->coveredGroupCols.size();

                    QWidget* cell = createWidgetCell(foundSpan->widgetId, compound, true);
                    if (cell)
                        gridLayout->addWidget(cell, gridRow, startCol, 1, colSpan);

                    // Row stretch: auto-expand if no saved height.
                    QString hStr = cfgVal(foundSpan->widgetId + "PanelHeight");
                    if (hStr.isEmpty())
                    {
                        gridLayout->setRowStretch(gridRow, 1);
                        anyRowHasStretch = true;
                    }

                    int homeGC = info.columnIndices.indexOf(foundSpan->colIdx);
                    cursorPos[homeGC]++;
                    gridRow++;
                    continue;
                }

                // ── Segment row: collect non-span widgets into column containers ──
                QMap<int, QStringList> segWidgets; // col -> widget IDs

                while (true)
                {
                    // Check if any column's next widget is a span → stop collecting.
                    bool spanAhead = false;
                    for (int c = 0; c < numCols; c++)
                    {
                        if (cursorPos[c] >= colWidgetLists[c].size()) continue;
                        QString wid = colWidgetLists[c][cursorPos[c]].trimmed();
                        if (spanEntryMap.contains(wid))
                        { spanAhead = true; break; }
                    }
                    if (spanAhead) break;

                    // Advance one widget per column.
                    bool anyAdvanced = false;
                    for (int c = 0; c < numCols; c++)
                    {
                        if (cursorPos[c] >= colWidgetLists[c].size()) continue;
                        segWidgets[c].append(colWidgetLists[c][cursorPos[c]].trimmed());
                        cursorPos[c]++;
                        anyAdvanced = true;
                    }
                    if (!anyAdvanced) break;
                }

                // Build column containers in a horizontal QSplitter.
                QSplitter* segSplitter = new QSplitter(Qt::Horizontal, compound);
                segSplitter->setHandleWidth(6);
                bool segHasExpanding = false;

                for (int c = 0; c < numCols; c++)
                {
                    if (segWidgets.contains(c))
                    {
                        QWidget* seg = buildSideColumn(segWidgets[c]);
                        if (seg)
                        {
                            segSplitter->addWidget(seg);
                            for (const QString& wid : segWidgets[c])
                                if (localPanelMode(wid) == "expanding")
                                    segHasExpanding = true;
                        }
                        else
                            segSplitter->addWidget(new QWidget());
                    }
                    else
                    {
                        segSplitter->addWidget(new QWidget());
                    }
                }

                gridLayout->addWidget(segSplitter, gridRow, 0, 1, numCols);
                segSplitters.append(segSplitter);

                if (segHasExpanding)
                {
                    gridLayout->setRowStretch(gridRow, 1);
                    anyRowHasStretch = true;
                }

                gridRow++;
            }

            // Sync column widths across segment splitters.
            for (QSplitter* s : segSplitters)
            {
                QObject::connect(s, &QSplitter::splitterMoved, compound,
                    [segSplitters, s](int, int) {
                        QList<int> sizes = s->sizes();
                        for (QSplitter* other : segSplitters)
                            if (other != s)
                                other->setSizes(sizes);
                    });
            }

            // Bottom stretch if no row has it (pushes content to top).
            if (!anyRowHasStretch)
                gridLayout->setRowStretch(gridRow, 1);

            splitterVertical->addWidget(compound);
            for (int ci : info.columnIndices)
                this->panelContainers[columns[ci]] = compound;
            sizes.append(200 * info.columnIndices.size());
        }
        else if (panelDbKeys.contains(col))
        {
            // Normal side panel column.
            QString dbKey = panelDbKeys[col];
            QString widgetStr = cfgVal(dbKey);
            QStringList widgetIds = widgetStr.split(",", Qt::SkipEmptyParts);
            if (widgetIds.isEmpty())
            {
                QString showEmpty = cfgVal("ShowEmptyPanels");
                if (showEmpty != "true")
                    continue;

                // Empty placeholder column.
                QWidget* container = new QWidget();
                QVBoxLayout* emptyLayout = new QVBoxLayout(container);
                emptyLayout->setContentsMargins(0, 0, 0, 0);
                emptyLayout->addStretch();
                splitterVertical->addWidget(container);
                this->panelContainers[col] = container;
                sizes.append(200);
                continue;
            }

            QWidget* container = buildSideColumn(widgetIds);
            if (!container)
                continue;

            splitterVertical->addWidget(container);
            this->panelContainers[col] = container;
            sizes.append(200);
        }
    }

    splitterVertical->setSizes(sizes);
    // Set stretch factors — skip indices for columns that were merged.
    int splitterIdx = 0;
    for (int colIdx = 0; colIdx < columns.size(); colIdx++)
    {
        if (skipColumns.contains(colIdx))
            continue;
        if (splitterIdx < splitterVertical->count())
        {
            splitterVertical->setStretchFactor(splitterIdx,
                columns[colIdx] == "mainwindow" ? 1 : 0);
            splitterIdx++;
        }
    }

    // Re-enable repaints and trigger a single consolidated repaint.
    this->setUpdatesEnabled(true);

    // Constrain after layout is rebuilt.
    QTimer::singleShot(0, this, [this]() { constrainToScreen(); });
}

void MainWindow::constrainToScreen()
{
    QScreen* screen = this->screen();
    if (screen == nullptr)
        return;

    QRect available = screen->availableGeometry();

    // Set maximum size so Qt layouts can never push the window beyond the screen.
    this->setMaximumSize(available.size());

    // Shrink if currently too large.
    if (this->height() > available.height() || this->width() > available.width())
    {
        this->resize(qMin(this->width(), available.width()),
                     qMin(this->height(), available.height()));
    }

    // Reposition if partially off-screen.
    if (this->y() < available.y())
        this->move(this->x(), available.y());
    if (this->x() < available.x())
        this->move(available.x(), this->y());
}

QWidget* MainWindow::widgetById(const QString& id)
{
    if (id == "AudioLevels") return widgetAudioLevels;
    if (id == "Preview") return widgetPreview;
    if (id == "Library") return widgetLibrary;
    if (id == "Duration") return widgetDuration;
    if (id == "ServerStatus") return widgetStatusPanel->serverTabWidget();
    if (id == "Activity") return widgetStatusPanel->activityTabWidget();
    if (id == "TriggerBanks") return widgetStatusPanel->banksTabWidget();
    if (id == "Live") return widgetLive;
    if (id == "NDI") return widgetNdi;
    if (id == "Performance") return widgetPerformance;
    if (id == "HttpLog") return widgetHttpLog;
    if (id == "Inspector") return widgetInspector;
    if (id == "StatusBar") return widgetStatusBar;
    if (id == "Clock") return widgetClock;
    return nullptr;
}

void MainWindow::refreshLibrary()
{
    EventManager::getInstance().fireRefreshLibraryEvent(RefreshLibraryEvent());
}
