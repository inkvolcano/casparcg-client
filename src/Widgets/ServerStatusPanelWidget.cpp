#include "ServerStatusPanelWidget.h"

#include "PanelPlacement.h"

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

ServerStatusPanelWidget::ServerStatusPanelWidget(QWidget* parent)
    : QWidget(parent),
      serverCollapsed(false),
      channelLockGrid(nullptr),
      maxChannels(0)
{
    setupUi(this);

    // Grows to fit its content rather than holding a fixed share of the column.
    this->tabWidgetServer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    QString val;
    val = DatabaseManager::getInstance().getConfigurationByName("ShowPVWButton").getValue();
    this->showPVW = val.isEmpty() || val == "true";
    val = DatabaseManager::getInstance().getConfigurationByName("ShowServers").getValue();
    this->showServers = val.isEmpty() || val == "true";
    val = DatabaseManager::getInstance().getConfigurationByName("ShowChannelLocks").getValue();
    this->showChannelLocks = val.isEmpty() || val == "true";
    val = DatabaseManager::getInstance().getConfigurationByName("DisconnectMode").getValue();
    this->disconnectMode = val.isEmpty() ? "ask" : val;
    val = DatabaseManager::getInstance().getConfigurationByName("ShowSTEPButton").getValue();
    this->showSTEP = val.isEmpty() || val == "true";

    setupMenus();
    setupServerPanel();

    this->previewModeButton->setVisible(this->showPVW);
    this->autostepModeButton->setVisible(this->showSTEP);

    updatePlacement();

    // The layout decides whether any of this runs, so the answer is re-read
    // whenever the layout changes rather than only at startup.
    QObject::connect(&EventManager::getInstance(), &EventManager::rebuildLayout,
                     this, [this]() { updatePlacement(); });

    QObject::connect(&DeviceManager::getInstance(), SIGNAL(deviceAdded(CasparDevice&)),
                     this, SLOT(deviceAdded(CasparDevice&)));
    QObject::connect(&DeviceManager::getInstance(), SIGNAL(deviceRemoved()),
                     this, SLOT(deviceRemoved()));
    QObject::connect(&DeviceManager::getInstance(), SIGNAL(channelLockChanged(const QString&, int, bool)),
                     this, SLOT(channelLockChanged(const QString&, int, bool)));
    QObject::connect(&DeviceManager::getInstance(), SIGNAL(timedLockTick(const QString&, int, int)),
                     this, SLOT(timedLockTick(const QString&, int, int)));

    QObject::connect(&EventManager::getInstance(), SIGNAL(previewModeChanged(bool)),
                     this, SLOT(previewModeChanged(bool)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(previewModifierHeld(bool)),
                     this, SLOT(previewModifierHeld(bool)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(autostepModeChanged(bool)),
                     this, SLOT(autostepModeChanged(bool)));
}

// Whether this panel is anywhere in the layout. A panel that is not placed does
// no work at all — not merely no drawing, because a hidden widget still receives
// the signal and the handler still runs.
void ServerStatusPanelWidget::updatePlacement()
{
    const bool simpleMode = DatabaseManager::getInstance()
        .getConfigurationByName("SimpleMode").getValue() == "true";

    QStringList columns;
    foreach (const QString& key, PanelPlacement::columnKeys(simpleMode))
        columns.append(DatabaseManager::getInstance().getConfigurationByName(key).getValue());

    this->serverPlaced = PanelPlacement::isPlaced("ServerStatus", columns);

    if (this->serverPlaced && !this->cacheStatusTimer.isActive())
        this->cacheStatusTimer.start(2000);
    else if (!this->serverPlaced && this->cacheStatusTimer.isActive())
        this->cacheStatusTimer.stop();
}

void ServerStatusPanelWidget::setupMenus()
{
    // Server hamburger menu.
    this->serverMenu = new QMenu(this);
    this->serverMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->serverMenu, "ServerStatus", this);
    this->serverMenu->addSeparator();
    this->serverExpandCollapseAction = this->serverMenu->addAction("Collapse", this, &ServerStatusPanelWidget::toggleServerCollapse);

    this->serverMenuButton = new QToolButton(this->tabWidgetServer);
    this->serverMenuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->serverMenuButton->setFixedSize(22, 22);
    this->serverMenuButton->setMenu(this->serverMenu);
    this->serverMenuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetServer->setCornerWidget(this->serverMenuButton);
}


void ServerStatusPanelWidget::setupServerPanel()
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

void ServerStatusPanelWidget::setupCacheRow(QVBoxLayout* serverOuterLayout)
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
    QObject::connect(&this->cacheStatusTimer, &QTimer::timeout, this, &ServerStatusPanelWidget::updateCacheStatus);
    // Started by updatePlacement(), and only when Server Status is somewhere.

    updateCacheStatus();
}

void ServerStatusPanelWidget::updateCacheStatus()
{
    if (!this->serverPlaced)
        return;

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

void ServerStatusPanelWidget::setupRelayRow(QVBoxLayout* serverOuterLayout)
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
    QObject::connect(&this->cacheStatusTimer, &QTimer::timeout, this, &ServerStatusPanelWidget::updateRelayStatus);

    updateRelayStatus();
}

void ServerStatusPanelWidget::updateRelayStatus()
{
    if (!this->serverPlaced)
        return;

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

void ServerStatusPanelWidget::toggleServerCollapse()
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

void ServerStatusPanelWidget::deviceAdded(CasparDevice& device)
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

void ServerStatusPanelWidget::deviceRemoved()
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

void ServerStatusPanelWidget::deviceConnectionStateChanged(CasparDevice& device)
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

void ServerStatusPanelWidget::deviceMediaChanged(const QList<CasparMedia>& mediaList, CasparDevice& device)
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

void ServerStatusPanelWidget::rebuildChannelLockGrid()
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

void ServerStatusPanelWidget::updateLockButtonStyle(QPushButton* button, bool locked, bool timed, int remainingSecs)
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

void ServerStatusPanelWidget::updateTimerButtonStyle(QPushButton* button, bool active)
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

void ServerStatusPanelWidget::channelLockChanged(const QString& deviceName, int channel, bool locked)
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

void ServerStatusPanelWidget::timedLockTick(const QString& deviceName, int channel, int remainingSecs)
{
    if (this->lockButtons.contains(deviceName) && this->lockButtons[deviceName].contains(channel))
    {
        QPushButton* btn = this->lockButtons[deviceName][channel];
        updateLockButtonStyle(btn, true, true, remainingSecs);
    }
}

void ServerStatusPanelWidget::previewModeChanged(bool active)
{
    this->previewModeButton->setChecked(active);
    updatePreviewButtonStyle();
}

void ServerStatusPanelWidget::previewModifierHeld(bool held)
{
    this->modifierHeld = held;
    updatePreviewButtonStyle();
}

void ServerStatusPanelWidget::autostepModeChanged(bool active)
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

void ServerStatusPanelWidget::updateAutostepButtonStyle()
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

void ServerStatusPanelWidget::updatePreviewButtonStyle()
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
