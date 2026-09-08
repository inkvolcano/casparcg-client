#include "StatusPanelWidget.h"

#include "Global.h"
#include "PanelHelper.h"
#include "DeviceManager.h"
#include "EventManager.h"
#include "RelayClient.h"
#include "SheetCacheServer.h"
#include "TriggerBankRegistry.h"
#include "Timecode.h"
#include "Rundown/AbstractRundownWidget.h"

#include "CasparDevice.h"

#include <algorithm>

#include "DatabaseManager.h"

#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>
#include <QtCore/QSet>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTreeWidget>

StatusPanelWidget::StatusPanelWidget(QWidget* parent)
    : QWidget(parent),
      serverCollapsed(false),
      activityCollapsed(false),
      banksCollapsed(false),
      channelLockGrid(nullptr),
      maxChannels(0)
{
    setupUi(this);

    // All three sub-panels auto-grow to fit content.
    this->tabWidgetServer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    this->tabWidgetActivity->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    this->tabWidgetBanks->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    this->widgetActivity->setMinimumHeight(0);

    // Load minimum heights from DB.
    int actMinH = DatabaseManager::getInstance()
        .getConfigurationByName("MinHeight_Activity").getValue().toInt();
    if (actMinH > 0)
        this->tabWidgetActivity->setMinimumHeight(actMinH);

    int banksMinH = DatabaseManager::getInstance()
        .getConfigurationByName("MinHeight_TriggerBanks").getValue().toInt();
    if (banksMinH > 0)
        this->tabWidgetBanks->setMinimumHeight(banksMinH);

    // Load visibility settings from DB (before setupMenus so actions get correct initial state).
    QString val;
    val = DatabaseManager::getInstance().getConfigurationByName("ShowPVWButton").getValue();
    this->showPVW = val.isEmpty() || val == "true";
    val = DatabaseManager::getInstance().getConfigurationByName("ShowServers").getValue();
    this->showServers = val.isEmpty() || val == "true";
    val = DatabaseManager::getInstance().getConfigurationByName("ShowChannelLocks").getValue();
    this->showChannelLocks = val.isEmpty() || val == "true";
    val = DatabaseManager::getInstance().getConfigurationByName("ShowChannelHeaders").getValue();
    this->showChannelHeaders = val.isEmpty() || val == "true";
    val = DatabaseManager::getInstance().getConfigurationByName("ShowBankIcons").getValue();
    this->showBankIcons = val.isEmpty() || val == "true";
    val = DatabaseManager::getInstance().getConfigurationByName("DisconnectMode").getValue();
    this->disconnectMode = val.isEmpty() ? "ask" : val;
    val = DatabaseManager::getInstance().getConfigurationByName("ShowSTEPButton").getValue();
    this->showSTEP = val.isEmpty() || val == "true";
    val = DatabaseManager::getInstance().getConfigurationByName("ShowNoActivity").getValue();
    this->showNoActivity = val.isEmpty() || val == "true";
    val = DatabaseManager::getInstance().getConfigurationByName("BigBoldMode").getValue();
    this->bigBoldMode = (val == "true");
    EventManager::getInstance().setBigBoldMode(this->bigBoldMode);

    setupMenus();
    setupServerPanel();
    setupActivityPanel();
    setupBanksPanel();

    // Apply initial visibility for elements created in setup methods.
    this->previewModeButton->setVisible(this->showPVW);
    this->autostepModeButton->setVisible(this->showSTEP);
    this->bankIconsContainer->setVisible(this->showBankIcons);

    // Cleanup timer for stale activity entries.
    this->cleanupTimer = new QTimer(this);
    this->cleanupTimer->setInterval(500);
    QObject::connect(this->cleanupTimer, SIGNAL(timeout()), this, SLOT(cleanupStaleEntries()));
    this->cleanupTimer->start();

    // Connect playback progress event.
    QObject::connect(&EventManager::getInstance(), SIGNAL(playbackProgress(const PlaybackProgressEvent&)),
                     this, SLOT(playbackProgress(const PlaybackProgressEvent&)));

    // Connect channel activity event.
    QObject::connect(&EventManager::getInstance(), SIGNAL(channelActivity(const ChannelActivityEvent&)),
                     this, SLOT(channelActivity(const ChannelActivityEvent&)));

    // Connect auto-loop countdown event.
    QObject::connect(&EventManager::getInstance(), SIGNAL(autoLoopCountdown(const AutoLoopCountdownEvent&)),
                     this, SLOT(autoLoopCountdown(const AutoLoopCountdownEvent&)));

    // Clear CH / Clear VL / Clear Output wipe every row on the affected channel(/layer).
    QObject::connect(&EventManager::getInstance(), &EventManager::channelCleared,
                     this, &StatusPanelWidget::channelCleared);

    // Connect bank assignment changed event.
    QObject::connect(&EventManager::getInstance(), SIGNAL(bankAssignmentChanged(const BankAssignmentChangedEvent&)),
                     this, SLOT(bankAssignmentChanged(const BankAssignmentChangedEvent&)));

    // Connect device manager signals.
    QObject::connect(&DeviceManager::getInstance(), SIGNAL(deviceAdded(CasparDevice&)),
                     this, SLOT(deviceAdded(CasparDevice&)));
    QObject::connect(&DeviceManager::getInstance(), SIGNAL(deviceRemoved()),
                     this, SLOT(deviceRemoved()));
    QObject::connect(&DeviceManager::getInstance(), SIGNAL(channelLockChanged(const QString&, int, bool)),
                     this, SLOT(channelLockChanged(const QString&, int, bool)));
    QObject::connect(&DeviceManager::getInstance(), SIGNAL(timedLockTick(const QString&, int, int)),
                     this, SLOT(timedLockTick(const QString&, int, int)));

    // Connect preview mode changed signal.
    QObject::connect(&EventManager::getInstance(), SIGNAL(previewModeChanged(bool)),
                     this, SLOT(previewModeChanged(bool)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(previewModifierHeld(bool)),
                     this, SLOT(previewModifierHeld(bool)));

    // Connect autostep mode changed signal.
    QObject::connect(&EventManager::getInstance(), SIGNAL(autostepModeChanged(bool)),
                     this, SLOT(autostepModeChanged(bool)));

    // Connect big bold mode changed signal.
    QObject::connect(&EventManager::getInstance(), SIGNAL(bigBoldModeChanged(bool)),
                     this, SLOT(bigBoldModeChangedSlot(bool)));

}

void StatusPanelWidget::setupServerPanel()
{
    // Set up Server content layout.
    QVBoxLayout* serverOuterLayout = new QVBoxLayout(this->widgetServer);
    serverOuterLayout->setContentsMargins(4, 2, 4, 2);
    serverOuterLayout->setSpacing(4);

    this->serverLayout = new QVBoxLayout();
    this->serverLayout->setSpacing(2);
    serverOuterLayout->addLayout(this->serverLayout);

    // Preview mode toggle button.
    this->previewModeButton = new QPushButton("PVW");
    this->previewModeButton->setCheckable(true);
    this->previewModeButton->setFocusPolicy(Qt::NoFocus);
    this->previewModeButton->setFixedHeight(24);
    this->previewModeButton->setStyleSheet(
        "QPushButton { background-color: rgba(50, 50, 50, 200); color: rgba(100, 100, 100, 200); "
        "border-radius: 3px; font-size: 10px; font-weight: bold; border: 1px solid rgba(70, 70, 70, 200); }"
        "QPushButton:hover { background-color: rgba(70, 70, 70, 200); }");
    QObject::connect(this->previewModeButton, &QPushButton::clicked, [](bool checked) {
        EventManager::getInstance().setPreviewMode(checked);
    });
    serverOuterLayout->addWidget(this->previewModeButton);

    // Autostep mode toggle button.
    this->autostepModeButton = new QPushButton("STEP");
    this->autostepModeButton->setCheckable(true);
    this->autostepModeButton->setFocusPolicy(Qt::NoFocus);
    this->autostepModeButton->setFixedHeight(24);
    this->autostepModeButton->setStyleSheet(
        "QPushButton { background-color: rgba(50, 50, 50, 200); color: rgba(100, 100, 100, 200); "
        "border-radius: 3px; font-size: 10px; font-weight: bold; border: 1px solid rgba(70, 70, 70, 200); }"
        "QPushButton:hover { background-color: rgba(70, 70, 70, 200); }");
    QObject::connect(this->autostepModeButton, &QPushButton::clicked, [](bool checked) {
        EventManager::getInstance().setAutostepMode(checked);
    });
    serverOuterLayout->addWidget(this->autostepModeButton);

    setupCacheRow(serverOuterLayout);
    setupRelayRow(serverOuterLayout);

    serverOuterLayout->addStretch();
}

// Same shape as a server row \xe2\x80\x94 name, light, button \xe2\x80\x94 because it answers the same
// question: is this thing responding right now, and can I change that from here.
void StatusPanelWidget::setupCacheRow(QVBoxLayout* serverOuterLayout)
{
    this->cacheRow = new QWidget(this->widgetServer);
    QHBoxLayout* rowLayout = new QHBoxLayout(this->cacheRow);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(6);

    this->cacheLabel = new QLabel(this->cacheRow);
    this->cacheLabel->setStyleSheet("font-size: 10px; color: rgba(200, 200, 200, 200);");
    this->cacheLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rowLayout->addWidget(this->cacheLabel, 1);

    this->cacheDot = new QLabel(this->cacheRow);
    this->cacheDot->setFixedSize(12, 12);
    rowLayout->addWidget(this->cacheDot, 0);

    // The switch sits where the light is, so throwing it does not mean going
    // looking for a menu mid-show.
    this->cacheBypassButton = new QPushButton(this->cacheRow);
    this->cacheBypassButton->setFixedHeight(20);
    this->cacheBypassButton->setFocusPolicy(Qt::NoFocus);
    this->cacheBypassButton->setStyleSheet(
        "QPushButton { font-size: 9px; padding: 1px 8px; border-radius: 3px; "
        "background-color: rgba(60, 60, 60, 200); color: rgba(200, 200, 200, 200); border: 1px solid rgba(80, 80, 80, 200); }"
        "QPushButton:hover { background-color: rgba(80, 80, 80, 200); }");
    QObject::connect(this->cacheBypassButton, &QPushButton::clicked, this, [this]() {
        SheetCacheServer::setBypassing(!SheetCacheServer::isBypassing());
        updateCacheStatus();
    });
    rowLayout->addWidget(this->cacheBypassButton, 0);

    serverOuterLayout->addWidget(this->cacheRow);
    this->cacheRow->setVisible(false);

    // Bypass can be thrown from the menu, the settings or over HTTP, so the light
    // is read from the server rather than remembered from the last click here.
    QObject::connect(&this->cacheStatusTimer, &QTimer::timeout, this, &StatusPanelWidget::updateCacheStatus);
    this->cacheStatusTimer.start(2000);

    updateCacheStatus();
}

void StatusPanelWidget::updateCacheStatus()
{
    if (this->cacheRow == nullptr)
        return;

    // Nothing to report for a client that is not hosting; the row stays out of the way.
    if (!SheetCacheServer::isEnabled())
    {
        this->cacheRow->setVisible(false);
        return;
    }

    this->cacheRow->setVisible(true);

    bool listening = SheetCacheServer::getInstance().isRunning();
    bool bypassing = SheetCacheServer::isBypassing();
    int port = SheetCacheServer::configuredPort();

    const QString green = "background-color: rgb(76, 175, 80); border-radius: 6px;";
    const QString amber = "background-color: rgb(230, 160, 30); border-radius: 6px;";
    const QString red = "background-color: rgb(198, 40, 40); border-radius: 6px;";

    if (!listening)
    {
        // Enabled but not answering: almost always the port already belongs to
        // something else, which is worth saying rather than leaving as a dark light.
        this->cacheDot->setStyleSheet(red);
        this->cacheLabel->setText(QString("Cache %1").arg(port));
        this->cacheRow->setToolTip(QString(
            "The sheet cache is enabled but not listening on port %1.\n"
            "Another service probably has the port \xe2\x80\x94 the PHP server uses 3000 too.").arg(port));
        this->cacheBypassButton->setText("Off");
        this->cacheBypassButton->setEnabled(false);
        return;
    }

    this->cacheBypassButton->setEnabled(true);

    if (bypassing)
    {
        // Amber, not red: it is doing exactly what was asked of it. A red light here
        // would read as a fault every time somebody deliberately went live.
        this->cacheDot->setStyleSheet(amber);
        this->cacheLabel->setText(QString("Cache %1  bypass").arg(port));
        this->cacheRow->setToolTip(QString(
            "Serving nothing on purpose: reads answer 404 so graphics go live to the sheet,\n"
            "while writes still land and keep the cache warm.\n"
            "Listening on port %1.").arg(port));
        this->cacheBypassButton->setText("Serve");
        return;
    }

    this->cacheDot->setStyleSheet(green);
    this->cacheLabel->setText(QString("Cache %1").arg(port));
    this->cacheRow->setToolTip(QString(
        "Serving cached sheet rows on port %1.\n"
        "Templates pointed here (local = true) read from the cache first.").arg(port));
    this->cacheBypassButton->setText("Bypass");
}


// The same shape again, for the same reason: an operator at a venue should be able
// to see whether templates are arriving without opening a settings dialog mid-show.
void StatusPanelWidget::setupRelayRow(QVBoxLayout* serverOuterLayout)
{
    this->relayRow = new QWidget(this->widgetServer);
    QHBoxLayout* rowLayout = new QHBoxLayout(this->relayRow);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(6);

    this->relayLabel = new QLabel(this->relayRow);
    this->relayLabel->setStyleSheet("font-size: 10px; color: rgba(200, 200, 200, 200);");
    this->relayLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rowLayout->addWidget(this->relayLabel, 1);

    this->relayDot = new QLabel(this->relayRow);
    this->relayDot->setFixedSize(12, 12);
    rowLayout->addWidget(this->relayDot, 0);

    this->relayCheckButton = new QPushButton("Check", this->relayRow);
    this->relayCheckButton->setFixedHeight(20);
    this->relayCheckButton->setFocusPolicy(Qt::NoFocus);
    this->relayCheckButton->setStyleSheet(
        "QPushButton { font-size: 9px; padding: 1px 8px; border-radius: 3px; "
        "background-color: rgba(60, 60, 60, 200); color: rgba(200, 200, 200, 200); border: 1px solid rgba(80, 80, 80, 200); }"
        "QPushButton:hover { background-color: rgba(80, 80, 80, 200); }");
    QObject::connect(this->relayCheckButton, &QPushButton::clicked, this, []() {
        RelayClient::getInstance().checkNow();
    });
    rowLayout->addWidget(this->relayCheckButton, 0);

    serverOuterLayout->addWidget(this->relayRow);
    this->relayRow->setVisible(false);

    // Shares the cache row's timer: both answer "is this working right now", and a
    // second timer for the same question would be a second thing to keep in step.
    QObject::connect(&this->cacheStatusTimer, &QTimer::timeout, this, &StatusPanelWidget::updateRelayStatus);

    updateRelayStatus();
}

void StatusPanelWidget::updateRelayStatus()
{
    if (this->relayRow == nullptr)
        return;

    // A client that does not pull has nothing to report; the row stays out of the way.
    if (!RelayClient::isEnabled())
    {
        this->relayRow->setVisible(false);
        return;
    }

    this->relayRow->setVisible(true);

    const QString green = "background-color: rgb(76, 175, 80); border-radius: 6px;";
    const QString amber = "background-color: rgb(230, 160, 30); border-radius: 6px;";
    const QString red = "background-color: rgb(198, 40, 40); border-radius: 6px;";
    const QString grey = "background-color: rgb(90, 90, 90); border-radius: 6px;";

    RelayClient& relay = RelayClient::getInstance();

    if (relay.isBusy())
    {
        this->relayDot->setStyleSheet(amber);
        this->relayLabel->setText(QString("%1 checking").arg(RelayClient::isGitHub() ? "GitHub" : "Relay"));
        this->relayRow->setToolTip("Reading the source now.");
        this->relayCheckButton->setEnabled(false);
        return;
    }

    this->relayCheckButton->setEnabled(true);

    QDateTime ran = relay.lastRun();
    if (!ran.isValid())
    {
        // Enabled but never run: grey rather than red, because nothing has gone
        // wrong yet and a red light on startup would be read as a fault.
        this->relayDot->setStyleSheet(grey);
        this->relayLabel->setText(QString("%1 not checked yet").arg(RelayClient::isGitHub() ? "GitHub" : "Relay"));
        this->relayRow->setToolTip(QString("Pulling from %1.\nNothing has been checked since this client started.")
            .arg(RelayClient::sourceLabel()));
        return;
    }

    // Relative, because "nine minutes ago" answers the question and a timestamp
    // makes the reader do the subtraction.
    qint64 seconds = ran.secsTo(QDateTime::currentDateTime());
    QString ago = (seconds < 60) ? QString("just now")
                : (seconds < 3600) ? QString("%1m ago").arg(seconds / 60)
                : QString("%1h ago").arg(seconds / 3600);

    this->relayDot->setStyleSheet(relay.lastCheckOk() ? green : red);
    this->relayLabel->setText(QString("%1 %2").arg(RelayClient::isGitHub() ? "GitHub" : "Relay", ago));
    this->relayRow->setToolTip(QString("Pulling from %1.\nLast check %2: %3.")
        .arg(RelayClient::sourceLabel(), ago, relay.lastSummary()));
}

void StatusPanelWidget::setupActivityPanel()
{
    // Set up Activity content layout.
    this->activityOuterLayout = new QVBoxLayout(this->widgetActivity);
    this->activityOuterLayout->setContentsMargins(4, 2, 4, 2);
    this->activityOuterLayout->setSpacing(2);

    this->activityLayout = new QVBoxLayout();
    this->activityLayout->setSpacing(this->bigBoldMode ? 5 : 2);
    this->activityOuterLayout->addLayout(this->activityLayout);
    this->activityOuterLayout->addStretch();  // Push activities to top

    updateNoActivityLabel();
}

void StatusPanelWidget::setupBanksPanel()
{
    // Set up Banks content layout.
    QVBoxLayout* banksOuterLayout = new QVBoxLayout(this->widgetBanks);
    banksOuterLayout->setContentsMargins(4, 2, 4, 2);
    banksOuterLayout->setSpacing(4);

    // Create horizontal row of bank icons (B1-B9).
    this->bankIconsContainer = new QWidget(this->widgetBanks);
    QHBoxLayout* iconsLayout = new QHBoxLayout(this->bankIconsContainer);
    iconsLayout->setContentsMargins(0, 4, 0, 0);
    iconsLayout->setSpacing(4);

    for (int i = 0; i < BANK_COUNT; i++)
    {
        this->bankIcons[i] = new QLabel(this->bankIconsContainer);
        this->bankIcons[i]->setText(QString("B%1").arg(i + 1));
        this->bankIcons[i]->setAlignment(Qt::AlignCenter);
        this->bankIcons[i]->setFixedHeight(20);
        this->bankIcons[i]->setStyleSheet(
            "background-color: rgba(60, 60, 60, 200); "
            "color: rgba(100, 100, 100, 200); "
            "border-radius: 3px; "
            "font-size: 10px; "
            "font-weight: bold;");
        iconsLayout->addWidget(this->bankIcons[i], 1);
    }

    banksOuterLayout->addWidget(this->bankIconsContainer);

    // Create layout for bank entries (below the icons).
    this->banksEntriesLayout = new QVBoxLayout();
    this->banksEntriesLayout->setSpacing(2);
    banksOuterLayout->addLayout(this->banksEntriesLayout);
    banksOuterLayout->addStretch();  // Push entries to top
}

void StatusPanelWidget::setupMenus()
{
    // Server hamburger menu.
    this->serverMenu = new QMenu(this);
    this->serverMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->serverMenu, "ServerStatus", this);
    this->serverMenu->addSeparator();
    this->serverExpandCollapseAction = this->serverMenu->addAction("Collapse", this, &StatusPanelWidget::toggleServerCollapse);

    this->serverMenuButton = new QToolButton(this->tabWidgetServer);
    this->serverMenuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->serverMenuButton->setFixedSize(22, 22);
    this->serverMenuButton->setMenu(this->serverMenu);
    this->serverMenuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetServer->setCornerWidget(this->serverMenuButton);

    // Activity hamburger menu.
    this->activityMenu = new QMenu(this);
    this->activityMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->activityMenu, "Activity", this);
    this->activityMenu->addSeparator();

    // "Show No Activity" toggle.
    QAction* noActivityAction = this->activityMenu->addAction("Show No Activity");
    noActivityAction->setCheckable(true);
    noActivityAction->setChecked(this->showNoActivity);
    QObject::connect(noActivityAction, &QAction::toggled, this, [this](bool checked) {
        this->showNoActivity = checked;
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "ShowNoActivity", checked ? "true" : "false"));
        updateNoActivityLabel();
    });

    // "Big & Bold" display mode toggle.
    QAction* bigBoldAction = this->activityMenu->addAction("Big & Bold Mode");
    bigBoldAction->setCheckable(true);
    bigBoldAction->setChecked(this->bigBoldMode);
    QObject::connect(bigBoldAction, &QAction::toggled, this, [this](bool checked) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "BigBoldMode", checked ? "true" : "false"));
        EventManager::getInstance().setBigBoldMode(checked);
    });

    // "Minimum Height" submenu with presets.
    {
        QMenu* minHeightMenu = new QMenu("Minimum Height", this);
        QActionGroup* minHGroup = new QActionGroup(minHeightMenu);
        minHGroup->setExclusive(true);
        int currentMinH = DatabaseManager::getInstance()
            .getConfigurationByName("MinHeight_Activity").getValue().toInt();
        for (int h : {0, 30, 50, 80, 100})
        {
            QAction* a = minHeightMenu->addAction(h == 0 ? "None" : QString("%1 px").arg(h));
            a->setCheckable(true);
            a->setData(h);
            a->setChecked(h == currentMinH);
            minHGroup->addAction(a);
        }
        QObject::connect(minHGroup, &QActionGroup::triggered, this, [this](QAction* action) {
            int h = action->data().toInt();
            DatabaseManager::getInstance().updateConfiguration(
                ConfigurationModel(0, "MinHeight_Activity", QString::number(h)));
            this->tabWidgetActivity->setMinimumHeight(h);
        });
        this->activityMenu->addMenu(minHeightMenu);
    }

    this->activityMenu->addSeparator();
    this->activityExpandCollapseAction = this->activityMenu->addAction("Collapse", this, &StatusPanelWidget::toggleActivityCollapse);

    this->activityMenuButton = new QToolButton(this->tabWidgetActivity);
    this->activityMenuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->activityMenuButton->setFixedSize(22, 22);
    this->activityMenuButton->setMenu(this->activityMenu);
    this->activityMenuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetActivity->setCornerWidget(this->activityMenuButton);

    // Banks hamburger menu.
    this->banksMenu = new QMenu(this);
    this->banksMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->banksMenu, "TriggerBanks", this);
    this->banksMenu->addSeparator();

    // "Minimum Height" submenu with presets.
    {
        QMenu* banksMinMenu = new QMenu("Minimum Height", this);
        QActionGroup* banksMinHGroup = new QActionGroup(banksMinMenu);
        banksMinHGroup->setExclusive(true);
        int currentBanksMinH = DatabaseManager::getInstance()
            .getConfigurationByName("MinHeight_TriggerBanks").getValue().toInt();
        for (int h : {0, 30, 50, 80, 100})
        {
            QAction* a = banksMinMenu->addAction(h == 0 ? "None" : QString("%1 px").arg(h));
            a->setCheckable(true);
            a->setData(h);
            a->setChecked(h == currentBanksMinH);
            banksMinHGroup->addAction(a);
        }
        QObject::connect(banksMinHGroup, &QActionGroup::triggered, this, [this](QAction* action) {
            int h = action->data().toInt();
            DatabaseManager::getInstance().updateConfiguration(
                ConfigurationModel(0, "MinHeight_TriggerBanks", QString::number(h)));
            this->tabWidgetBanks->setMinimumHeight(h);
        });
        this->banksMenu->addMenu(banksMinMenu);
    }

    this->banksMenu->addSeparator();
    this->banksExpandCollapseAction = this->banksMenu->addAction("Collapse", this, &StatusPanelWidget::toggleBanksCollapse);

    this->banksMenuButton = new QToolButton(this->tabWidgetBanks);
    this->banksMenuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->banksMenuButton->setFixedSize(22, 22);
    this->banksMenuButton->setMenu(this->banksMenu);
    this->banksMenuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetBanks->setCornerWidget(this->banksMenuButton);
}

void StatusPanelWidget::toggleServerCollapse()
{
    this->serverCollapsed = !this->serverCollapsed;
    this->widgetServer->setVisible(!this->serverCollapsed);
    this->serverExpandCollapseAction->setText(this->serverCollapsed ? "Expand" : "Collapse");

    if (this->serverCollapsed)
    {
        this->tabWidgetServer->setFixedHeight(Panel::COMPACT_AUDIOLEVELS_HEIGHT);
    }
    else
    {
        this->tabWidgetServer->setMinimumHeight(0);
        this->tabWidgetServer->setMaximumHeight(QWIDGETSIZE_MAX);
    }
}

void StatusPanelWidget::toggleActivityCollapse()
{
    this->activityCollapsed = !this->activityCollapsed;
    this->widgetActivity->setVisible(!this->activityCollapsed);
    this->activityExpandCollapseAction->setText(this->activityCollapsed ? "Expand" : "Collapse");

    if (this->activityCollapsed)
    {
        this->tabWidgetActivity->setFixedHeight(Panel::COMPACT_AUDIOLEVELS_HEIGHT);
    }
    else
    {
        int minH = DatabaseManager::getInstance()
            .getConfigurationByName("MinHeight_Activity").getValue().toInt();
        this->tabWidgetActivity->setMinimumHeight(qMax(minH, 0));
        this->tabWidgetActivity->setMaximumHeight(QWIDGETSIZE_MAX);
    }
}

void StatusPanelWidget::toggleBanksCollapse()
{
    this->banksCollapsed = !this->banksCollapsed;
    this->widgetBanks->setVisible(!this->banksCollapsed);
    this->banksExpandCollapseAction->setText(this->banksCollapsed ? "Expand" : "Collapse");

    if (this->banksCollapsed)
    {
        this->tabWidgetBanks->setFixedHeight(Panel::COMPACT_AUDIOLEVELS_HEIGHT);
    }
    else
    {
        int minH = DatabaseManager::getInstance()
            .getConfigurationByName("MinHeight_TriggerBanks").getValue().toInt();
        this->tabWidgetBanks->setMinimumHeight(qMax(minH, 0));
        this->tabWidgetBanks->setMaximumHeight(QWIDGETSIZE_MAX);
    }
}

QString StatusPanelWidget::typeBadgeColor(const QString& itemType) const
{
    if (itemType == "STILL")
        return "rgba(255, 165, 0, 200)";   // orange
    if (itemType == "TEMPLATE")
        return "rgba(76, 175, 80, 200)";    // green
    if (itemType == "HTML")
        return "rgba(0, 188, 212, 200)";    // cyan
    if (itemType == "IMAGESCROLLER")
        return "rgba(156, 39, 176, 200)";   // purple
    if (itemType == "AUDIO")
        return "rgba(255, 235, 59, 200)";   // yellow
    if (itemType == "MOVIE")
        return "rgba(33, 150, 243, 200)";   // blue
    if (itemType == "SOLIDCOLOR")
        return "rgba(121, 85, 72, 200)";    // brown
    if (itemType == "DECKLINKINPUT")
        return "rgba(255, 87, 34, 200)";    // deep orange
    if (itemType == "ROUTECHANNEL" || itemType == "ROUTEVIDEOLAYER")
        return "rgba(96, 125, 139, 200)";   // blue-grey

    return "rgba(158, 158, 158, 200)";      // grey default
}

QString StatusPanelWidget::typeBadgeLabel(const QString& itemType) const
{
    if (itemType == "STILL")
        return "IMAGE";
    if (itemType == "MOVIE")
        return "VIDEO";
    return itemType;
}

QString StatusPanelWidget::channelColorStyle(int channel) const
{
    QColor color = QColor::fromHslF(ChannelColor::hue(channel) / 360.0, ChannelColor::saturation(), ChannelColor::lightness());
    int fs = this->bigBoldMode ? 14 : 10;
    return QString("background-color: %1; color: white; font-size: %2px; font-weight: bold;").arg(color.name()).arg(fs);
}

void StatusPanelWidget::removeActivityEntry(const QString& key)
{
    if (!this->activityEntries.contains(key))
        return;

    ActivityEntry& entry = this->activityEntries[key];
    if (entry.rowFadeAnim != nullptr)
    {
        entry.rowFadeAnim->stop();
        delete entry.rowFadeAnim;
        entry.rowFadeAnim = nullptr;
    }
    if (entry.animation != nullptr)
        entry.animation->stop();
    this->activityLayout->removeWidget(entry.row);
    delete entry.row;
    this->activityEntries.remove(key);
    updateNoActivityLabel();
}

void StatusPanelWidget::updateNoActivityLabel()
{
    bool hasEntries = false;
    for (auto it = this->activityEntries.cbegin(); it != this->activityEntries.cend(); ++it)
    {
        if (!it.value().suppressed)
        {
            hasEntries = true;
            break;
        }
    }

    if (!hasEntries && this->showNoActivity)
    {
        if (!this->noActivityLabel)
        {
            this->noActivityLabel = new QLabel("No Activity", this->widgetActivity);
            this->noActivityLabel->setAlignment(Qt::AlignCenter);
            this->noActivityLabel->setStyleSheet("color: rgba(150, 150, 150, 150); padding: 8px;");
            this->activityOuterLayout->insertWidget(0, this->noActivityLabel);
        }
        this->noActivityLabel->show();
    }
    else if (this->noActivityLabel)
    {
        this->noActivityLabel->hide();
    }
}

void StatusPanelWidget::channelActivity(const ChannelActivityEvent& event)
{
    QString key = QString("%1:%2").arg(event.getChannel()).arg(event.getVideolayer());

    if (!event.getActive())
    {
        // Item deactivated - remove entry.
        if (this->activityEntries.contains(key))
        {
            removeActivityEntry(key);
            reorderActivity();
        }
        return;
    }

    // Item activated.
    if (this->activityEntries.contains(key))
    {
        ActivityEntry& existing = this->activityEntries[key];
        if (!existing.isStatic && !existing.suppressed)
        {
            // Already tracked as a progress entry via PlaybackProgressEvent.
            // Only update label if the new one is not empty/whitespace.
            QString newLabel = event.getLabel().trimmed();
            if (!newLabel.isEmpty())
            {
                existing.label = newLabel;
                existing.labelInfo->setText(newLabel);
            }
            return;
        }

        // Replace existing entry (static->static, static->video, video->static, or suppressed->new).
        removeActivityEntry(key);
    }

    // Create a new static activity entry.
    ActivityEntry entry;
    entry.isStatic = true;
    entry.itemType = event.getItemType();
    entry.channel = event.getChannel();
    entry.videolayer = event.getVideolayer();
    entry.label = event.getLabel();
    entry.lastUpdate = QDateTime::currentMSecsSinceEpoch();

    entry.row = new QWidget(this->widgetActivity);
    QHBoxLayout* rowLayout = new QHBoxLayout(entry.row);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(4);

    // Type badge
    int sbFS = this->bigBoldMode ? 13 : 9;
    int sbH  = this->bigBoldMode ? 22 : 16;
    entry.labelTypeBadge = new QLabel(entry.row);
    entry.labelTypeBadge->setText(typeBadgeLabel(event.getItemType()));
    entry.labelTypeBadge->setStyleSheet(QString("background-color: %1; color: white; border-radius: 3px; font-size: %2px; font-weight: bold; padding: 1px 4px;")
        .arg(typeBadgeColor(event.getItemType())).arg(sbFS));
    entry.labelTypeBadge->setFixedHeight(sbH);
    rowLayout->addWidget(entry.labelTypeBadge, 0);

    // Info label (item name)
    int siFS = this->bigBoldMode ? 14 : 11;
    entry.labelInfo = new QLabel(entry.row);
    entry.labelInfo->setText(event.getLabel());
    entry.labelInfo->setStyleSheet(QString("font-size: %1px; color: white;").arg(siFS));
    entry.labelInfo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rowLayout->addWidget(entry.labelInfo, 1);

    // Layer label (badge showing channel-layer)
    QSize slSz = this->bigBoldMode ? QSize(48, 30) : QSize(36, 24);
    entry.labelLayer = new QLabel(entry.row);
    entry.labelLayer->setText(QString("%1-%2").arg(event.getChannel()).arg(event.getVideolayer()));
    entry.labelLayer->setStyleSheet(channelColorStyle(event.getChannel()));
    entry.labelLayer->setAlignment(Qt::AlignCenter);
    entry.labelLayer->setFixedSize(slSz);
    rowLayout->addWidget(entry.labelLayer, 0);

    this->activityLayout->addWidget(entry.row);
    this->activityEntries[key] = entry;

    reorderActivity();
}

void StatusPanelWidget::channelCleared(const QString& deviceName, int channel, int videolayer)
{
    Q_UNUSED(deviceName); // activity entries are keyed by channel/layer only

    // Remove every row on the cleared channel (or the exact layer when given) —
    // play rows, progress rows and auto-loop countdown rows alike.
    QStringList clearedKeys;
    for (auto it = this->activityEntries.constBegin(); it != this->activityEntries.constEnd(); ++it)
    {
        if (it.value().channel == channel && (videolayer == -1 || it.value().videolayer == videolayer))
            clearedKeys.append(it.key());
    }

    for (const QString& key : clearedKeys)
        removeActivityEntry(key);

    if (!clearedKeys.isEmpty())
        reorderActivity();
}

void StatusPanelWidget::autoLoopCountdown(const AutoLoopCountdownEvent& event)
{
    // Use a distinct key so this row doesn't collide with the regular play activity row
    // for the same channel/videolayer.
    QString key = QString("autoloop:%1:%2").arg(event.getChannel()).arg(event.getVideolayer());

    if (!event.getActive())
    {
        if (this->activityEntries.contains(key))
        {
            removeActivityEntry(key);
            reorderActivity();
        }
        return;
    }

    int remaining = event.getRemainingSeconds();
    int total = event.getTotalSeconds();
    if (total < 1) total = 1;

    if (this->activityEntries.contains(key))
    {
        ActivityEntry& e = this->activityEntries[key];
        e.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        if (e.progressBar != nullptr)
        {
            e.progressBar->setRange(0, total);
            e.progressBar->setValue(remaining);
            e.progressBar->setFormat(QString("%1s").arg(remaining));
        }
        if (e.labelInfo != nullptr)
            e.labelInfo->setText(event.getLabel());
        return;
    }

    ActivityEntry entry;
    entry.isStatic = false; // Progress-style entry so cleanup can remove stale rows if events stop.
    entry.itemType = event.getItemType();
    entry.channel = event.getChannel();
    entry.videolayer = event.getVideolayer();
    entry.label = event.getLabel();
    entry.lastUpdate = QDateTime::currentMSecsSinceEpoch();

    entry.row = new QWidget(this->widgetActivity);
    QHBoxLayout* rowLayout = new QHBoxLayout(entry.row);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(4);

    int sbFS = this->bigBoldMode ? 13 : 9;
    int sbH  = this->bigBoldMode ? 22 : 16;
    entry.labelTypeBadge = new QLabel(entry.row);
    entry.labelTypeBadge->setText(tr("LOOP"));
    // Distinct color for loop badges — dark cyan/teal.
    entry.labelTypeBadge->setStyleSheet(QString("background-color: #008b8b; color: white; border-radius: 3px; font-size: %1px; font-weight: bold; padding: 1px 4px;").arg(sbFS));
    entry.labelTypeBadge->setFixedHeight(sbH);
    rowLayout->addWidget(entry.labelTypeBadge, 0);

    int siFS = this->bigBoldMode ? 14 : 11;
    entry.labelInfo = new QLabel(entry.row);
    entry.labelInfo->setText(event.getLabel());
    entry.labelInfo->setStyleSheet(QString("font-size: %1px; color: white;").arg(siFS));
    entry.labelInfo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rowLayout->addWidget(entry.labelInfo, 1);

    entry.progressBar = new QProgressBar(entry.row);
    entry.progressBar->setRange(0, total);
    entry.progressBar->setValue(remaining);
    entry.progressBar->setFormat(QString("%1s").arg(remaining));
    entry.progressBar->setTextVisible(true);
    entry.progressBar->setFixedWidth(this->bigBoldMode ? 90 : 70);
    entry.progressBar->setFixedHeight(sbH);
    rowLayout->addWidget(entry.progressBar, 0);

    QSize slSz = this->bigBoldMode ? QSize(48, 30) : QSize(36, 24);
    entry.labelLayer = new QLabel(entry.row);
    entry.labelLayer->setText(QString("%1-%2").arg(event.getChannel()).arg(event.getVideolayer()));
    entry.labelLayer->setStyleSheet(channelColorStyle(event.getChannel()));
    entry.labelLayer->setAlignment(Qt::AlignCenter);
    entry.labelLayer->setFixedSize(slSz);
    rowLayout->addWidget(entry.labelLayer, 0);

    this->activityLayout->addWidget(entry.row);
    this->activityEntries[key] = entry;

    reorderActivity();
}

void StatusPanelWidget::playbackProgress(const PlaybackProgressEvent& event)
{
    QString key = QString("%1:%2").arg(event.getChannel()).arg(event.getVideolayer());
    bool isNew = false;
    QString preservedLabel;     // Preserve label from static entry when upgrading
    QString preservedItemType;  // Preserve item type from static entry when upgrading

    if (!this->activityEntries.contains(key))
    {
        isNew = true;
    }
    else if (this->activityEntries[key].isStatic)
    {
        // Upgrade static entry to progress entry when PlaybackProgressEvent arrives.
        // Preserve label from the static entry (which came from executeCommand)
        // since OSC data might not include a label.
        preservedLabel = this->activityEntries[key].label;
        preservedItemType = this->activityEntries[key].itemType;
        removeActivityEntry(key);
        isNew = true;
    }

    if (isNew)
    {
        ActivityEntry entry;
        // Use event's itemType, falling back to preserved type from static entry, then "MOVIE".
        QString eventItemType = event.getItemType();
        if (eventItemType.isEmpty() && !preservedItemType.isEmpty())
            eventItemType = preservedItemType;
        if (eventItemType.isEmpty())
            eventItemType = "MOVIE";
        entry.itemType = eventItemType;
        entry.channel = event.getChannel();
        entry.videolayer = event.getVideolayer();
        // Use preserved label from static entry if OSC label is empty/whitespace.
        // Fall back to channel-layer as last resort.
        QString oscLabel = event.getLabel().trimmed();
        if (!oscLabel.isEmpty())
            entry.label = oscLabel;
        else if (!preservedLabel.isEmpty())
            entry.label = preservedLabel;
        else
            entry.label = QString("%1 %2-%3").arg(typeBadgeLabel(entry.itemType)).arg(event.getChannel()).arg(event.getVideolayer());

        entry.row = new QWidget(this->widgetActivity);

        // Grid layout with overlapping cells: progress bar behind, labels on top.
        QGridLayout* grid = new QGridLayout(entry.row);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(0);

        // Background layer: progress bar (fills entire row).
        entry.progressBar = new QProgressBar();
        entry.progressBar->setTextVisible(false);
        entry.progressBar->setMaximum(999999999);
        entry.progressBar->setStyleSheet(
            "QProgressBar {"
            "  background-color: rgba(25, 25, 30, 200);"
            "  border: none;"
            "  border-radius: 4px;"
            "}"
            "QProgressBar::chunk {"
            "  background-color: rgba(40, 180, 80, 120);"
            "  border-radius: 4px;"
            "}");
        grid->addWidget(entry.progressBar, 0, 0);

        // Foreground layer: single row of labels overlaid on top of progress bar.
        QWidget* overlay = new QWidget();
        overlay->setAttribute(Qt::WA_TranslucentBackground);
        overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
        QHBoxLayout* overlayLayout = new QHBoxLayout(overlay);
        int oM = this->bigBoldMode ? 8 : 4;
        overlayLayout->setContentsMargins(6, oM, oM, oM);
        overlayLayout->setSpacing(4);

        // Type badge (far left)
        int pbFS = this->bigBoldMode ? 12 : 8;
        int pbH  = this->bigBoldMode ? 20 : 14;
        entry.labelTypeBadge = new QLabel();
        entry.labelTypeBadge->setText(typeBadgeLabel(entry.itemType));
        entry.labelTypeBadge->setStyleSheet(QString("background-color: %1; color: white; border-radius: 2px; font-size: %2px; font-weight: bold; padding: 1px 4px;")
            .arg(typeBadgeColor(entry.itemType)).arg(pbFS));
        entry.labelTypeBadge->setFixedHeight(pbH);
        overlayLayout->addWidget(entry.labelTypeBadge, 0);

        // Item name (stretches to fill)
        int piFS = this->bigBoldMode ? 14 : 10;
        entry.labelInfo = new QLabel();
        entry.labelInfo->setText(entry.label);
        entry.labelInfo->setStyleSheet(QString("font-size: %1px; color: rgba(200, 200, 200, 170); background: transparent;").arg(piFS));
        entry.labelInfo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        overlayLayout->addWidget(entry.labelInfo, 1);

        // Loop/pause icons
        int iiFS = this->bigBoldMode ? 14 : 10;
        entry.labelIcons = new QLabel();
        entry.labelIcons->setStyleSheet(QString("font-size: %1px; color: rgba(200, 200, 200, 180); background: transparent;").arg(iiFS));
        overlayLayout->addWidget(entry.labelIcons, 0);

        // Countdown time (bold, in focus)
        int tFS = this->bigBoldMode ? 18 : 13;
        entry.labelTime = new QLabel();
        entry.labelTime->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: rgba(255, 255, 255, 230); background: transparent;").arg(tFS));
        entry.labelTime->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        overlayLayout->addWidget(entry.labelTime, 0);

        // Layer badge (far right)
        QSize plSz = this->bigBoldMode ? QSize(42, 26) : QSize(30, 18);
        entry.labelLayer = new QLabel();
        entry.labelLayer->setText(QString("%1-%2").arg(event.getChannel()).arg(event.getVideolayer()));
        entry.labelLayer->setStyleSheet(channelColorStyle(event.getChannel()));
        entry.labelLayer->setAlignment(Qt::AlignCenter);
        entry.labelLayer->setFixedSize(plSz);
        overlayLayout->addWidget(entry.labelLayer, 0);

        // Add overlay on same cell as progress bar, then raise it to the front.
        grid->addWidget(overlay, 0, 0);
        overlay->raise();

        // Smooth animation for progress bar.
        entry.animation = new QPropertyAnimation(entry.progressBar, "value", entry.progressBar);
        entry.animation->setDuration(200);
        entry.animation->setEasingCurve(QEasingCurve::Linear);

        // Opacity effect for fade animations (grayout and disappear).
        entry.rowOpacity = new QGraphicsOpacityEffect(entry.row);
        entry.rowOpacity->setOpacity(1.0);
        entry.row->setGraphicsEffect(entry.rowOpacity);

        entry.lastUpdate = QDateTime::currentMSecsSinceEpoch();

        this->activityLayout->addWidget(entry.row);
        this->activityEntries[key] = entry;
    }

    ActivityEntry& entry = this->activityEntries[key];

    // Skip processing for static entries - they don't have progress bars.
    if (entry.isStatic)
        return;

    entry.lastUpdate = QDateTime::currentMSecsSinceEpoch();

    // If suppressed (auto-hidden after 20s done), silently absorb OSC updates.
    if (entry.suppressed)
    {
        double fps = event.getFps();
        if (fps > 0)
        {
            int rangeMin = static_cast<int>(event.getClip() * fps);
            int rangeMax = static_cast<int>(event.getTotalClip() * fps);
            int targetValue = static_cast<int>(event.getTime() * fps);
            bool isDone = (rangeMax > rangeMin) && (targetValue >= rangeMax - 1);
            if (!isDone)
            {
                // New clip on this layer — remove suppressed entry so the next call recreates it.
                removeActivityEntry(key);
            }
        }
        return;
    }

    entry.fps = event.getFps();

    // Update itemType and badge if the event carries a different type
    // (e.g. a still replacing a movie on the same layer).
    QString eventType = event.getItemType();
    if (!eventType.isEmpty() && eventType != entry.itemType)
    {
        entry.itemType = eventType;
        if (entry.labelTypeBadge != nullptr)
        {
            int ubFS = this->bigBoldMode ? 12 : 8;
            entry.labelTypeBadge->setText(typeBadgeLabel(entry.itemType));
            entry.labelTypeBadge->setStyleSheet(QString("background-color: %1; color: white; border-radius: 2px; font-size: %2px; font-weight: bold; padding: 1px 4px;")
                .arg(typeBadgeColor(entry.itemType)).arg(ubFS));
        }
    }

    // Use OSC label if available (and not just whitespace), otherwise keep existing label.
    // Don't overwrite a good label with empty OSC data.
    QString newLabel = event.getLabel().trimmed();
    if (!newLabel.isEmpty())
        entry.label = newLabel;
    // If OSC label is empty, keep the existing entry.label (don't clear it).
    bool wasLoop = entry.loop;
    entry.loop = event.getLoop();

    // Update info label (use entry.label which may have been preserved from static entry)
    entry.labelInfo->setText(entry.label);

    // Update icons
    QString icons;
    if (event.getLoop())
        icons += QString::fromUtf8("\xe2\x9f\xb3");  // loop arrow
    if (event.getPaused())
        icons += QString::fromUtf8(" \xe2\x8f\xb8");  // pause symbol
    entry.labelIcons->setText(icons);

    // Update progress bar range and value
    if (entry.fps > 0)
    {
        double inTime = event.getClip();
        double outTime = event.getTotalClip();
        int rangeMin = static_cast<int>(inTime * entry.fps);
        int rangeMax = static_cast<int>(outTime * entry.fps);
        int targetValue = static_cast<int>(event.getTime() * entry.fps);

        entry.progressBar->setRange(rangeMin, rangeMax);

        // Animate smoothly to new value (skip if unchanged, e.g. paused clips).
        if (targetValue != entry.animation->endValue().toInt())
        {
            entry.animation->stop();
            entry.animation->setStartValue(entry.progressBar->value());
            entry.animation->setEndValue(targetValue);
            entry.animation->start();
        }

        // Update time display (countdown - remaining time)
        double remaining = event.getTotalClip() - event.getTime();
        if (remaining < 0) remaining = 0;
        entry.labelTime->setText(Timecode::fromTime(remaining, entry.fps, false));

        // Check if done playing (progress reached end)
        bool isDone = (rangeMax > rangeMin) && (targetValue >= rangeMax - 1);
        if (isDone != entry.done)
        {
            entry.done = isDone;
            int stBFS = this->bigBoldMode ? 12 : 8;
            int stIFS = this->bigBoldMode ? 14 : 10;
            int stTFS = this->bigBoldMode ? 18 : 13;
            int stLFS = this->bigBoldMode ? 14 : 10;
            if (isDone)
            {
                // Stop any existing fade animation before starting a new one.
                if (entry.rowFadeAnim != nullptr)
                {
                    entry.rowFadeAnim->stop();
                    delete entry.rowFadeAnim;
                    entry.rowFadeAnim = nullptr;
                }

                // Gradually fade to gray over 20s, then fade away over 5s.
                // Opacity reduction on the dark background naturally desaturates colors.
                entry.rowFadeAnim = new QPropertyAnimation(entry.rowOpacity, "opacity", entry.row);
                entry.rowFadeAnim->setDuration(25000);
                entry.rowFadeAnim->setKeyValueAt(0.0, entry.rowOpacity->opacity());
                entry.rowFadeAnim->setKeyValueAt(0.8, 0.3);  // Fade to near-gray over 20s
                entry.rowFadeAnim->setKeyValueAt(1.0, 0.0);   // Fade away over 5s
                entry.rowFadeAnim->start();

                QObject::connect(entry.rowFadeAnim, &QPropertyAnimation::finished, this, [this, key]() {
                    if (this->activityEntries.contains(key) && this->activityEntries[key].done
                        && !this->activityEntries[key].suppressed)
                    {
                        this->activityEntries[key].suppressed = true;
                        this->activityEntries[key].row->hide();
                        this->activityEntries[key].rowFadeAnim = nullptr;
                        reorderActivity();
                    }
                });
            }
            else
            {
                entry.progressBar->setStyleSheet(
                    "QProgressBar { background-color: rgba(25, 25, 30, 200); border: none; border-radius: 4px; }"
                    "QProgressBar::chunk { background-color: rgba(33, 120, 200, 60); border-radius: 4px; }");
                entry.labelTypeBadge->setStyleSheet(QString("background-color: %1; color: white; border-radius: 2px; font-size: %2px; font-weight: bold; padding: 1px 4px;")
                    .arg(typeBadgeColor(entry.itemType)).arg(stBFS));
                entry.labelInfo->setStyleSheet(QString("font-size: %1px; color: rgba(200, 200, 200, 170); background: transparent;").arg(stIFS));
                entry.labelTime->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: rgba(255, 255, 255, 230); background: transparent;").arg(stTFS));
                entry.labelLayer->setStyleSheet(channelColorStyle(entry.channel));

                // Fade opacity back to 1.0 (loop restart).
                if (entry.rowOpacity != nullptr)
                {
                    // Stop any existing fade animation before starting a new one.
                    if (entry.rowFadeAnim != nullptr)
                    {
                        entry.rowFadeAnim->stop();
                        delete entry.rowFadeAnim;
                        entry.rowFadeAnim = nullptr;
                    }

                    entry.rowFadeAnim = new QPropertyAnimation(entry.rowOpacity, "opacity", entry.row);
                    entry.rowFadeAnim->setDuration(300);
                    entry.rowFadeAnim->setStartValue(entry.rowOpacity->opacity());
                    entry.rowFadeAnim->setEndValue(1.0);
                    entry.rowFadeAnim->start();

                    QObject::connect(entry.rowFadeAnim, &QPropertyAnimation::finished, this, [this, key]() {
                        if (this->activityEntries.contains(key))
                            this->activityEntries[key].rowFadeAnim = nullptr;
                    });
                }
            }
        }
    }

    // Reorder if loop state changed or new entry
    if (isNew || wasLoop != entry.loop)
        reorderActivity();
}

void StatusPanelWidget::reorderActivity()
{
    // Remove all widgets from layout (without deleting)
    while (this->activityLayout->count() > 0)
        this->activityLayout->takeAt(0);

    // Remove old channel headers
    for (auto it = this->channelHeaders.begin(); it != this->channelHeaders.end(); ++it)
        delete it.value();
    this->channelHeaders.clear();

    // Helper struct to store entry with its sort key
    struct SortableEntry
    {
        QString key;
        QWidget* row;
        int channel;
        int videolayer;
        bool done;
    };

    QList<SortableEntry> activeEntries;
    QList<SortableEntry> doneEntries;

    for (auto it = this->activityEntries.begin(); it != this->activityEntries.end(); ++it)
    {
        if (it.value().suppressed)
            continue; // Suppressed entries are hidden and excluded from layout.

        SortableEntry se;
        se.key = it.key();
        se.row = it.value().row;
        se.done = it.value().done;
        se.channel = it.value().channel;
        se.videolayer = it.value().videolayer;

        if (se.done)
            doneEntries.append(se);
        else
            activeEntries.append(se);
    }

    // Sort active entries: by channel ascending, then by videolayer descending (highest at top)
    std::sort(activeEntries.begin(), activeEntries.end(), [](const SortableEntry& a, const SortableEntry& b) {
        if (a.channel != b.channel)
            return a.channel < b.channel;  // Channel ascending
        return a.videolayer > b.videolayer;  // Videolayer descending (highest first)
    });

    // Sort done entries the same way
    std::sort(doneEntries.begin(), doneEntries.end(), [](const SortableEntry& a, const SortableEntry& b) {
        if (a.channel != b.channel)
            return a.channel < b.channel;
        return a.videolayer > b.videolayer;
    });

    // Collect all unique channels from both active and done entries
    QSet<int> allChannels;
    for (const SortableEntry& se : activeEntries)
        allChannels.insert(se.channel);
    for (const SortableEntry& se : doneEntries)
        allChannels.insert(se.channel);

    // Sort channels
    QList<int> sortedChannels = allChannels.values();
    std::sort(sortedChannels.begin(), sortedChannels.end());

    // Add entries grouped by channel, with active entries first, then done entries
    for (int channel : sortedChannels)
    {
        // Create channel header
        bool hasActive = false;
        for (const SortableEntry& se : activeEntries)
        {
            if (se.channel == channel)
            {
                hasActive = true;
                break;
            }
        }

        int chFS = this->bigBoldMode ? 13 : 10;
        QLabel* header = new QLabel(this->widgetActivity);
        header->setText(QString("Channel %1").arg(channel));
        // Use brighter color if there are active entries, dimmer if only done entries
        if (hasActive)
            header->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: rgba(150, 150, 150, 200); padding: 4px 0 2px 0;").arg(chFS));
        else
            header->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: rgba(100, 100, 100, 200); padding: 4px 0 2px 0;").arg(chFS));
        header->setVisible(this->showChannelHeaders);
        this->activityLayout->addWidget(header);
        this->channelHeaders[channel] = header;

        // Add active entries for this channel
        for (const SortableEntry& se : activeEntries)
        {
            if (se.channel == channel)
                this->activityLayout->addWidget(se.row);
        }

        // Add done entries for this channel (greyed out, after active)
        for (const SortableEntry& se : doneEntries)
        {
            if (se.channel == channel)
                this->activityLayout->addWidget(se.row);
        }
    }
}

void StatusPanelWidget::cleanupStaleEntries()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList toRemove;

    for (auto it = this->activityEntries.begin(); it != this->activityEntries.end(); ++it)
    {
        // Only auto-remove non-static entries (video entries with stale OSC data).
        // Static entries persist until explicitly removed via channelActivity.
        if (!it.value().isStatic && (now - it.value().lastUpdate) > 3000)
            toRemove.append(it.key());
    }

    for (const QString& key : toRemove)
        removeActivityEntry(key);
}

void StatusPanelWidget::updateBankIcons()
{
    // Update icons: only banks with actual assignments are lit.
    for (int i = 0; i < BANK_COUNT; i++)
    {
        int bankNum = i + 1;
        bool hasAssignment = (TriggerBankRegistry::getInstance().getItem(bankNum) != nullptr);

        if (hasAssignment)
        {
            // Lit - bank has an assignment
            this->bankIcons[i]->setStyleSheet(
                "background-color: rgba(255, 165, 0, 220); "
                "color: white; "
                "border-radius: 3px; "
                "font-size: 10px; "
                "font-weight: bold;");
        }
        else
        {
            // Greyed out - no assignment
            this->bankIcons[i]->setStyleSheet(
                "background-color: rgba(60, 60, 60, 200); "
                "color: rgba(100, 100, 100, 200); "
                "border-radius: 3px; "
                "font-size: 10px; "
                "font-weight: bold;");
        }
    }
}

void StatusPanelWidget::bankAssignmentChanged(const BankAssignmentChangedEvent& event)
{
    updateBankDisplay(event.getBankId());
    updateBankIcons();
}

void StatusPanelWidget::updateBankDisplay(int bankId)
{
    if (bankId < 1 || bankId > BANK_COUNT)
        return;

    QTreeWidgetItem* item = TriggerBankRegistry::getInstance().getItem(bankId);

    // If no item assigned, remove the bank entry if it exists
    if (item == nullptr)
    {
        removeBankEntry(bankId);
        return;
    }

    QTreeWidget* treeWidget = item->treeWidget();
    if (treeWidget == nullptr)
    {
        removeBankEntry(bankId);
        return;
    }

    QWidget* widget = treeWidget->itemWidget(item, 0);
    if (widget == nullptr)
    {
        removeBankEntry(bankId);
        return;
    }

    AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(widget);
    if (rundownWidget == nullptr)
    {
        removeBankEntry(bankId);
        return;
    }

    // Get item label
    QString label = rundownWidget->getLibraryModel()->getLabel();
    if (label.isEmpty())
        label = rundownWidget->getLibraryModel()->getName();
    if (label.isEmpty())
        label = rundownWidget->getLibraryModel()->getType();

    // Get channel info
    int channel = rundownWidget->getCommand()->getChannel();
    int videolayer = rundownWidget->getCommand()->getVideolayer();
    QString channelText = QString("%1-%2").arg(channel).arg(videolayer);

    // Create or update bank entry
    if (!this->bankEntries.contains(bankId))
    {
        BankEntry entry;
        entry.row = new QWidget(this->widgetBanks);
        QHBoxLayout* rowLayout = new QHBoxLayout(entry.row);
        rowLayout->setContentsMargins(0, 1, 0, 1);
        rowLayout->setSpacing(4);

        int bbFS  = this->bigBoldMode ? 13 : 9;
        int bbW   = this->bigBoldMode ? 32 : 24;
        int bbH   = this->bigBoldMode ? 22 : 16;
        int biFS  = this->bigBoldMode ? 14 : 10;
        QSize bcSz = this->bigBoldMode ? QSize(42, 24) : QSize(30, 18);

        entry.labelBank = new QLabel(entry.row);
        entry.labelBank->setText(QString("B%1").arg(bankId));
        entry.labelBank->setStyleSheet(QString("background-color: rgba(255, 165, 0, 200); color: white; border-radius: 3px; font-size: %1px; font-weight: bold; padding: 1px 4px;").arg(bbFS));
        entry.labelBank->setFixedWidth(bbW);
        entry.labelBank->setFixedHeight(bbH);
        entry.labelBank->setAlignment(Qt::AlignCenter);
        rowLayout->addWidget(entry.labelBank, 0);

        entry.labelItem = new QLabel(entry.row);
        entry.labelItem->setStyleSheet(QString("font-size: %1px; color: rgba(200, 200, 200, 200);").arg(biFS));
        entry.labelItem->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        rowLayout->addWidget(entry.labelItem, 1);

        entry.labelChannel = new QLabel(entry.row);
        entry.labelChannel->setAlignment(Qt::AlignCenter);
        entry.labelChannel->setFixedSize(bcSz);
        rowLayout->addWidget(entry.labelChannel, 0);

        // Add to layout in sorted order by bank ID
        int insertIndex = 0;
        for (int i = 0; i < this->banksEntriesLayout->count(); i++)
        {
            QLayoutItem* layoutItem = this->banksEntriesLayout->itemAt(i);
            if (layoutItem == nullptr || layoutItem->widget() == nullptr)
                continue;

            QWidget* w = layoutItem->widget();
            for (auto it = this->bankEntries.begin(); it != this->bankEntries.end(); ++it)
            {
                if (it.value().row == w && it.key() < bankId)
                    insertIndex = i + 1;
            }
        }
        this->banksEntriesLayout->insertWidget(insertIndex, entry.row);
        this->bankEntries[bankId] = entry;
    }

    BankEntry& entry = this->bankEntries[bankId];
    entry.labelItem->setText(label);
    entry.labelChannel->setText(channelText);
    entry.labelChannel->setStyleSheet(channelColorStyle(channel));
}

void StatusPanelWidget::removeBankEntry(int bankId)
{
    if (!this->bankEntries.contains(bankId))
        return;

    BankEntry& entry = this->bankEntries[bankId];
    this->banksEntriesLayout->removeWidget(entry.row);
    delete entry.row;
    this->bankEntries.remove(bankId);
}

void StatusPanelWidget::clearAllActivityEntries()
{
    QStringList keys = this->activityEntries.keys();
    for (const QString& key : keys)
        removeActivityEntry(key);
}

void StatusPanelWidget::rebuildAllBankEntries()
{
    // Remove all current bank entries and re-add from registry.
    QList<int> ids = this->bankEntries.keys();
    for (int id : ids)
        removeBankEntry(id);

    for (int bankId = 1; bankId <= BANK_COUNT; bankId++)
    {
        if (TriggerBankRegistry::getInstance().getItem(bankId) != nullptr)
            updateBankDisplay(bankId);
    }
    updateBankIcons();
}

void StatusPanelWidget::bigBoldModeChangedSlot(bool active)
{
    this->bigBoldMode = active;
    this->activityLayout->setSpacing(active ? 5 : 2);
    clearAllActivityEntries();
    rebuildAllBankEntries();
}

void StatusPanelWidget::deviceAdded(CasparDevice& device)
{
    QString deviceName;
    int channels = 0;

    QString serverPath;

    // Find the device model to get its name, channel count, and server path.
    QList<DeviceModel> models = DeviceManager::getInstance().getDeviceModels();
    for (const DeviceModel& model : models)
    {
        if (model.getAddress() == device.getAddress() && model.getPort() == device.getPort())
        {
            deviceName = model.getName();
            channels = model.getChannels();
            serverPath = model.getServerPath();
            break;
        }
    }

    if (deviceName.isEmpty())
        return;

    // Create server status row.
    ServerEntry entry;
    entry.deviceName = deviceName;
    entry.connected = device.isConnected();
    entry.mediaReceived = false;

    entry.row = new QWidget(this->widgetServer);
    QHBoxLayout* rowLayout = new QHBoxLayout(entry.row);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(6);

    // Device name label.
    entry.labelName = new QLabel(entry.row);
    entry.labelName->setText(deviceName);
    entry.labelName->setStyleSheet("font-size: 10px; color: rgba(200, 200, 200, 200);");
    entry.labelName->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rowLayout->addWidget(entry.labelName, 1);

    // Connection dot.
    entry.labelConnectionDot = new QLabel(entry.row);
    entry.labelConnectionDot->setFixedSize(12, 12);
    entry.labelConnectionDot->setToolTip("AMCP Connection");
    rowLayout->addWidget(entry.labelConnectionDot, 0);

    // Media dot.
    entry.labelMediaDot = new QLabel(entry.row);
    entry.labelMediaDot->setFixedSize(12, 12);
    entry.labelMediaDot->setToolTip("Media Data");
    rowLayout->addWidget(entry.labelMediaDot, 0);

    // Connect/Disconnect button.
    entry.buttonConnect = new QPushButton(entry.row);
    entry.buttonConnect->setFixedHeight(20);
    entry.buttonConnect->setStyleSheet(
        "QPushButton { font-size: 9px; padding: 1px 8px; border-radius: 3px; "
        "background-color: rgba(60, 60, 60, 200); color: rgba(200, 200, 200, 200); border: 1px solid rgba(80, 80, 80, 200); }"
        "QPushButton:hover { background-color: rgba(80, 80, 80, 200); }");
    rowLayout->addWidget(entry.buttonConnect, 0);

    // Update dot styles.
    QString connectedStyle = "background-color: rgb(76, 175, 80); border-radius: 6px;";
    QString disconnectedStyle = "background-color: rgb(198, 40, 40); border-radius: 6px;";

    entry.labelConnectionDot->setStyleSheet(entry.connected ? connectedStyle : disconnectedStyle);
    entry.labelMediaDot->setStyleSheet(disconnectedStyle);
    entry.buttonConnect->setText(entry.connected ? "Disconnect" : "Connect");

    // Connect button click.
    QObject::connect(entry.buttonConnect, &QPushButton::clicked, [this, deviceName]() {
        auto device = DeviceManager::getInstance().getDeviceByName(deviceName);
        if (device == nullptr)
            return;
        if (device->isConnected())
        {
            if (this->disconnectMode == "ask")
            {
                if (QMessageBox::question(this, "Disconnect?",
                        QString("Are you sure you want to disconnect from %1?").arg(deviceName),
                        QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                    return;
            }
            device->disconnectDevice();
        }
        else
        {
            device->connectDevice();
        }
    });

    // Start/Restart buttons (only when server path is configured).
    entry.serverPath = serverPath;
    if (!serverPath.isEmpty())
    {
        static const QString btnStyle =
            "QPushButton { font-size: 9px; padding: 1px 6px; border-radius: 3px; "
            "background-color: rgba(60, 60, 60, 200); color: rgba(200, 200, 200, 200); border: 1px solid rgba(80, 80, 80, 200); }"
            "QPushButton:hover { background-color: rgba(80, 80, 80, 200); }";

        entry.buttonStart = new QPushButton("Start", entry.row);
        entry.buttonStart->setFixedHeight(20);
        entry.buttonStart->setStyleSheet(btnStyle);
        rowLayout->addWidget(entry.buttonStart, 0);

        entry.buttonRestart = new QPushButton("Restart", entry.row);
        entry.buttonRestart->setFixedHeight(20);
        entry.buttonRestart->setStyleSheet(btnStyle);
        entry.buttonRestart->setEnabled(false);
        rowLayout->addWidget(entry.buttonRestart, 0);

        // Start/Stop handler.
        QObject::connect(entry.buttonStart, &QPushButton::clicked, [this, deviceName]() {
            ServerEntry& e = this->serverEntries[deviceName];
            if (e.serverProcess != nullptr && e.serverProcess->state() != QProcess::NotRunning)
            {
                // Stop.
                if (QMessageBox::question(this, "Stop Server",
                        QString("Stop CasparCG server '%1'?\nThis will interrupt all outputs.").arg(deviceName),
                        QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                    return;

                auto device = DeviceManager::getInstance().getDeviceByName(deviceName);
                if (device != nullptr && device->isConnected())
                    device->disconnectDevice();

                e.serverProcess->kill();
            }
            else
            {
                // Start.
                if (QMessageBox::question(this, "Start Server",
                        QString("Start CasparCG server '%1'?").arg(deviceName),
                        QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                    return;

                if (e.serverProcess == nullptr)
                {
                    e.serverProcess = new QProcess(this);
                    QObject::connect(e.serverProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                                     [this, deviceName](int, QProcess::ExitStatus) {
                        if (!this->serverEntries.contains(deviceName))
                            return;
                        ServerEntry& e2 = this->serverEntries[deviceName];
                        e2.buttonStart->setText("Start");
                        e2.buttonRestart->setEnabled(false);
                    });
                }

                QFileInfo fi(e.serverPath);
                e.serverProcess->setWorkingDirectory(fi.absolutePath());
                e.serverProcess->start(e.serverPath);
                e.buttonStart->setText("Stop");
                e.buttonRestart->setEnabled(true);

                // Auto-reconnect after a short delay.
                QTimer::singleShot(3000, [deviceName]() {
                    auto device = DeviceManager::getInstance().getDeviceByName(deviceName);
                    if (device != nullptr && !device->isConnected())
                        device->connectDevice();
                });
            }
        });

        // Restart handler.
        QObject::connect(entry.buttonRestart, &QPushButton::clicked, [this, deviceName]() {
            ServerEntry& e = this->serverEntries[deviceName];
            if (e.serverProcess == nullptr || e.serverProcess->state() == QProcess::NotRunning)
                return;

            if (QMessageBox::question(this, "Restart Server",
                    QString("Restart CasparCG server '%1'?\nOutputs will be interrupted briefly.").arg(deviceName),
                    QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                return;

            auto device = DeviceManager::getInstance().getDeviceByName(deviceName);
            if (device != nullptr && device->isConnected())
                device->disconnectDevice();

            e.serverProcess->kill();
            e.serverProcess->waitForFinished(5000);

            QFileInfo fi(e.serverPath);
            e.serverProcess->setWorkingDirectory(fi.absolutePath());
            e.serverProcess->start(e.serverPath);
            e.buttonStart->setText("Stop");
            e.buttonRestart->setEnabled(true);

            QTimer::singleShot(3000, [deviceName]() {
                auto device = DeviceManager::getInstance().getDeviceByName(deviceName);
                if (device != nullptr && !device->isConnected())
                    device->connectDevice();
            });
        });
    }

    // Apply visibility settings.
    entry.row->setVisible(this->showServers);
    entry.buttonConnect->setVisible(this->disconnectMode != "hidden");

    // Connect to device signals (UniqueConnection prevents duplicates on device re-add).
    QObject::connect(&device, SIGNAL(connectionStateChanged(CasparDevice&)),
                     this, SLOT(deviceConnectionStateChanged(CasparDevice&)), Qt::UniqueConnection);
    QObject::connect(&device, SIGNAL(mediaChanged(const QList<CasparMedia>&, CasparDevice&)),
                     this, SLOT(deviceMediaChanged(const QList<CasparMedia>&, CasparDevice&)), Qt::UniqueConnection);

    this->serverLayout->addWidget(entry.row);
    this->serverEntries[deviceName] = entry;

    // Update max channels and rebuild lock grid.
    if (channels > this->maxChannels)
        this->maxChannels = channels;

    rebuildChannelLockGrid();
}

void StatusPanelWidget::deviceRemoved()
{
    // Rebuild entire server panel from current device list.
    // Remove all existing server entries.
    for (auto it = this->serverEntries.begin(); it != this->serverEntries.end(); ++it)
    {
        this->serverLayout->removeWidget(it.value().row);
        delete it.value().row;
    }
    this->serverEntries.clear();
    this->lockButtons.clear();
    this->globalLockButtons.clear();

    if (this->channelLockGrid != nullptr)
    {
        this->serverLayout->removeWidget(this->channelLockGrid);
        delete this->channelLockGrid;
        this->channelLockGrid = nullptr;
    }

    this->maxChannels = 0;

    // Re-add all current devices.
    QList<DeviceModel> models = DeviceManager::getInstance().getDeviceModels();
    for (const DeviceModel& model : models)
    {
        auto device = DeviceManager::getInstance().getDeviceByName(model.getName());
        if (device != nullptr)
            deviceAdded(*device);
    }
}

void StatusPanelWidget::deviceConnectionStateChanged(CasparDevice& device)
{
    // Find the server entry by matching address/port.
    for (auto it = this->serverEntries.begin(); it != this->serverEntries.end(); ++it)
    {
        ServerEntry& entry = it.value();
        auto devicePtr = DeviceManager::getInstance().getDeviceByName(entry.deviceName);
        if (devicePtr == nullptr)
            continue;
        if (devicePtr->getAddress() == device.getAddress() && devicePtr->getPort() == device.getPort())
        {
            entry.connected = device.isConnected();
            QString connectedStyle = "background-color: rgb(76, 175, 80); border-radius: 6px;";
            QString disconnectedStyle = "background-color: rgb(198, 40, 40); border-radius: 6px;";
            entry.labelConnectionDot->setStyleSheet(entry.connected ? connectedStyle : disconnectedStyle);
            entry.buttonConnect->setText(entry.connected ? "Disconnect" : "Connect");

            if (!entry.connected)
            {
                entry.mediaReceived = false;
                entry.labelMediaDot->setStyleSheet(disconnectedStyle);
            }
            break;
        }
    }
}

void StatusPanelWidget::deviceMediaChanged(const QList<CasparMedia>& mediaList, CasparDevice& device)
{
    Q_UNUSED(mediaList);

    for (auto it = this->serverEntries.begin(); it != this->serverEntries.end(); ++it)
    {
        ServerEntry& entry = it.value();
        auto devicePtr = DeviceManager::getInstance().getDeviceByName(entry.deviceName);
        if (devicePtr == nullptr)
            continue;
        if (devicePtr->getAddress() == device.getAddress() && devicePtr->getPort() == device.getPort())
        {
            entry.mediaReceived = true;
            entry.labelMediaDot->setStyleSheet("background-color: rgb(76, 175, 80); border-radius: 6px;");
            break;
        }
    }
}

void StatusPanelWidget::rebuildChannelLockGrid()
{
    // Remove old grid.
    if (this->channelLockGrid != nullptr)
    {
        this->serverLayout->removeWidget(this->channelLockGrid);
        delete this->channelLockGrid;
        this->channelLockGrid = nullptr;
    }
    this->lockButtons.clear();
    this->timerButtons.clear();
    this->globalLockButtons.clear();

    if (this->maxChannels <= 0 || this->serverEntries.isEmpty())
        return;

    this->channelLockGrid = new QWidget(this->widgetServer);
    QGridLayout* grid = new QGridLayout(this->channelLockGrid);
    grid->setContentsMargins(0, 4, 0, 0);
    grid->setSpacing(2);

    // Header row: channel numbers.
    QLabel* cornerLabel = new QLabel("Lock", this->channelLockGrid);
    cornerLabel->setStyleSheet("font-size: 9px; color: rgba(150, 150, 150, 180);");
    grid->addWidget(cornerLabel, 0, 0);

    for (int ch = 1; ch <= this->maxChannels; ch++)
    {
        QLabel* chLabel = new QLabel(QString("Ch%1").arg(ch), this->channelLockGrid);
        chLabel->setStyleSheet("font-size: 9px; color: rgba(150, 150, 150, 180);");
        chLabel->setAlignment(Qt::AlignCenter);
        grid->addWidget(chLabel, 0, ch);
    }

    // Per-device rows.
    int row = 1;
    QStringList deviceNames = this->serverEntries.keys();
    std::sort(deviceNames.begin(), deviceNames.end());

    for (const QString& deviceName : deviceNames)
    {
        QLabel* nameLabel = new QLabel(deviceName, this->channelLockGrid);
        nameLabel->setStyleSheet("font-size: 9px; color: rgba(200, 200, 200, 180);");
        grid->addWidget(nameLabel, row, 0);

        for (int ch = 1; ch <= this->maxChannels; ch++)
        {
            // Container widget with lock button on top, stopwatch button below.
            QWidget* cellWidget = new QWidget(this->channelLockGrid);
            QVBoxLayout* cellLayout = new QVBoxLayout(cellWidget);
            cellLayout->setContentsMargins(0, 0, 0, 0);
            cellLayout->setSpacing(1);
            cellLayout->setAlignment(Qt::AlignCenter);

            // Lock button (top).
            QPushButton* lockBtn = new QPushButton(cellWidget);
            lockBtn->setFixedSize(28, 20);
            lockBtn->setCheckable(true);
            bool locked = DeviceManager::getInstance().isChannelLocked(deviceName, ch);
            bool timed = DeviceManager::getInstance().isTimedLock(deviceName, ch);
            int remaining = DeviceManager::getInstance().getRemainingLockSeconds(deviceName, ch);
            lockBtn->setChecked(locked);
            updateLockButtonStyle(lockBtn, locked, timed, remaining);

            QObject::connect(lockBtn, &QPushButton::clicked, [deviceName, ch]() {
                DeviceManager::getInstance().toggleChannelLock(deviceName, ch);
            });

            // Stopwatch button (⏱) below.
            QPushButton* timerBtn = new QPushButton(QString::fromUtf8("\xe2\x8f\xb1"), cellWidget);
            timerBtn->setFixedSize(28, 16);
            updateTimerButtonStyle(timerBtn, timed);

            QObject::connect(timerBtn, &QPushButton::clicked, [this, deviceName, ch]() {
                bool ok;
                int minutes = QInputDialog::getInt(this, "Timed Channel Lock",
                    QString("Lock Ch%1 for how many minutes?").arg(ch),
                    5, 1, 120, 1, &ok);
                if (ok)
                    DeviceManager::getInstance().setTimedChannelLock(deviceName, ch, minutes * 60);
            });

            cellLayout->addWidget(lockBtn, 0, Qt::AlignCenter);
            cellLayout->addWidget(timerBtn, 0, Qt::AlignCenter);

            grid->addWidget(cellWidget, row, ch, Qt::AlignCenter);
            this->lockButtons[deviceName][ch] = lockBtn;
            this->timerButtons[deviceName][ch] = timerBtn;
        }
        row++;
    }

    // "All" row (global locks) — no timed lock for global row.
    if (deviceNames.size() > 1)
    {
        QLabel* allLabel = new QLabel("All", this->channelLockGrid);
        allLabel->setStyleSheet("font-size: 9px; font-weight: bold; color: rgba(200, 200, 200, 180);");
        grid->addWidget(allLabel, row, 0);

        for (int ch = 1; ch <= this->maxChannels; ch++)
        {
            QPushButton* btn = new QPushButton(this->channelLockGrid);
            btn->setFixedSize(28, 20);
            btn->setCheckable(true);
            bool locked = DeviceManager::getInstance().getGlobalLockedChannels().contains(ch);
            btn->setChecked(locked);
            updateLockButtonStyle(btn, locked);

            QObject::connect(btn, &QPushButton::clicked, [ch]() {
                DeviceManager::getInstance().toggleGlobalChannelLock(ch);
            });

            grid->addWidget(btn, row, ch);
            this->globalLockButtons[ch] = btn;
        }
    }

    this->serverLayout->addWidget(this->channelLockGrid);
    this->channelLockGrid->setVisible(this->showChannelLocks);
}

void StatusPanelWidget::updateLockButtonStyle(QPushButton* button, bool locked, bool timed, int remainingSecs)
{
    if (locked && timed && remainingSecs > 0)
    {
        // Timed lock: amber with countdown.
        int mins = remainingSecs / 60;
        int secs = remainingSecs % 60;
        button->setText(QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0')));
        button->setStyleSheet(
            "QPushButton { background-color: rgba(200, 150, 30, 200); color: white; border-radius: 3px; "
            "font-size: 8px; font-weight: bold; border: 1px solid rgba(220, 170, 50, 200); }"
            "QPushButton:hover { background-color: rgba(220, 170, 50, 200); }");
    }
    else if (locked)
    {
        button->setStyleSheet(
            "QPushButton { background-color: rgba(198, 40, 40, 200); color: white; border-radius: 3px; "
            "font-size: 9px; font-weight: bold; border: 1px solid rgba(220, 60, 60, 200); }"
            "QPushButton:hover { background-color: rgba(220, 60, 60, 200); }");
        button->setText(QString::fromUtf8("\xf0\x9f\x94\x92")); // lock emoji
    }
    else
    {
        button->setStyleSheet(
            "QPushButton { background-color: rgba(50, 50, 50, 200); color: rgba(100, 100, 100, 200); border-radius: 3px; "
            "font-size: 9px; border: 1px solid rgba(70, 70, 70, 200); }"
            "QPushButton:hover { background-color: rgba(70, 70, 70, 200); }");
        button->setText(QString::fromUtf8("\xf0\x9f\x94\x93")); // unlock emoji
    }
}

void StatusPanelWidget::updateTimerButtonStyle(QPushButton* button, bool active)
{
    if (active)
    {
        button->setStyleSheet(
            "QPushButton { background-color: rgba(200, 150, 30, 180); color: white; border-radius: 3px; "
            "font-size: 10px; border: 1px solid rgba(220, 170, 50, 200); }"
            "QPushButton:hover { background-color: rgba(220, 170, 50, 200); }");
    }
    else
    {
        button->setStyleSheet(
            "QPushButton { background-color: rgba(50, 50, 50, 180); color: rgba(120, 120, 120, 200); border-radius: 3px; "
            "font-size: 10px; border: 1px solid rgba(70, 70, 70, 200); }"
            "QPushButton:hover { background-color: rgba(70, 70, 70, 200); }");
    }
}

void StatusPanelWidget::channelLockChanged(const QString& deviceName, int channel, bool locked)
{
    if (deviceName.isEmpty())
    {
        // Global lock changed - update the "All" row button.
        if (this->globalLockButtons.contains(channel))
        {
            QPushButton* btn = this->globalLockButtons[channel];
            btn->setChecked(locked);
            updateLockButtonStyle(btn, locked);
        }
    }
    else
    {
        // Per-device lock changed - update the specific button.
        if (this->lockButtons.contains(deviceName) && this->lockButtons[deviceName].contains(channel))
        {
            bool effectiveLocked = DeviceManager::getInstance().isChannelLocked(deviceName, channel);
            bool timed = DeviceManager::getInstance().isTimedLock(deviceName, channel);
            int remaining = DeviceManager::getInstance().getRemainingLockSeconds(deviceName, channel);
            QPushButton* btn = this->lockButtons[deviceName][channel];
            btn->setChecked(effectiveLocked);
            updateLockButtonStyle(btn, effectiveLocked, timed, remaining);

            // Update stopwatch button style.
            if (this->timerButtons.contains(deviceName) && this->timerButtons[deviceName].contains(channel))
                updateTimerButtonStyle(this->timerButtons[deviceName][channel], timed);
        }
    }
}

void StatusPanelWidget::timedLockTick(const QString& deviceName, int channel, int remainingSecs)
{
    if (this->lockButtons.contains(deviceName) && this->lockButtons[deviceName].contains(channel))
    {
        QPushButton* btn = this->lockButtons[deviceName][channel];
        updateLockButtonStyle(btn, true, true, remainingSecs);
    }
}

void StatusPanelWidget::previewModeChanged(bool active)
{
    this->previewModeButton->setChecked(active);
    updatePreviewButtonStyle();
}

void StatusPanelWidget::previewModifierHeld(bool held)
{
    this->modifierHeld = held;
    updatePreviewButtonStyle();
}

void StatusPanelWidget::autostepModeChanged(bool active)
{
    this->autostepModeButton->setChecked(active);
    updateAutostepButtonStyle();
}

static QColor lightenColor(const QColor& c, int amount)
{
    return QColor(qMin(c.red() + amount, 255), qMin(c.green() + amount, 255), qMin(c.blue() + amount, 255), c.alpha());
}

static QColor cachedColor(const QString& cacheVal, const QColor& fallback)
{
    QColor c(cacheVal);
    return (c.isValid() && !cacheVal.isEmpty()) ? c : fallback;
}

static QString buttonActiveStyle(const QColor& bg)
{
    QColor border = lightenColor(bg, 30);
    return QString(
        "QPushButton { background-color: rgba(%1,%2,%3,200); color: white; "
        "border-radius: 3px; font-size: 10px; font-weight: bold; border: 1px solid rgba(%4,%5,%6,200); }"
        "QPushButton:hover { background-color: rgba(%4,%5,%6,200); }")
        .arg(bg.red()).arg(bg.green()).arg(bg.blue())
        .arg(border.red()).arg(border.green()).arg(border.blue());
}

static const QString BUTTON_INACTIVE_STYLE =
    "QPushButton { background-color: rgba(50, 50, 50, 200); color: rgba(100, 100, 100, 200); "
    "border-radius: 3px; font-size: 10px; font-weight: bold; border: 1px solid rgba(70, 70, 70, 200); }"
    "QPushButton:hover { background-color: rgba(70, 70, 70, 200); }";

void StatusPanelWidget::updateAutostepButtonStyle()
{
    bool toggled = EventManager::getInstance().getAutostepMode();

    if (toggled)
    {
        QColor color = cachedColor(ColorCache::stepButton(), QColor(100, 60, 160));
        this->autostepModeButton->setStyleSheet(buttonActiveStyle(color));
    }
    else
    {
        this->autostepModeButton->setStyleSheet(BUTTON_INACTIVE_STYLE);
    }
}

void StatusPanelWidget::updatePreviewButtonStyle()
{
    bool toggled = EventManager::getInstance().getPreviewMode();

    if (toggled)
    {
        QColor color = cachedColor(ColorCache::pvwButton(), QColor(200, 150, 0));
        this->previewModeButton->setStyleSheet(buttonActiveStyle(color));
    }
    else if (this->modifierHeld)
    {
        // Modifier key held — brighter variant of PVW color.
        QColor color = cachedColor(ColorCache::pvwButton(), QColor(200, 150, 0));
        QColor bright = lightenColor(color, 55);
        this->previewModeButton->setStyleSheet(buttonActiveStyle(bright));
    }
    else
    {
        this->previewModeButton->setStyleSheet(BUTTON_INACTIVE_STYLE);
    }
}
