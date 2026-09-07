#include "SettingsDialog.h"
#include "DeviceDialog.h"
#include "LayoutEditorWidget.h"
#include "OscOutputDialog.h"
#include "ImportDeviceDialog.h"
#include "Rundown/RundownWidgetHelper.h"

#include "DatabaseManager.h"
#include "GpiManager.h"
#include "RelayClient.h"
#include "SheetCacheServer.h"
#include "TemplateInstaller.h"
#include "SheetsProjectRegistry.h"
#include "EventManager.h"
#include "Events/OscOutputChangedEvent.h"
#include "Events/Library/RefreshLibraryEvent.h"
#include "Events/Library/AutoRefreshLibraryEvent.h"
#include "Events/Rundown/SaveRundownEvent.h"
#include "Models/ConfigurationModel.h"
#include "Models/DeviceModel.h"
#include "Models/GpiModel.h"

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>
#include "Models/OscOutputModel.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QUuid>
#include <QtCore/QSignalBlocker>
#include <QtCore/QTimeZone>
#include <QtCore/QTimer>

#include <QtGui/QIcon>

#include <QtWidgets/QColorDialog>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QSlider>

namespace
{
    // Room under a group box title, and air between rows.
    //
    // A layout put straight into a QGroupBox starts at the very top of it, which is
    // where the title already is, so the first row and the title crowd each other.
    // The top margin is what buys the title its own line. Four pixels between rows
    // reads as cramped once a group has more than two or three of them.
    void spaceOutGroup(QGridLayout* grid)
    {
        grid->setContentsMargins(10, 18, 10, 10);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(8);
    }

    void spaceOutGroup(QHBoxLayout* row)
    {
        row->setContentsMargins(10, 18, 10, 10);
        row->setSpacing(8);
    }

    void spaceOutGroup(QVBoxLayout* box)
    {
        box->setContentsMargins(10, 18, 10, 10);
        box->setSpacing(8);
    }
}

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi(this);

    setupGeneralTab();

    // Remove fixed size constraints to allow resizing on low-resolution screens.
    this->setMinimumSize(500, 400);
    this->setMaximumSize(16777215, 16777215);

    // Create a main vertical layout for the dialog.
    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(11, 11, 11, 11);
    mainLayout->setSpacing(6);

    // Create top bar with OK button on the right.
    QHBoxLayout* topBar = new QHBoxLayout();
    topBar->addStretch();
    this->pushButtonOk->setParent(this);
    topBar->addWidget(this->pushButtonOk);
    mainLayout->addLayout(topBar);

    // Make each tab's content scrollable for low-resolution screens.
    for (int i = 0; i < this->tabWidgetSettings->count(); i++)
    {
        QWidget* tab = this->tabWidgetSettings->widget(i);

        // Skip tabs that already have a layout (e.g. General tab built by setupGeneralTab).
        if (tab->layout())
            continue;

        // Create a scroll area for this tab's content.
        QScrollArea* scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);

        // Create a container widget to hold the original tab content.
        QWidget* container = new QWidget();

        // Move all children from the tab to the container.
        QList<QObject*> children = tab->children();
        int maxRight = 0;
        int maxBottom = 0;
        for (QObject* child : children)
        {
            QWidget* widget = qobject_cast<QWidget*>(child);
            if (widget != nullptr)
            {
                QRect geom = widget->geometry();
                maxRight = qMax(maxRight, geom.right() + 1);
                maxBottom = qMax(maxBottom, geom.bottom() + 1);
                widget->setParent(container);
            }
        }
        container->setMinimumSize(maxRight, maxBottom + 10);

        scrollArea->setWidget(container);

        // Set scroll area as the tab's new content.
        QVBoxLayout* tabLayout = new QVBoxLayout(tab);
        tabLayout->setContentsMargins(0, 0, 0, 0);
        tabLayout->addWidget(scrollArea);
    }

    // Add the tab widget to the main layout.
    this->tabWidgetSettings->setParent(this);
    mainLayout->addWidget(this->tabWidgetSettings, 1);

    // Move the information label to the bottom.
    this->labelSettingsInformation->setParent(this);
    mainLayout->addWidget(this->labelSettingsInformation);

    // Replace the dialog's layout.
    delete this->layout();
    this->setLayout(mainLayout);

    this->stylesheet = qApp->styleSheet();

    // Debounce timer for font-size spinner: coalesce rapid spinbox ticks into
    // a single stylesheet application + DB write.
    this->fontSizeDebounceTimer = new QTimer(this);
    this->fontSizeDebounceTimer->setSingleShot(true);
    this->fontSizeDebounceTimer->setInterval(150);
    QObject::connect(this->fontSizeDebounceTimer, &QTimer::timeout, this, [this]() {
        qApp->setStyleSheet(this->stylesheet + WidgetHeaderCSS::generate() +
                            QString(" QWidget { font-size: %1px; }").arg(this->pendingFontSize));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "FontSize", QString::number(this->pendingFontSize)));
    });

    // Debounce timer for channel color slider DB writes: visual feedback stays
    // instant via in-memory setters, only the DB write is deferred.
    this->sliderDbWriteTimer = new QTimer(this);
    this->sliderDbWriteTimer->setSingleShot(true);
    this->sliderDbWriteTimer->setInterval(200);
    QObject::connect(this->sliderDbWriteTimer, &QTimer::timeout, this, [this]() {
        for (auto it = this->pendingSliderDbWrites.constBegin();
             it != this->pendingSliderDbWrites.constEnd(); ++it)
        {
            DatabaseManager::getInstance().updateConfiguration(
                ConfigurationModel(0, it.key(), it.value()));
        }
        this->pendingSliderDbWrites.clear();
    });

    blockAllSignals(true);
    this->comboBoxLogLevel->clear();
    this->comboBoxLogLevel->addItem("Disable", "-1");
    this->comboBoxLogLevel->addItem("Info", "0");
    this->comboBoxLogLevel->addItem("Error", "1");
    this->comboBoxLogLevel->addItem("Warning", "2");
    this->comboBoxLogLevel->addItem("Debug", "3");
    blockAllSignals(false);

    bool enableOscInputMonitor = (DatabaseManager::getInstance().getConfigurationByName("EnableOscInputMonitor").getValue() == "true") ? true : false;
    this->checkBoxEnableOscInputMonitor->setChecked(enableOscInputMonitor);
    this->labelOscInputMonitorPort->setEnabled(enableOscInputMonitor);
    this->lineEditOscInputMonitorPort->setEnabled(enableOscInputMonitor);

    bool enableOscInputControl = (DatabaseManager::getInstance().getConfigurationByName("EnableOscInputControl").getValue() == "true") ? true : false;
    this->checkBoxEnableOscInputControl->setChecked(enableOscInputControl);
    this->labelOscInputControlPort->setEnabled(enableOscInputControl);
    this->lineEditOscInputControlPort->setEnabled(enableOscInputControl);

    bool enableOscInputWebSocket = (DatabaseManager::getInstance().getConfigurationByName("EnableOscInputWebSocket").getValue() == "true") ? true : false;
    this->checkBoxEnableOscInputWebSocket->setChecked(enableOscInputWebSocket);
    this->labelOscInputWebSocketPort->setEnabled(enableOscInputWebSocket);
    this->lineEditOscInputWebSocketPort->setEnabled(enableOscInputWebSocket);

    bool disableAudioInStream = (DatabaseManager::getInstance().getConfigurationByName("DisableAudioInStream").getValue() == "true") ? true : false;
    this->checkBoxDisableAudioInStream->setChecked(disableAudioInStream);
    this->spinBoxQuality->setValue(100 - DatabaseManager::getInstance().getConfigurationByName("StreamQuality").getValue().toInt());
    this->spinBoxNetworkCache->setValue(DatabaseManager::getInstance().getConfigurationByName("NetworkCache").getValue().toInt());
    this->comboBoxLogLevel->setCurrentIndex(this->comboBoxLogLevel->findData(DatabaseManager::getInstance().getConfigurationByName("LogLevel").getValue()));
    this->lineEditStreamPort->setPlaceholderText(QString("%1").arg(Stream::DEFAULT_PORT));
    QString streamPort = DatabaseManager::getInstance().getConfigurationByName("StreamPort").getValue();
    if (!streamPort.isEmpty())
        this->lineEditStreamPort->setText(streamPort);

    this->lineEditOscInputMonitorPort->setPlaceholderText(QString("%1").arg(Osc::DEFAULT_MONITOR_PORT));
    QString oscMonitorPort = DatabaseManager::getInstance().getConfigurationByName("OscMonitorPort").getValue();
    if (!oscMonitorPort.isEmpty())
        this->lineEditOscInputMonitorPort->setText(oscMonitorPort);

    this->lineEditOscInputControlPort->setPlaceholderText(QString("%1").arg(Osc::DEFAULT_CONTROL_PORT));
    QString oscControlPort = DatabaseManager::getInstance().getConfigurationByName("OscControlPort").getValue();
    if (!oscControlPort.isEmpty())
        this->lineEditOscInputControlPort->setText(oscControlPort);

    this->lineEditOscInputWebSocketPort->setPlaceholderText(QString("%1").arg(Osc::DEFAULT_WEBSOCKET_PORT));
    QString oscWebSocketPort = DatabaseManager::getInstance().getConfigurationByName("OscWebSocketPort").getValue();
    if (!oscWebSocketPort.isEmpty())
        this->lineEditOscInputWebSocketPort->setText(oscWebSocketPort);

    loadDevice();
    loadGpi();
    loadOscOutput();
    setupHotkeyTab();

    // Layout editor tab.
    QWidget* tabLayout = new QWidget();
    QVBoxLayout* tabLayoutVBox = new QVBoxLayout(tabLayout);
    this->layoutEditor = new LayoutEditorWidget(tabLayout);
    tabLayoutVBox->addWidget(this->layoutEditor);

    // Panel Sizing group box.
    QGroupBox* panelSizingGroup = new QGroupBox("Panel Sizing", tabLayout);
    QGridLayout* psGrid = new QGridLayout(panelSizingGroup);
    spaceOutGroup(psGrid);

    struct PanelDef { QString id; QString label; QString defaultMode; };
    QList<PanelDef> panels = {
        {"AudioLevels", "Audio Levels", "fixed"},
        {"Preview", "Preview", "resizable"},
        {"Library", "Library", "expanding"},
        {"Inspector", "Inspector", "expanding"},
        {"ServerStatus", "Server Status", "fixed"},
        {"Activity", "Activity", "expanding"},
        {"TriggerBanks", "Trigger Banks", "fixed"},
        {"Live", "Live", "resizable"},
        {"NDI", "NDI", "resizable"},
        {"Performance", "Performance", "fixed"},
        {"HttpLog", "Http Log", "fixed"},
        {"Sheets", "Google Sheets", "resizable"},
        {"SimpleInspector", "Simple Inspector", "resizable"},
        {"Clock", "Clock", "fixed"},
        {"StatusBar", "Status Bar", "fixed"},
    };

    // Column headers.
    psGrid->addWidget(new QLabel("Panel", panelSizingGroup), 0, 0);
    psGrid->addWidget(new QLabel("Size Mode", panelSizingGroup), 0, 1);

    int psRow = 1;
    for (const auto& p : panels)
    {
        psGrid->addWidget(new QLabel(p.label, panelSizingGroup), psRow, 0);

        QComboBox* combo = new QComboBox(panelSizingGroup);
        combo->addItem("Fixed", "fixed");
        combo->addItem("Resizable", "resizable");
        combo->addItem("Expanding", "expanding");

        QString currentMode = DatabaseManager::getInstance()
            .getConfigurationByName("PanelSizeMode_" + p.id).getValue();
        if (currentMode.isEmpty()) currentMode = p.defaultMode;

        for (int i = 0; i < combo->count(); i++)
        {
            if (combo->itemData(i).toString() == currentMode)
            {
                combo->setCurrentIndex(i);
                break;
            }
        }
        psGrid->addWidget(combo, psRow, 1);

        this->panelSizingEntries.append({p.id, p.label, combo});
        psRow++;
    }

    tabLayoutVBox->addWidget(panelSizingGroup);

    // Show empty panels checkbox.
    this->checkBoxShowEmptyPanels = new QCheckBox("Show empty panel columns", tabLayout);
    QString showEmptyVal = DatabaseManager::getInstance()
        .getConfigurationByName("ShowEmptyPanels").getValue();
    this->checkBoxShowEmptyPanels->setChecked(showEmptyVal == "true");
    tabLayoutVBox->addWidget(this->checkBoxShowEmptyPanels);

    this->tabWidgetSettings->addTab(tabLayout, "Layout");

    // Simple Mode tab: its own independent layout + grid options.
    QWidget* tabSimpleMode = new QWidget();
    QVBoxLayout* smVBox = new QVBoxLayout(tabSimpleMode);

    smVBox->addWidget(new QLabel("Panel layout used while Simple Mode is active (View \xe2\x86\x92 Simple Mode):", tabSimpleMode));
    this->simpleLayoutEditor = new LayoutEditorWidget(tabSimpleMode, "Simple");
    smVBox->addWidget(this->simpleLayoutEditor, 1);

    QGroupBox* smButtonsGroup = new QGroupBox("Button Grid", tabSimpleMode);
    QHBoxLayout* smOptionsRow = new QHBoxLayout(smButtonsGroup);
    spaceOutGroup(smOptionsRow);
    // Grid columns/rows are set directly in the Simple Mode bottom bar.
    this->checkBoxSimplePlayStop = new QCheckBox("Show play/stop controls on buttons", smButtonsGroup);
    QString psVal = DatabaseManager::getInstance().getConfigurationByName("SimpleModeShowPlayStop").getValue();
    this->checkBoxSimplePlayStop->setChecked(psVal.isEmpty() || psVal == "true");
    smOptionsRow->addWidget(this->checkBoxSimplePlayStop);

    // Preview control: plays the item (or group) on the device's preview channel.
    this->checkBoxSimplePreview = new QCheckBox("Show preview (PVW) control on buttons", smButtonsGroup);
    this->checkBoxSimplePreview->setToolTip(
        "Adds a PVW control to each button that plays the item on the device's preview channel");
    QString pvVal = DatabaseManager::getInstance().getConfigurationByName("SimpleModeShowPreview").getValue();
    this->checkBoxSimplePreview->setChecked(pvVal.isEmpty() || pvVal == "true");
    smOptionsRow->addWidget(this->checkBoxSimplePreview);

    smOptionsRow->addStretch();
    smVBox->addWidget(smButtonsGroup);

    this->tabWidgetSettings->addTab(tabSimpleMode, "Simple Mode");

    // Sheets tab: the cache service, the budget it is measured against, and the
    // outward report.
    QWidget* tabSheets = new QWidget();
    QVBoxLayout* sheetsVBox = new QVBoxLayout(tabSheets);

    QGroupBox* sheetsCacheGroup = new QGroupBox("Cache Service", tabSheets);
    QGridLayout* sheetsGrid = new QGridLayout(sheetsCacheGroup);
    spaceOutGroup(sheetsGrid);

    sheetsGrid->addWidget(new QLabel("Service URL:", sheetsCacheGroup), 0, 0);
    this->lineEditSheetsCacheUrl = new QLineEdit(sheetsCacheGroup);
    this->lineEditSheetsCacheUrl->setPlaceholderText("http://localhost:3000/local_server.php");
    this->lineEditSheetsCacheUrl->setToolTip(
        "The cache the client reads before it spends a call on the sheet API.\n"
        "A 404 from here simply means a live read follows, which is what bypass mode does.");
    this->lineEditSheetsCacheUrl->setText(
        DatabaseManager::getInstance().getConfigurationByName("SheetsCacheUrl").getValue());
    sheetsGrid->addWidget(this->lineEditSheetsCacheUrl, 0, 1);

    sheetsGrid->addWidget(new QLabel("Reads per minute:", sheetsCacheGroup), 1, 0);
    this->spinBoxSheetsQuota = new QSpinBox(sheetsCacheGroup);
    this->spinBoxSheetsQuota->setRange(1, 6000);
    this->spinBoxSheetsQuota->setToolTip(
        "The budget the strain meter is drawn against. Google allows sixty a minute per key.");
    QString quotaVal = DatabaseManager::getInstance().getConfigurationByName("SheetsQuotaPerMinute").getValue();
    this->spinBoxSheetsQuota->setValue(quotaVal.toInt() > 0 ? quotaVal.toInt() : 60);
    sheetsGrid->addWidget(this->spinBoxSheetsQuota, 1, 1, Qt::AlignLeft);

    sheetsVBox->addWidget(sheetsCacheGroup);

    // Hosting the cache here instead of alongside it. The templates already know how
    // to talk to a cache; this answers on the same parameters, so pointing them at this
    // port is the whole migration.
    QGroupBox* sheetsHostGroup = new QGroupBox("Host The Cache In This Client", tabSheets);
    QGridLayout* hostGrid = new QGridLayout(sheetsHostGroup);
    spaceOutGroup(hostGrid);

    this->checkBoxHostSheetCache = new QCheckBox("Serve the sheet cache from this client", sheetsHostGroup);
    this->checkBoxHostSheetCache->setToolTip(
        "Answers the same spreadsheetId and sheetNumber parameters the PHP service answers,\n"
        "so a template pointed at this port needs no change. Takes effect on restart.");
    this->checkBoxHostSheetCache->setChecked(
        DatabaseManager::getInstance().getConfigurationByName("SheetsHostCache").getValue() == "true");
    hostGrid->addWidget(this->checkBoxHostSheetCache, 0, 0, 1, 3);

    hostGrid->addWidget(new QLabel("Port:", sheetsHostGroup), 1, 0);
    this->spinBoxSheetCachePort = new QSpinBox(sheetsHostGroup);
    this->spinBoxSheetCachePort->setRange(1, 65535);
    QString cachePortVal = DatabaseManager::getInstance().getConfigurationByName("SheetsHostCachePort").getValue();
    this->spinBoxSheetCachePort->setValue(cachePortVal.toInt() > 0 ? cachePortVal.toInt() : 3000);
    hostGrid->addWidget(this->spinBoxSheetCachePort, 1, 1, Qt::AlignLeft);

    hostGrid->addWidget(new QLabel("Cache folder:", sheetsHostGroup), 2, 0);
    this->lineEditSheetCacheDir = new QLineEdit(sheetsHostGroup);
    this->lineEditSheetCacheDir->setPlaceholderText(QCoreApplication::applicationDirPath() + "/sheets_data");
    this->lineEditSheetCacheDir->setToolTip(
        "File names match the PHP service, so an existing sheets_data folder can be used as it stands.");
    this->lineEditSheetCacheDir->setText(
        DatabaseManager::getInstance().getConfigurationByName("SheetsCacheDirectory").getValue());
    QObject::connect(this->lineEditSheetCacheDir, &QLineEdit::editingFinished, this, [this]() {
        refreshSheetCacheSize();
    });
    hostGrid->addWidget(this->lineEditSheetCacheDir, 2, 1, 1, 2);

    this->checkBoxSheetCacheBypass = new QCheckBox("Bypass \xe2\x80\x94 answer nothing, keep writing", sheetsHostGroup);
    this->checkBoxSheetCacheBypass->setToolTip(
        "Reads answer 404 so graphics go live to the sheet, while writes still land.\n"
        "The cache stays warm for the moment it is switched back on.\n"
        "Also reachable at /bypass?on=1 and /bypass?on=0 while the client is hosting.");
    this->checkBoxSheetCacheBypass->setChecked(
        DatabaseManager::getInstance().getConfigurationByName("SheetsCacheBypass").getValue() == "true");
    hostGrid->addWidget(this->checkBoxSheetCacheBypass, 3, 0, 1, 3);

    // Sitting under the folder it empties, and saying how much is in there, so the
    // button is a fact about the cache rather than a lever with an unknown effect.
    this->buttonClearSheetCache = new QPushButton(sheetsHostGroup);
    this->buttonClearSheetCache->setFixedHeight(22);
    this->buttonClearSheetCache->setFocusPolicy(Qt::NoFocus);
    this->buttonClearSheetCache->setToolTip(
        "Deletes the cached sheet files. Nothing is lost that a read will not fetch again \xe2\x80\x94\n"
        "the next request for each tab goes to the sheet and warms it back up.");
    QObject::connect(this->buttonClearSheetCache, &QPushButton::clicked, this, [this]() {
        qint64 bytes = 0;
        int count = SheetCacheServer::cachedSheetCount(&bytes);
        if (count == 0)
        {
            EventManager::getInstance().fireStatusbarEvent(StatusbarEvent("The sheet cache is already empty"));
            return;
        }

        QMessageBox confirm(this);
        confirm.setWindowTitle("Clear Sheet Cache");
        confirm.setIcon(QMessageBox::Question);
        confirm.setText(QString("Delete %1 cached sheet file(s)?").arg(count));
        confirm.setInformativeText(QString("%1\n\nEach tab is read from the sheet again the next "
                                           "time something asks for it, so this costs reads rather "
                                           "than data.").arg(SheetCacheServer::cacheDirectory()));
        confirm.setStandardButtons(QMessageBox::Cancel | QMessageBox::Yes);
        confirm.setDefaultButton(QMessageBox::Cancel);
        if (confirm.exec() != QMessageBox::Yes)
            return;

        QString error;
        int removed = SheetCacheServer::clearCache(&error);
        if (removed < 0)
        {
            EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(error, 8000, true));
            refreshSheetCacheSize();
            return;
        }

        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent(QString("Cleared %1 cached sheet file(s)").arg(removed)));
        refreshSheetCacheSize();
    });
    hostGrid->addWidget(this->buttonClearSheetCache, 4, 0, 1, 3, Qt::AlignLeft);

    refreshSheetCacheSize();


    sheetsVBox->addWidget(sheetsHostGroup);

    // Warming happens because something asked for a tab, never on a schedule. These
    // two numbers are the only shaping it gets: how long a copy stays good enough,
    // and how far apart refreshes are spaced when several are wanted at once.
    // Where each project's templates look for the cache. The flag lives in the
    // project's own project.js, which is the file the templates read, so this writes
    // there rather than keeping a second copy of the answer.
    QGroupBox* sheetsProjectsGroup = new QGroupBox("Template Projects", tabSheets);
    QVBoxLayout* projectsVBox = new QVBoxLayout(sheetsProjectsGroup);
    spaceOutGroup(projectsVBox);

    projectsVBox->addWidget(new QLabel(
        "Ticked, a project's templates read the cache at localhost:3000 (local = true).\n"
        "Unticked they use a relative path, which fails inside CasparCG and sends them\n"
        "straight to Google (local = false). Templates pick this up when they next load.",
        sheetsProjectsGroup));

    this->sheetProjectsBox = new QWidget(sheetsProjectsGroup);
    new QVBoxLayout(this->sheetProjectsBox);
    projectsVBox->addWidget(this->sheetProjectsBox);

    QPushButton* rescanProjects = new QPushButton("Rescan", sheetsProjectsGroup);
    rescanProjects->setFixedHeight(20);
    rescanProjects->setFocusPolicy(Qt::NoFocus);
    QObject::connect(rescanProjects, &QPushButton::clicked, this, [this]() {
        SheetsProjectRegistry::getInstance().discover();
        buildSheetProjectsGroup();
    });
    projectsVBox->addWidget(rescanProjects, 0, Qt::AlignLeft);

    sheetsVBox->addWidget(sheetsProjectsGroup);

    buildSheetProjectsGroup();

    QGroupBox* sheetsWarmGroup = new QGroupBox("Warming", tabSheets);
    QGridLayout* warmGrid = new QGridLayout(sheetsWarmGroup);
    spaceOutGroup(warmGrid);

    warmGrid->addWidget(new QLabel("Treat a tab as stale after:", sheetsWarmGroup), 0, 0);
    this->spinBoxWarmStale = new QSpinBox(sheetsWarmGroup);
    this->spinBoxWarmStale->setRange(1, 3600);
    this->spinBoxWarmStale->setSuffix(" s");
    this->spinBoxWarmStale->setToolTip(
        "A tab asked for again within this window is left alone; past it, the next demand refreshes it.");
    QString staleVal = DatabaseManager::getInstance().getConfigurationByName("SheetsWarmStaleSeconds").getValue();
    this->spinBoxWarmStale->setValue(staleVal.toInt() > 0 ? staleVal.toInt() : 30);
    warmGrid->addWidget(this->spinBoxWarmStale, 0, 1, Qt::AlignLeft);

    warmGrid->addWidget(new QLabel("Space refreshes by:", sheetsWarmGroup), 1, 0);
    this->spinBoxWarmSpacing = new QSpinBox(sheetsWarmGroup);
    this->spinBoxWarmSpacing->setRange(0, 60000);
    this->spinBoxWarmSpacing->setSingleStep(100);
    this->spinBoxWarmSpacing->setSuffix(" ms");
    this->spinBoxWarmSpacing->setToolTip(
        "A rundown full of sheet templates should trickle rather than arrive as a burst.");
    QString spacingVal = DatabaseManager::getInstance().getConfigurationByName("SheetsWarmSpacingMs").getValue();
    this->spinBoxWarmSpacing->setValue(spacingVal.toInt() > 0 ? spacingVal.toInt() : 1500);
    warmGrid->addWidget(this->spinBoxWarmSpacing, 1, 1, Qt::AlignLeft);

    warmGrid->addWidget(new QLabel(
        "Refreshes read through the keyless proxy named in the project's project.js when it\n"
        "has one, so they cost nothing against the per-minute budget. Without a proxy they\n"
        "come out of the budget, and are skipped once it is half spent.", sheetsWarmGroup), 2, 0, 1, 2);

    sheetsVBox->addWidget(sheetsWarmGroup);

    QGroupBox* sheetsStrainGroup = new QGroupBox("Strain Reporting", tabSheets);
    QVBoxLayout* strainVBox = new QVBoxLayout(sheetsStrainGroup);
    spaceOutGroup(strainVBox);

    strainVBox->addWidget(new QLabel(
        "This client can only count its own reads exactly; reads made inside graphics are\n"
        "estimated from the plays it fires. Publishing those figures lets something with a\n"
        "wider view \xe2\x80\x94 the Salvo connector \xe2\x80\x94 add them to every other source and arrive at a\n"
        "real total.", sheetsStrainGroup));

    QHBoxLayout* strainRow = new QHBoxLayout();
    strainRow->addWidget(new QLabel("Report to URL:", sheetsStrainGroup));
    this->lineEditSheetsStrainUrl = new QLineEdit(sheetsStrainGroup);
    this->lineEditSheetsStrainUrl->setPlaceholderText("leave empty to publish nothing");
    this->lineEditSheetsStrainUrl->setToolTip(
        "A JSON body is POSTed here every ten seconds while sheets are in use.\n"
        "Nothing is sent when this is empty, or when no sheet has been read in the last minute.");
    this->lineEditSheetsStrainUrl->setText(
        DatabaseManager::getInstance().getConfigurationByName("SheetsStrainReportUrl").getValue());
    strainRow->addWidget(this->lineEditSheetsStrainUrl, 1);
    strainVBox->addLayout(strainRow);

    sheetsVBox->addWidget(sheetsStrainGroup);
    sheetsVBox->addStretch();

    this->tabWidgetSettings->addTab(tabSheets, "Sheets");

    // Templates tab: receiving a pack pushed from a dev machine. Separate from
    // Sheets because it is a different job, though it shares the same socket.
    QWidget* tabTemplates = new QWidget();
    QVBoxLayout* templatesVBox = new QVBoxLayout(tabTemplates);

    QGroupBox* pushGroup = new QGroupBox("Accept Template Pushes", tabTemplates);
    QGridLayout* pushGrid = new QGridLayout(pushGroup);
    spaceOutGroup(pushGrid);

    this->checkBoxTemplatePush = new QCheckBox("Let a dev machine install template packs on this client", pushGroup);
    this->checkBoxTemplatePush->setToolTip(
        "A template is HTML that CasparCG runs, so this stays off until it is wanted.\n"
        "It uses the same port as the sheet cache, and turning it on starts that server\n"
        "even when the cache itself is not being hosted.");
    this->checkBoxTemplatePush->setChecked(TemplateInstaller::isEnabled());
    pushGrid->addWidget(this->checkBoxTemplatePush, 0, 0, 1, 3);

    pushGrid->addWidget(new QLabel("Token:", pushGroup), 1, 0);
    this->lineEditTemplatePushToken = new QLineEdit(TemplateInstaller::token(), pushGroup);
    this->lineEditTemplatePushToken->setPlaceholderText("press Generate");
    this->lineEditTemplatePushToken->setToolTip(
        "The pusher has to send this. An empty token never matches, so switching the\n"
        "feature on without setting one leaves the endpoint shut rather than open.");
    pushGrid->addWidget(this->lineEditTemplatePushToken, 1, 1);

    QPushButton* generateToken = new QPushButton("Generate", pushGroup);
    generateToken->setFixedHeight(22);
    generateToken->setFocusPolicy(Qt::NoFocus);
    QObject::connect(generateToken, &QPushButton::clicked, this, [this]() {
        this->lineEditTemplatePushToken->setText(QUuid::createUuid().toString(QUuid::WithoutBraces));
        this->lineEditTemplatePushToken->selectAll();
    });
    pushGrid->addWidget(generateToken, 1, 2);

    pushGrid->addWidget(new QLabel("Install into:", pushGroup), 2, 0);
    this->lineEditTemplatePushPath = new QLineEdit(
        DatabaseManager::getInstance().getConfigurationByName("TemplatePushPath").getValue(), pushGroup);
    this->lineEditTemplatePushPath->setPlaceholderText(TemplateInstaller::templatesRoot());
    this->lineEditTemplatePushPath->setToolTip(
        "Left empty, packs go to the template path of the first device that has one,\n"
        "which on an ordinary setup is the right answer and needs nothing filled in.");
    pushGrid->addWidget(this->lineEditTemplatePushPath, 2, 1, 1, 2);

    pushGrid->addWidget(new QLabel(
        "project.js and extensions.json are never written by a push: this machine owns the\n"
        "API key, the local flag and the Sheets panel buttons.", pushGroup), 3, 0, 1, 3);

    templatesVBox->addWidget(pushGroup);

    // Pulling: the same job as a push, in the direction that survives a venue
    // firewall. Nothing inbound is opened here; this machine reaches out. Either a
    // relay or a private GitHub repository, told apart by how the address is written.
    QGroupBox* relayGroup = new QGroupBox("Pull Packs From A Relay Or GitHub", tabTemplates);
    QGridLayout* relayGrid = new QGridLayout(relayGroup);
    spaceOutGroup(relayGrid);

    this->checkBoxRelayEnabled = new QCheckBox("Check for new template packs", relayGroup);
    this->checkBoxRelayEnabled->setToolTip(
        "This client asks what is there and fetches only the files that differ.\n"
        "It works from behind any firewall, because nothing has to reach in to this machine.\n"
        "Files removed at the far end are never removed from here.");
    this->checkBoxRelayEnabled->setChecked(RelayClient::isEnabled());
    relayGrid->addWidget(this->checkBoxRelayEnabled, 0, 0, 1, 4);

    relayGrid->addWidget(new QLabel("Source:", relayGroup), 1, 0);
    this->lineEditRelayUrl = new QLineEdit(RelayClient::url(), relayGroup);
    this->lineEditRelayUrl->setPlaceholderText("https://example.com/relay/relay.php   or   github:owner/repo");
    this->lineEditRelayUrl->setToolTip(
        "A relay: the full address of its relay.php.\n\n"
        "A private GitHub repository: github:owner/repo, or github:owner/repo@branch\n"
        "for a branch other than the default. Each folder at the root of the repository\n"
        "is one pack. GitHub costs nothing to run and keeps the history of every\n"
        "template as a side effect.");
    relayGrid->addWidget(this->lineEditRelayUrl, 1, 1, 1, 3);

    relayGrid->addWidget(new QLabel("Token:", relayGroup), 2, 0);
    this->lineEditRelayToken = new QLineEdit(RelayClient::token(), relayGroup);
    this->lineEditRelayToken->setPlaceholderText("read-only token for whichever source is above");
    this->lineEditRelayToken->setToolTip(
        "For a relay: its DOWNLOAD_TOKEN, the one that can only read. The upload token\n"
        "belongs on the dev machine and should never be put here.\n\n"
        "For GitHub: a fine-grained personal access token with read-only access to that\n"
        "repository's contents. Give it nothing else, and nothing on any other repository.");
    relayGrid->addWidget(this->lineEditRelayToken, 2, 1, 1, 3);

    // Where a GitHub token comes from. Nobody guesses this menu path, and getting it
    // wrong quietly gives a token far more reach than the job needs.
    QLabel* tokenHelp = new QLabel(
        "GitHub: avatar \xe2\x86\x92 Settings \xe2\x86\x92 Developer settings \xe2\x86\x92 "
        "Personal access tokens \xe2\x86\x92 Fine-grained tokens \xe2\x86\x92 Generate new token.\n"
        "Give it only this one repository, with Contents: read-only. The push tool needs\n"
        "a second token with read and write. A relay uses its own DOWNLOAD token instead.",
        relayGroup);
    tokenHelp->setWordWrap(true);
    tokenHelp->setStyleSheet("color: rgba(150, 150, 150, 220);");
    relayGrid->addWidget(tokenHelp, 3, 1, 1, 3);

    relayGrid->addWidget(new QLabel("Check every:", relayGroup), 4, 0);
    this->spinBoxRelayPoll = new QSpinBox(relayGroup);
    this->spinBoxRelayPoll->setRange(1, 1440);
    this->spinBoxRelayPoll->setSuffix(" min");
    this->spinBoxRelayPoll->setValue(RelayClient::pollMinutes());
    relayGrid->addWidget(this->spinBoxRelayPoll, 4, 1);

    relayGrid->addWidget(new QLabel("Packs:", relayGroup), 5, 0);
    this->lineEditRelayPacks = new QLineEdit(RelayClient::packFilter().join(", "), relayGroup);
    this->lineEditRelayPacks->setPlaceholderText("leave empty to follow every pack at the source");
    this->lineEditRelayPacks->setToolTip(
        "A comma-separated list, so one relay or one repository can carry every venue\n"
        "while this client takes only the packs that are its own.");
    relayGrid->addWidget(this->lineEditRelayPacks, 5, 1, 1, 3);

    this->checkBoxRelayPacksLocal = new QCheckBox(
        "Ignore what this machine is assigned and use the list above", relayGroup);
    this->checkBoxRelayPacksLocal->setToolTip(
        "Normally the source decides which packs this machine takes, so one\n"
        "person can run the whole estate from one place. Tick this and the\n"
        "field above wins here instead, for the one machine that has to differ.");
    this->checkBoxRelayPacksLocal->setChecked(RelayClient::packsDecidedLocally());
    relayGrid->addWidget(this->checkBoxRelayPacksLocal, 6, 1, 1, 3);

    this->labelRelayStatus = new QLabel(RelayClient::getInstance().lastSummary(), relayGroup);
    this->labelRelayStatus->setWordWrap(true);
    relayGrid->addWidget(this->labelRelayStatus, 7, 0, 1, 2);

    QPushButton* relayTest = new QPushButton("Test", relayGroup);
    relayTest->setFixedHeight(22);
    relayTest->setFocusPolicy(Qt::NoFocus);
    relayTest->setToolTip("Reach the source and say what it is. Writes nothing.");
    relayGrid->addWidget(relayTest, 7, 2);

    QPushButton* relayCheck = new QPushButton("Check now", relayGroup);
    relayCheck->setFixedHeight(22);
    relayCheck->setFocusPolicy(Qt::NoFocus);
    relayCheck->setToolTip("Check now and install anything that differs.");
    relayGrid->addWidget(relayCheck, 7, 3);

    relayGrid->addWidget(new QLabel(
        "Nothing is ever deleted by a pull, and project.js and extensions.json are left\n"
        "alone here exactly as they are during a push.", relayGroup), 8, 0, 1, 4);

    relayGrid->addWidget(new QLabel(
        "Use a PRIVATE repository. Anyone can read a public one.", relayGroup), 9, 0, 1, 4);

    // Both buttons act on what is typed rather than on what was last saved, so a
    // test is a test of the address in front of the operator.
    auto applyRelayFields = [this]() {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "RelayUrl", this->lineEditRelayUrl->text().trimmed()));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "RelayToken", this->lineEditRelayToken->text().trimmed()));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "RelayPacks", this->lineEditRelayPacks->text().trimmed()));
    };

    QObject::connect(&RelayClient::getInstance(), &RelayClient::progress, this, [this](const QString& line) {
        if (this->labelRelayStatus != nullptr)
            this->labelRelayStatus->setText(line);
    });

    QObject::connect(relayTest, &QPushButton::clicked, this, [this, applyRelayFields]() {
        applyRelayFields();
        this->labelRelayStatus->setText("Asking the relay...");
        RelayClient::getInstance().ping();
    });

    QObject::connect(relayCheck, &QPushButton::clicked, this, [this, applyRelayFields]() {
        applyRelayFields();
        if (!RelayClient::getInstance().checkNow())
            this->labelRelayStatus->setText("Already checking, or nothing is configured.");
    });

    templatesVBox->addWidget(relayGroup);
    templatesVBox->addStretch();

    this->tabWidgetSettings->addTab(tabTemplates, "Templates");

    QObject::connect(this, &QDialog::accepted, this, [this]() {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "TemplatePushEnabled", this->checkBoxTemplatePush->isChecked() ? "true" : "false"));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "TemplatePushToken", this->lineEditTemplatePushToken->text().trimmed()));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "TemplatePushPath", this->lineEditTemplatePushPath->text().trimmed()));

        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "RelayEnabled", this->checkBoxRelayEnabled->isChecked() ? "true" : "false"));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "RelayUrl", this->lineEditRelayUrl->text().trimmed()));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "RelayToken", this->lineEditRelayToken->text().trimmed()));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "RelayPollMinutes", QString::number(this->spinBoxRelayPoll->value())));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "RelayPacks", this->lineEditRelayPacks->text().trimmed()));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "RelayPacksLocal",
                               this->checkBoxRelayPacksLocal->isChecked() ? "true" : "false"));

        // Turning the feature on has to bring the socket up, and off may be the
        // last thing keeping it up.
        SheetCacheServer::getInstance().stop();
        SheetCacheServer::getInstance().start();

        // A new address or a new interval only means anything once the timer has
        // been rebuilt on it.
        RelayClient::getInstance().start();
    });

    QObject::connect(this, &QDialog::accepted, this, [this]() {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "SheetsCacheUrl", this->lineEditSheetsCacheUrl->text().trimmed()));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "SheetsQuotaPerMinute", QString::number(this->spinBoxSheetsQuota->value())));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "SheetsStrainReportUrl", this->lineEditSheetsStrainUrl->text().trimmed()));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "SheetsHostCache", this->checkBoxHostSheetCache->isChecked() ? "true" : "false"));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "SheetsHostCachePort", QString::number(this->spinBoxSheetCachePort->value())));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "SheetsCacheDirectory", this->lineEditSheetCacheDir->text().trimmed()));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "SheetsCacheBypass", this->checkBoxSheetCacheBypass->isChecked() ? "true" : "false"));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "SheetsWarmStaleSeconds", QString::number(this->spinBoxWarmStale->value())));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "SheetsWarmSpacingMs", QString::number(this->spinBoxWarmSpacing->value())));

        // Pick up a port or folder change without a restart when we can; a port already
        // taken simply leaves it stopped, which the log says.
        SheetCacheServer::getInstance().stop();
        SheetCacheServer::getInstance().start();
    });

    // Flush any pending debounced writes before the dialog closes.
    QObject::connect(this, &QDialog::accepted, this, &SettingsDialog::flushPendingWrites);

    // Save layout configuration when the dialog is accepted.
    QObject::connect(this, &QDialog::accepted, this->layoutEditor, &LayoutEditorWidget::saveToConfig);
    QObject::connect(this, &QDialog::accepted, this->simpleLayoutEditor, &LayoutEditorWidget::saveToConfig);
    QObject::connect(this, &QDialog::accepted, this, [this]() {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "SimpleModeShowPlayStop", this->checkBoxSimplePlayStop->isChecked() ? "true" : "false"));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "SimpleModeShowPreview", this->checkBoxSimplePreview->isChecked() ? "true" : "false"));
    });

    // Save panel sizing and layout options when the dialog is accepted.
    QObject::connect(this, &QDialog::accepted, this, [this]() {
        for (const auto& entry : this->panelSizingEntries)
        {
            QString mode = entry.combo->currentData().toString();
            DatabaseManager::getInstance().updateConfiguration(
                ConfigurationModel(0, "PanelSizeMode_" + entry.panelId, mode));
        }
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "ShowEmptyPanels",
                this->checkBoxShowEmptyPanels->isChecked() ? "true" : "false"));
        EventManager::getInstance().fireRebuildLayout();
    });
}

void SettingsDialog::setupGeneralTab()
{
    // Build the entire General tab with a single QGridLayout inside a scroll area.
    QWidget* content = new QWidget();
    QGridLayout* grid = new QGridLayout(content);
    grid->setContentsMargins(10, 10, 10, 10);
    grid->setVerticalSpacing(8);
    grid->setHorizontalSpacing(8);
    grid->setColumnMinimumWidth(0, 170);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    grid->setColumnStretch(3, 1);

    int row = 0;

    // Helper: add a section header with blue underline.
    auto addSection = [&](const QString& title) {
        if (row > 0)
        {
            grid->setRowMinimumHeight(row, 12);
            row++;
        }
        QLabel* label = new QLabel(title);
        label->setStyleSheet("font-weight: bold; color: rgba(200, 200, 200, 255); padding-bottom: 2px;");
        label->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
        grid->addWidget(label, row, 0, 1, 4);
        row++;
        QFrame* line = new QFrame();
        line->setFixedHeight(1);
        line->setStyleSheet("background-color: rgba(70, 115, 195, 255);");
        grid->addWidget(line, row, 0, 1, 4);
        row++;
    };

    // Helper: add a right-aligned label at column 0.
    auto addLabel = [&](const QString& text) -> QLabel* {
        QLabel* label = new QLabel(text);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(label, row, 0);
        return label;
    };

    // Helper: add a color swatch + picker button row.
    auto addColorRow = [&](const QString& text, QLabel*& swatch) -> QPushButton* {
        addLabel(text);
        swatch = new QLabel();
        swatch->setFixedSize(60, 22);
        swatch->setAutoFillBackground(true);
        grid->addWidget(swatch, row, 1);
        QPushButton* btn = new QPushButton("...");
        btn->setFixedSize(30, 22);
        btn->setFocusPolicy(Qt::NoFocus);
        grid->addWidget(btn, row, 2);
        row++;
        return btn;
    };

    // ── Startup ──────────────────────────────────────────────
    addSection("Startup");
    this->checkBoxFullscreen = new QCheckBox("Start in fullscreen");
    this->checkBoxFullscreen->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxFullscreen, row, 1, 1, 3);
    row++;

    addLabel("Theme:");
    this->comboBoxTheme = new QComboBox();
    this->comboBoxTheme->addItems({"Curve", "Flat", "Light"});
    this->comboBoxTheme->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(this->comboBoxTheme, row, 1);
    row++;

    addLabel("Font size:");
    this->spinBoxFontSize = new QSpinBox();
    this->spinBoxFontSize->setValue(11);
    grid->addWidget(this->spinBoxFontSize, row, 1);
    grid->addWidget(new QLabel("pixels"), row, 2);
    row++;

    this->checkBoxUseDropFrameNotation = new QCheckBox("Use drop frame notation");
    this->checkBoxUseDropFrameNotation->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxUseDropFrameNotation, row, 1, 1, 3);
    row++;

    // ── Library ──────────────────────────────────────────────
    addSection("Library");
    this->checkBoxAutoRefresh = new QCheckBox("Refresh library automatically");
    this->checkBoxAutoRefresh->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxAutoRefresh, row, 1, 1, 3);
    row++;

    this->labelInterval = new QLabel("Interval:");
    this->labelInterval->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(this->labelInterval, row, 0);
    this->spinBoxRefreshInterval = new QSpinBox();
    this->spinBoxRefreshInterval->setMinimum(5);
    this->spinBoxRefreshInterval->setValue(30);
    grid->addWidget(this->spinBoxRefreshInterval, row, 1);
    this->labelSeconds = new QLabel("seconds");
    grid->addWidget(this->labelSeconds, row, 2);
    row++;

    this->checkBoxShowThumbnailTooltip = new QCheckBox("Show thumbnail tooltip");
    this->checkBoxShowThumbnailTooltip->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxShowThumbnailTooltip, row, 1, 1, 3);
    row++;

    // ── Playback ─────────────────────────────────────────────
    addSection("Playback");
    this->checkBoxReverseOscTime = new QCheckBox("Count video progress down");
    this->checkBoxReverseOscTime->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxReverseOscTime, row, 1, 1, 3);
    row++;

    this->checkBoxDisableInAndOutPoints = new QCheckBox("Disable in and out points in video progress");
    this->checkBoxDisableInAndOutPoints->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxDisableInAndOutPoints, row, 1, 1, 3);
    row++;

    this->checkBoxMarkUsedItems = new QCheckBox("Mark used items (Windows and Linux only)");
    this->checkBoxMarkUsedItems->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxMarkUsedItems, row, 1, 1, 3);
    row++;

    this->checkBoxUseFreezeOnLoad = new QCheckBox("Use freeze on load for video items");
    this->checkBoxUseFreezeOnLoad->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxUseFreezeOnLoad, row, 1, 1, 3);
    row++;

    addLabel("Duration format:");
    this->comboBoxDurationFormat = new QComboBox();
    this->comboBoxDurationFormat->addItem("Human Readable", "HumanReadable");
    this->comboBoxDurationFormat->addItem("Timecode (HH:MM:SS)", "Timecode");
    this->comboBoxDurationFormat->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    grid->addWidget(this->comboBoxDurationFormat, row, 1);
    row++;

    addLabel("Audio polling rate:");
    this->spinBoxOscRefreshRate = new QSpinBox();
    this->spinBoxOscRefreshRate->setMinimum(50);
    this->spinBoxOscRefreshRate->setMaximum(1000);
    this->spinBoxOscRefreshRate->setSingleStep(10);
    this->spinBoxOscRefreshRate->setValue(200);
    this->spinBoxOscRefreshRate->setSuffix(" ms");
    this->spinBoxOscRefreshRate->setToolTip("Lower values give smoother audio meters and faster progress updates. "
                                            "Warning: values below 100ms can cause performance issues on low-performance devices.");
    grid->addWidget(this->spinBoxOscRefreshRate, row, 1);
    row++;

    addLabel("Undo history limit:");
    this->spinBoxUndoHistoryLimit = new QSpinBox();
    this->spinBoxUndoHistoryLimit->setMinimum(10);
    this->spinBoxUndoHistoryLimit->setMaximum(1000);
    this->spinBoxUndoHistoryLimit->setSingleStep(10);
    this->spinBoxUndoHistoryLimit->setValue(50);
    grid->addWidget(this->spinBoxUndoHistoryLimit, row, 1);
    row++;

    // ── Repository ───────────────────────────────────────────
    addSection("Repository");
    addLabel("Repository URL:");
    this->lineEditRundownRepository = new QLineEdit();
    this->lineEditRundownRepository->setFocusPolicy(Qt::ClickFocus);
    this->lineEditRundownRepository->setPlaceholderText("URL");
    this->lineEditRundownRepository->setToolTip("http://<host>:<port>/urllist/<profile>");
    grid->addWidget(this->lineEditRundownRepository, row, 1, 1, 3);
    row++;

    addLabel("Repository port:");
    this->lineEditRepositoryPort = new QLineEdit();
    this->lineEditRepositoryPort->setFocusPolicy(Qt::ClickFocus);
    this->lineEditRepositoryPort->setPlaceholderText("8250");
    grid->addWidget(this->lineEditRepositoryPort, row, 1);
    row++;

    // ── Database ─────────────────────────────────────────────
    addSection("Database");
    this->checkBoxStoreThumbnailsInDatabase = new QCheckBox("Store thumbnails in database");
    this->checkBoxStoreThumbnailsInDatabase->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxStoreThumbnailsInDatabase, row, 1, 1, 3);
    row++;

    addLabel("Delete thumbnails:");
    this->pushButtonDeleteThumbnails = new QPushButton("&Delete Now!");
    this->pushButtonDeleteThumbnails->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->pushButtonDeleteThumbnails, row, 1);
    row++;

    // ── Preview ──────────────────────────────────────────────
    addSection("Preview");
    this->checkBoxShowPreviewBorder = new QCheckBox("Show preview mode border");
    this->checkBoxShowPreviewBorder->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxShowPreviewBorder, row, 1, 1, 3);
    row++;

    this->previewFreezeTemplateCheck = new QCheckBox("Load template without playing on preview (F8)");
    this->previewFreezeTemplateCheck->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->previewFreezeTemplateCheck, row, 1, 1, 3);
    row++;

    // ── Panels ───────────────────────────────────────────────
    addSection("Panels");
    this->checkBoxShowSTEPButton = new QCheckBox("Show STEP button in Server Status");
    this->checkBoxShowSTEPButton->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxShowSTEPButton, row, 1, 1, 3);
    row++;

    this->checkBoxShowPVWButton = new QCheckBox("Show PVW button in Server Status");
    this->checkBoxShowPVWButton->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxShowPVWButton, row, 1, 1, 3);
    row++;

    this->checkBoxShowServers = new QCheckBox("Show Servers in Server Status");
    this->checkBoxShowServers->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxShowServers, row, 1, 1, 3);
    row++;

    this->checkBoxShowChannelLocks = new QCheckBox("Show Channel Locks in Server Status");
    this->checkBoxShowChannelLocks->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxShowChannelLocks, row, 1, 1, 3);
    row++;

    this->checkBoxShowChannelHeaders = new QCheckBox("Show Channel Headers in Activity");
    this->checkBoxShowChannelHeaders->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxShowChannelHeaders, row, 1, 1, 3);
    row++;

    this->checkBoxShowBankIcons = new QCheckBox("Show Bank Icons in Trigger Banks");
    this->checkBoxShowBankIcons->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxShowBankIcons, row, 1, 1, 3);
    row++;

    this->checkBoxHttpLogLastOnly = new QCheckBox("Show only last response in Http Log");
    this->checkBoxHttpLogLastOnly->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxHttpLogLastOnly, row, 1, 1, 3);
    row++;

    this->checkBoxShowLastAction = new QCheckBox("Show last action in statusbar");
    this->checkBoxShowLastAction->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxShowLastAction, row, 1, 1, 3);
    row++;

    this->checkBoxActiveIndicatorPerChannel = new QCheckBox("Active indicator per channel");
    this->checkBoxActiveIndicatorPerChannel->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxActiveIndicatorPerChannel, row, 1, 1, 3);
    row++;

    addLabel("Disconnect mode:");
    this->comboBoxDisconnectMode = new QComboBox();
    this->comboBoxDisconnectMode->addItems({"ask", "hidden", "direct"});
    grid->addWidget(this->comboBoxDisconnectMode, row, 1);
    row++;

    this->checkBoxActivityGrow = new QCheckBox("Activity panel grows to fill available space");
    this->checkBoxActivityGrow->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxActivityGrow, row, 1, 1, 3);
    row++;

    addLabel("NDI Outputs:");
    this->spinBoxNdiOutputs = new QSpinBox();
    this->spinBoxNdiOutputs->setMinimum(1);
    this->spinBoxNdiOutputs->setMaximum(9);
    this->spinBoxNdiOutputs->setToolTip("Number of NDI viewer outputs in the NDI panel (1-9)");
    grid->addWidget(this->spinBoxNdiOutputs, row, 1);

    row++;
    this->checkBoxNdiRestoreOutputs = new QCheckBox("Reconnect NDI sources on startup");
    this->checkBoxNdiRestoreOutputs->setFocusPolicy(Qt::NoFocus);
    this->checkBoxNdiRestoreOutputs->setToolTip(
        "Puts each tile back on the source it was showing when the client last closed.\n"
        "Sources are reconnected as discovery finds them, and a tile filled by hand is left alone.");
    grid->addWidget(this->checkBoxNdiRestoreOutputs, row, 1, 1, 3);
    row++;

    addLabel("NDI Bandwidth:");
    this->comboBoxNdiBandwidth = new QComboBox();
    this->comboBoxNdiBandwidth->addItem("High (Full Quality)", "high");
    this->comboBoxNdiBandwidth->addItem("Low (Reduced Quality)", "low");
    this->comboBoxNdiBandwidth->setToolTip("Low bandwidth reduces resolution and CPU usage significantly");
    grid->addWidget(this->comboBoxNdiBandwidth, row, 1);
    row++;

    addLabel("NDI Frame Rate Limit:");
    this->comboBoxNdiFpsLimit = new QComboBox();
    this->comboBoxNdiFpsLimit->addItem("Off (Source Rate)", 0);
    this->comboBoxNdiFpsLimit->addItem("30 fps", 30);
    this->comboBoxNdiFpsLimit->addItem("15 fps", 15);
    this->comboBoxNdiFpsLimit->addItem("10 fps", 10);
    this->comboBoxNdiFpsLimit->addItem("5 fps", 5);
    this->comboBoxNdiFpsLimit->setToolTip("Limit received frame rate to reduce CPU usage");
    grid->addWidget(this->comboBoxNdiFpsLimit, row, 1);
    row++;

    addLabel("NDI Scaling:");
    this->comboBoxNdiScaling = new QComboBox();
    this->comboBoxNdiScaling->addItem("Smooth (High Quality)", "smooth");
    this->comboBoxNdiScaling->addItem("Fast (Low CPU)", "fast");
    this->comboBoxNdiScaling->setToolTip("Fast scaling uses less CPU but lower visual quality");
    grid->addWidget(this->comboBoxNdiScaling, row, 1);
    row++;

    // ── Clock ────────────────────────────────────────────────
    addSection("Clock");
    this->checkBoxDualClock = new QCheckBox("Show second clock");
    this->checkBoxDualClock->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxDualClock, row, 1, 1, 3);
    row++;

    addLabel("Clock Timezone 1:");
    this->comboBoxTimezone1 = new QComboBox();
    grid->addWidget(this->comboBoxTimezone1, row, 1, 1, 2);
    row++;

    addLabel("Clock Timezone 2:");
    this->comboBoxTimezone2 = new QComboBox();
    grid->addWidget(this->comboBoxTimezone2, row, 1, 1, 2);
    row++;

    addLabel("Show labels:");
    this->checkBoxClockShowLabels = new QCheckBox();
    grid->addWidget(this->checkBoxClockShowLabels, row, 1);
    row++;

    addLabel("Stack clocks:");
    this->checkBoxClockStacked = new QCheckBox();
    grid->addWidget(this->checkBoxClockStacked, row, 1);
    row++;

    QPushButton* btnClock1 = addColorRow("Clock 1 color:", this->swatchClock1);
    QPushButton* btnClock2 = addColorRow("Clock 2 color:", this->swatchClock2);
    QPushButton* btnClockShadow = addColorRow("Shadow color:", this->swatchClockShadow);

    // ── Channel Colors ───────────────────────────────────────
    addSection("Channel Colors");

    auto addSliderRow = [&](const QString& text, int minVal, int maxVal, int defaultVal) -> QSlider* {
        addLabel(text);
        QSlider* slider = new QSlider(Qt::Horizontal);
        slider->setRange(minVal, maxVal);
        slider->setValue(defaultVal);
        slider->setFocusPolicy(Qt::NoFocus);
        grid->addWidget(slider, row, 1);
        QLabel* valLabel = new QLabel();
        valLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(valLabel, row, 2);
        row++;
        return slider;
    };

    this->sliderAngle = addSliderRow("Color spacing:", 0, 3600, 1375);
    this->labelAngleValue = qobject_cast<QLabel*>(grid->itemAtPosition(row - 1, 2)->widget());

    this->sliderOffset = addSliderRow("Color offset:", 0, 360, 120);
    this->labelOffsetValue = qobject_cast<QLabel*>(grid->itemAtPosition(row - 1, 2)->widget());

    this->sliderSaturation = addSliderRow("Saturation:", 0, 100, 65);
    this->labelSaturationValue = qobject_cast<QLabel*>(grid->itemAtPosition(row - 1, 2)->widget());

    this->sliderLightness = addSliderRow("Lightness:", 0, 100, 30);
    this->labelLightnessValue = qobject_cast<QLabel*>(grid->itemAtPosition(row - 1, 2)->widget());

    addLabel("Preview:");
    QWidget* previewContainer = new QWidget();
    QHBoxLayout* previewLayout = new QHBoxLayout(previewContainer);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(4);
    for (int i = 0; i < 5; i++)
    {
        this->channelPreview[i] = new QLabel(QString::number(i + 1));
        this->channelPreview[i]->setFixedSize(24, 24);
        this->channelPreview[i]->setAlignment(Qt::AlignCenter);
        this->channelPreview[i]->setStyleSheet("color: white; font-size: 10px; font-weight: bold; border-radius: 3px;");
        previewLayout->addWidget(this->channelPreview[i]);
    }
    previewLayout->addStretch();
    grid->addWidget(previewContainer, row, 1, 1, 3);
    row++;

    // ── Interface Colors ─────────────────────────────────────
    addSection("Interface Colors");

    QPushButton* btnPVW = addColorRow("PVW button:", this->swatchPVW);
    QPushButton* btnSTEP = addColorRow("STEP button:", this->swatchSTEP);
    QPushButton* btnPreviewBorder = addColorRow("Preview border:", this->swatchPreviewBorder);
    QPushButton* btnAutostepHL = addColorRow("Autostep highlight:", this->swatchAutostepHighlight);
    QPushButton* btnActiveInd = addColorRow("Active indicator:", this->swatchActiveIndicator);
    QPushButton* btnSectionLine = addColorRow("Library section line:", this->swatchLibrarySectionLine);

    // ── Header Colors ───────────────────────────────────────
    addSection("Header Colors");

    // Column headers for the 3 swatch columns.
    auto makeColHeader = [](const QString& text) {
        QLabel* lbl = new QLabel(text);
        lbl->setAlignment(Qt::AlignCenter | Qt::AlignBottom);
        lbl->setStyleSheet("color: rgba(160, 160, 160, 255); font-size: 11px;");
        return lbl;
    };
    grid->addWidget(makeColHeader("Line"), row, 1);
    grid->addWidget(makeColHeader("Block"), row, 2);
    grid->addWidget(makeColHeader("Text"), row, 3);
    row++;

    // Helper: clickable swatch (QPushButton styled as color square).
    auto makeSwatch = [](QLabel*& swatch) {
        swatch = new QLabel();
        swatch->setFixedSize(40, 22);
        swatch->setAutoFillBackground(true);
        swatch->setCursor(Qt::PointingHandCursor);
        return swatch;
    };

    // Helper: add a triple-swatch row (label + 3 color swatches).
    auto addTripleRow = [&](const QString& text, QLabel*& s1, QLabel*& s2, QLabel*& s3) {
        addLabel(text);
        grid->addWidget(makeSwatch(s1), row, 1, Qt::AlignHCenter);
        grid->addWidget(makeSwatch(s2), row, 2, Qt::AlignHCenter);
        grid->addWidget(makeSwatch(s3), row, 3, Qt::AlignHCenter);
        row++;
    };

    addTripleRow("Master:", this->swatchMasterLine, this->swatchMasterBlock, this->swatchMasterText);
    addTripleRow("Active Rundown:", this->swatchRundownLine, this->swatchRundownBlock, this->swatchRundownText);

    this->checkBoxCustomizeHeaders = new QCheckBox("Customize individually");
    this->checkBoxCustomizeHeaders->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(this->checkBoxCustomizeHeaders, row, 1, 1, 3);
    row++;

    // Per-widget color pickers (hidden when customize is unchecked).
    // Rundown is always visible above; these are the remaining 9 panels.
    static const char* panelLabels[] = {
        "Library:", "Inspector:", "Audio Levels:", "Preview:",
        "Live:", "Clock:", "Server Status:", "Activity:", "Trigger Banks:",
        "NDI:", "Performance:"
    };
    for (int i = 0; i < HEADER_PANEL_COUNT; i++)
    {
        addTripleRow(QString("  %1").arg(panelLabels[i]),
                     this->swatchLine[i], this->swatchBlock[i], this->swatchText[i]);
        this->panelRowWidgets[i] = grid->itemAtPosition(row - 1, 0)->widget();
    }

    // Trailing stretch so content doesn't spread vertically.
    grid->setRowStretch(row, 1);

    // Wrap in scroll area and assign to tabGeneral.
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(content);

    QVBoxLayout* tabLayout = new QVBoxLayout(this->tabGeneral);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scrollArea);

    // ── Load saved values from DB ────────────────────────────

    bool startFullscreen = (DatabaseManager::getInstance().getConfigurationByName("StartFullscreen").getValue() == "true");
    this->checkBoxFullscreen->setChecked(startFullscreen);

    this->comboBoxTheme->setCurrentIndex(this->comboBoxTheme->findText(DatabaseManager::getInstance().getConfigurationByName("Theme").getValue()));
    this->spinBoxFontSize->setValue(DatabaseManager::getInstance().getConfigurationByName("FontSize").getValue().toInt());

    bool useDropFrameNotation = (DatabaseManager::getInstance().getConfigurationByName("UseDropFrameNotation").getValue() == "true");
    this->checkBoxUseDropFrameNotation->setChecked(useDropFrameNotation);

    bool autoRefreshLibrary = (DatabaseManager::getInstance().getConfigurationByName("AutoRefreshLibrary").getValue() == "true");
    this->checkBoxAutoRefresh->setChecked(autoRefreshLibrary);
    this->labelInterval->setEnabled(autoRefreshLibrary);
    this->spinBoxRefreshInterval->setEnabled(autoRefreshLibrary);
    this->labelSeconds->setEnabled(autoRefreshLibrary);
    this->spinBoxRefreshInterval->setValue(DatabaseManager::getInstance().getConfigurationByName("RefreshLibraryInterval").getValue().toInt());

    bool showThumbnailTooltip = (DatabaseManager::getInstance().getConfigurationByName("ShowThumbnailTooltip").getValue() == "true");
    this->checkBoxShowThumbnailTooltip->setChecked(showThumbnailTooltip);

    bool reverseOscTime = (DatabaseManager::getInstance().getConfigurationByName("ReverseOscTime").getValue() == "true");
    this->checkBoxReverseOscTime->setChecked(reverseOscTime);

    bool disableInAndOutPoints = (DatabaseManager::getInstance().getConfigurationByName("DisableInAndOutPoints").getValue() == "true");
    this->checkBoxDisableInAndOutPoints->setChecked(disableInAndOutPoints);

    QString oscRefreshStr = DatabaseManager::getInstance().getConfigurationByName("OscRefreshRate").getValue();
    this->spinBoxOscRefreshRate->setValue(oscRefreshStr.isEmpty() ? Osc::DEFAULT_REFRESH_RATE : oscRefreshStr.toInt());

    QString undoLimitStr = DatabaseManager::getInstance().getConfigurationByName("UndoHistoryLimit").getValue();
    this->spinBoxUndoHistoryLimit->setValue(undoLimitStr.isEmpty() ? 50 : undoLimitStr.toInt());

    bool markUsedItems = (DatabaseManager::getInstance().getConfigurationByName("MarkUsedItems").getValue() == "true");
    this->checkBoxMarkUsedItems->setChecked(markUsedItems);

    this->lineEditRundownRepository->setText(DatabaseManager::getInstance().getConfigurationByName("RundownRepository").getValue());
    this->lineEditRepositoryPort->setPlaceholderText(QString("%1").arg(Repository::DEFAULT_PORT));
    QString repositoryPort = DatabaseManager::getInstance().getConfigurationByName("RepositoryPort").getValue();
    if (!repositoryPort.isEmpty())
        this->lineEditRepositoryPort->setText(repositoryPort);

    bool useFreezeOnLoad = (DatabaseManager::getInstance().getConfigurationByName("UseFreezeOnLoad").getValue() == "true");
    this->checkBoxUseFreezeOnLoad->setChecked(useFreezeOnLoad);
    QString durationFormat = DatabaseManager::getInstance().getConfigurationByName("DurationFormat").getValue();
    int formatIndex = this->comboBoxDurationFormat->findData(durationFormat);
    if (formatIndex >= 0)
        this->comboBoxDurationFormat->setCurrentIndex(formatIndex);

    bool storeThumbnailsInDatabase = (DatabaseManager::getInstance().getConfigurationByName("StoreThumbnailsInDatabase").getValue() == "true");
    this->checkBoxStoreThumbnailsInDatabase->setChecked(storeThumbnailsInDatabase);

    // Wire up programmatic General-tab checkboxes.
    auto wireCheckBox = [](QCheckBox* cb, const QString& key) {
        QString val = DatabaseManager::getInstance().getConfigurationByName(key).getValue();
        cb->setChecked(val.isEmpty() || val == "true");
        QObject::connect(cb, &QCheckBox::stateChanged, [key](int state) {
            QString value = (state == Qt::Checked) ? "true" : "false";
            DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, key, value));
        });
    };

    wireCheckBox(this->checkBoxShowPreviewBorder, "ShowPreviewBorder");
    wireCheckBox(this->checkBoxShowSTEPButton, "ShowSTEPButton");
    wireCheckBox(this->checkBoxShowPVWButton, "ShowPVWButton");
    wireCheckBox(this->checkBoxNdiRestoreOutputs, "NdiRestoreOutputs");
    wireCheckBox(this->checkBoxShowServers, "ShowServers");
    wireCheckBox(this->checkBoxShowChannelLocks, "ShowChannelLocks");
    wireCheckBox(this->checkBoxShowChannelHeaders, "ShowChannelHeaders");
    wireCheckBox(this->checkBoxShowBankIcons, "ShowBankIcons");
    wireCheckBox(this->checkBoxHttpLogLastOnly, "HttpLogLastOnly");
    wireCheckBox(this->checkBoxShowLastAction, "ShowLastAction");
    wireCheckBox(this->checkBoxActiveIndicatorPerChannel, "ActiveIndicatorPerChannel");

    // Disconnect mode dropdown.
    QString disconnectMode = DatabaseManager::getInstance().getConfigurationByName("DisconnectMode").getValue();
    int dmIdx = this->comboBoxDisconnectMode->findText(disconnectMode.isEmpty() ? "ask" : disconnectMode);
    if (dmIdx >= 0) this->comboBoxDisconnectMode->setCurrentIndex(dmIdx);
    QObject::connect(this->comboBoxDisconnectMode, &QComboBox::currentTextChanged, [](const QString& text) {
        DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "DisconnectMode", text));
    });

    // Template preview freeze.
    QString freezeVal = DatabaseManager::getInstance().getConfigurationByName("PreviewFreezeTemplate").getValue();
    this->previewFreezeTemplateCheck->setChecked(freezeVal == "true");
    QObject::connect(this->previewFreezeTemplateCheck, &QCheckBox::toggled, [](bool checked) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "PreviewFreezeTemplate", checked ? "true" : "false"));
    });

    // Activity grow mode.
    QString activityMode = DatabaseManager::getInstance().getConfigurationByName("ActivityGrowMode").getValue();
    this->checkBoxActivityGrow->setChecked(activityMode != "fit");
    QObject::connect(this->checkBoxActivityGrow, &QCheckBox::toggled, [](bool checked) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "ActivityGrowMode", checked ? "grow" : "fit"));
    });

    // NDI outputs.
    QString ndiCountStr = DatabaseManager::getInstance().getConfigurationByName("NdiOutputCount").getValue();
    this->spinBoxNdiOutputs->setValue(ndiCountStr.isEmpty() ? 1 : qBound(1, ndiCountStr.toInt(), 9));
    QObject::connect(this->spinBoxNdiOutputs, &QSpinBox::valueChanged, [](int value) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "NdiOutputCount", QString::number(value)));
    });

    // NDI Bandwidth.
    QString bwStr = DatabaseManager::getInstance().getConfigurationByName("NdiBandwidth").getValue();
    if (bwStr.isEmpty()) bwStr = "high";
    int bwIdx = this->comboBoxNdiBandwidth->findData(bwStr);
    if (bwIdx >= 0) this->comboBoxNdiBandwidth->setCurrentIndex(bwIdx);
    QObject::connect(this->comboBoxNdiBandwidth, &QComboBox::currentIndexChanged, [this](int index) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "NdiBandwidth", this->comboBoxNdiBandwidth->itemData(index).toString()));
    });

    // NDI FPS Limit.
    QString fpsStr = DatabaseManager::getInstance().getConfigurationByName("NdiFpsLimit").getValue();
    int fpsVal = fpsStr.isEmpty() ? 0 : fpsStr.toInt();
    int fpsIdx = this->comboBoxNdiFpsLimit->findData(fpsVal);
    if (fpsIdx >= 0) this->comboBoxNdiFpsLimit->setCurrentIndex(fpsIdx);
    QObject::connect(this->comboBoxNdiFpsLimit, &QComboBox::currentIndexChanged, [this](int index) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "NdiFpsLimit", QString::number(this->comboBoxNdiFpsLimit->itemData(index).toInt())));
    });

    // NDI Scaling.
    QString scaleStr = DatabaseManager::getInstance().getConfigurationByName("NdiScalingQuality").getValue();
    if (scaleStr.isEmpty()) scaleStr = "smooth";
    int scIdx = this->comboBoxNdiScaling->findData(scaleStr);
    if (scIdx >= 0) this->comboBoxNdiScaling->setCurrentIndex(scIdx);
    QObject::connect(this->comboBoxNdiScaling, &QComboBox::currentIndexChanged, [this](int index) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "NdiScalingQuality", this->comboBoxNdiScaling->itemData(index).toString()));
    });

    // Clock settings.
    QString dualMode = DatabaseManager::getInstance().getConfigurationByName("ClockDualMode").getValue();
    this->checkBoxDualClock->setChecked(dualMode.isEmpty() || dualMode == "true");
    QObject::connect(this->checkBoxDualClock, &QCheckBox::toggled, [](bool checked) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "ClockDualMode", checked ? "true" : "false"));
    });

    // Timezone combos.
    QList<QByteArray> tzIds = QTimeZone::availableTimeZoneIds();
    std::sort(tzIds.begin(), tzIds.end());
    QDateTime tzNow = QDateTime::currentDateTimeUtc();

    auto populateTzCombo = [&](QComboBox* combo, const QStringList& specials) {
        for (const QString& s : specials)
            combo->addItem(s, s);
        for (const QByteArray& id : tzIds)
        {
            QTimeZone tz(id);
            int off = tz.offsetFromUtc(tzNow);
            QString sign = (off >= 0) ? "+" : "-";
            int h = qAbs(off) / 3600;
            int m = (qAbs(off) % 3600) / 60;
            QString offStr = (m > 0)
                ? QString("GMT%1%2:%3").arg(sign).arg(h).arg(m, 2, 10, QChar('0'))
                : QString("GMT%1%2").arg(sign).arg(h);
            combo->addItem(QString("%1 (%2)").arg(QString::fromUtf8(id), offStr),
                           QString::fromUtf8(id));
        }
    };

    populateTzCombo(this->comboBoxTimezone1, {"Local"});
    QString curTz1 = DatabaseManager::getInstance().getConfigurationByName("ClockTimezone1").getValue();
    int tz1Idx = this->comboBoxTimezone1->findData(curTz1.isEmpty() ? "Local" : curTz1);
    if (tz1Idx >= 0) this->comboBoxTimezone1->setCurrentIndex(tz1Idx);
    QObject::connect(this->comboBoxTimezone1, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "ClockTimezone1", this->comboBoxTimezone1->currentData().toString()));
    });

    populateTzCombo(this->comboBoxTimezone2, {"UTC", "Local"});
    QString curTz2 = DatabaseManager::getInstance().getConfigurationByName("ClockTimezone2").getValue();
    int tz2Idx = this->comboBoxTimezone2->findData(curTz2.isEmpty() ? "UTC" : curTz2);
    if (tz2Idx >= 0) this->comboBoxTimezone2->setCurrentIndex(tz2Idx);
    QObject::connect(this->comboBoxTimezone2, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "ClockTimezone2", this->comboBoxTimezone2->currentData().toString()));
    });

    auto updateTz2 = [this](bool dual) {
        this->comboBoxTimezone2->setEnabled(dual);
    };
    updateTz2(this->checkBoxDualClock->isChecked());
    QObject::connect(this->checkBoxDualClock, &QCheckBox::toggled, updateTz2);

    // ── Channel color sliders ────────────────────────────────
    auto loadSlider = [](QSlider* slider, QLabel* valLabel, const QString& dbKey, int defaultVal, double divisor) {
        QString dbVal = DatabaseManager::getInstance().getConfigurationByName(dbKey).getValue();
        int v = dbVal.isEmpty() ? defaultVal : static_cast<int>(dbVal.toDouble() * divisor + 0.5);
        slider->setValue(v);
        if (divisor > 1.0)
            valLabel->setText(QString::number(v / divisor, 'f', 1));
        else
            valLabel->setText(QString::number(v));
    };

    loadSlider(this->sliderAngle, this->labelAngleValue, "ChannelColorAngle", 1375, 10.0);
    loadSlider(this->sliderOffset, this->labelOffsetValue, "ChannelColorOffset", 120, 1.0);
    loadSlider(this->sliderSaturation, this->labelSaturationValue, "ChannelColorSaturation", 65, 100.0);
    loadSlider(this->sliderLightness, this->labelLightnessValue, "ChannelColorLightness", 30, 100.0);

    ChannelColor::setAngle(this->sliderAngle->value() / 10.0);
    ChannelColor::setOffset(this->sliderOffset->value());
    ChannelColor::setSaturation(this->sliderSaturation->value() / 100.0);
    ChannelColor::setLightness(this->sliderLightness->value() / 100.0);
    updateChannelPreview();

    auto wireSlider = [this](QSlider* slider, QLabel* valLabel, const QString& dbKey, double divisor, auto setter) {
        QObject::connect(slider, &QSlider::valueChanged, [this, valLabel, dbKey, divisor, setter](int v) {
            double real = v / divisor;
            if (divisor > 1.0)
                valLabel->setText(QString::number(real, 'f', 1));
            else
                valLabel->setText(QString::number(v));
            setter(real);
            updateChannelPreview();
            this->pendingSliderDbWrites[dbKey] = QString::number(real);
            this->sliderDbWriteTimer->start();
        });
    };

    wireSlider(this->sliderAngle, this->labelAngleValue, "ChannelColorAngle", 10.0, [](double v) { ChannelColor::setAngle(v); });
    wireSlider(this->sliderOffset, this->labelOffsetValue, "ChannelColorOffset", 1.0, [](double v) { ChannelColor::setOffset(v); });
    wireSlider(this->sliderSaturation, this->labelSaturationValue, "ChannelColorSaturation", 100.0, [](double v) { ChannelColor::setSaturation(v); });
    wireSlider(this->sliderLightness, this->labelLightnessValue, "ChannelColorLightness", 100.0, [](double v) { ChannelColor::setLightness(v); });

    // ── Interface color pickers ──────────────────────────────
    auto loadSwatch = [](QLabel* swatch, const QString& dbKey, const QColor& defaultColor) {
        QString dbVal = DatabaseManager::getInstance().getConfigurationByName(dbKey).getValue();
        QColor color = dbVal.isEmpty() ? defaultColor : QColor(dbVal);
        if (!color.isValid()) color = defaultColor;
        swatch->setStyleSheet(QString("background-color: %1; border: 1px solid rgba(80,80,80,200); border-radius: 3px;").arg(color.name(QColor::HexArgb)));
        swatch->setProperty("currentColor", color);
    };

    loadSwatch(this->swatchPVW, "PVWButtonColor", QColor(200, 150, 0));
    loadSwatch(this->swatchSTEP, "STEPButtonColor", QColor(100, 60, 160));
    loadSwatch(this->swatchPreviewBorder, "PreviewBorderColor", QColor(230, 200, 40, 220));
    loadSwatch(this->swatchAutostepHighlight, "AutostepHighlightColor", QColor(100, 60, 160, 40));
    loadSwatch(this->swatchActiveIndicator, "ActiveIndicatorColor", QColor(0, 0, 0, 0));
    loadSwatch(this->swatchLibrarySectionLine, "LibrarySectionLineColor", QColor(76, 175, 80));

    // ── Header color pickers (Line / Block / Text) ─────────
    QColor defaultLine(70, 115, 195);
    QColor defaultBlock(70, 115, 195);
    QColor defaultText(255, 255, 255);

    loadSwatch(this->swatchMasterLine,  "HeaderLineMaster",  defaultLine);
    loadSwatch(this->swatchMasterBlock, "HeaderBlockMaster", defaultBlock);
    loadSwatch(this->swatchMasterText,  "HeaderTextMaster",  defaultText);
    loadSwatch(this->swatchRundownLine,  "HeaderLineRundown",  defaultLine);
    loadSwatch(this->swatchRundownBlock, "HeaderBlockRundown", defaultBlock);
    loadSwatch(this->swatchRundownText,  "HeaderTextRundown",  defaultText);

    static const char* panelDbKeys[] = {
        "Library", "Inspector", "AudioLevels", "Preview",
        "Live", "Clock", "ServerStatus", "Activity", "TriggerBanks",
        "NDI", "Performance"
    };
    for (int i = 0; i < HEADER_PANEL_COUNT; i++)
    {
        loadSwatch(this->swatchLine[i],  QString("HeaderLine_%1").arg(panelDbKeys[i]),  defaultLine);
        loadSwatch(this->swatchBlock[i], QString("HeaderBlock_%1").arg(panelDbKeys[i]), defaultBlock);
        loadSwatch(this->swatchText[i],  QString("HeaderText_%1").arg(panelDbKeys[i]),  defaultText);
    }

    // Show/hide per-widget rows based on customize checkbox.
    bool customizeHeaders = DatabaseManager::getInstance().getConfigurationByName("CustomizeWidgetHeaders").getValue() == "true";
    this->checkBoxCustomizeHeaders->setChecked(customizeHeaders);
    auto setHeaderRowsVisible = [this](bool visible) {
        for (int i = 0; i < HEADER_PANEL_COUNT; i++)
        {
            this->swatchLine[i]->setVisible(visible);
            this->swatchBlock[i]->setVisible(visible);
            this->swatchText[i]->setVisible(visible);
            if (this->panelRowWidgets[i])
                this->panelRowWidgets[i]->setVisible(visible);
        }
    };
    setHeaderRowsVisible(customizeHeaders);
    QObject::connect(this->checkBoxCustomizeHeaders, &QCheckBox::toggled, [this, setHeaderRowsVisible](bool checked) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "CustomizeWidgetHeaders", checked ? "true" : "false"));
        ColorCache::setCustomizeHeaders(checked);
        setHeaderRowsVisible(checked);
        qApp->setStyleSheet(this->stylesheet + WidgetHeaderCSS::generate());
    });

    // Populate ColorCache from DB.
    ColorCache::setPvwButton(DatabaseManager::getInstance().getConfigurationByName("PVWButtonColor").getValue());
    ColorCache::setStepButton(DatabaseManager::getInstance().getConfigurationByName("STEPButtonColor").getValue());
    ColorCache::setPreviewBorder(DatabaseManager::getInstance().getConfigurationByName("PreviewBorderColor").getValue());
    ColorCache::setAutostepHighlight(DatabaseManager::getInstance().getConfigurationByName("AutostepHighlightColor").getValue());
    ColorCache::setActiveIndicator(DatabaseManager::getInstance().getConfigurationByName("ActiveIndicatorColor").getValue());
    ColorCache::setLibrarySectionLine(DatabaseManager::getInstance().getConfigurationByName("LibrarySectionLineColor").getValue());
    ColorCache::setCustomizeHeaders(customizeHeaders);

    ColorCache::setHeaderLineMaster(DatabaseManager::getInstance().getConfigurationByName("HeaderLineMaster").getValue());
    ColorCache::setHeaderBlockMaster(DatabaseManager::getInstance().getConfigurationByName("HeaderBlockMaster").getValue());
    ColorCache::setHeaderTextMaster(DatabaseManager::getInstance().getConfigurationByName("HeaderTextMaster").getValue());
    ColorCache::setHeaderLineRundown(DatabaseManager::getInstance().getConfigurationByName("HeaderLineRundown").getValue());
    ColorCache::setHeaderBlockRundown(DatabaseManager::getInstance().getConfigurationByName("HeaderBlockRundown").getValue());
    ColorCache::setHeaderTextRundown(DatabaseManager::getInstance().getConfigurationByName("HeaderTextRundown").getValue());

    for (int i = 0; i < HEADER_PANEL_COUNT; i++)
    {
        QString lineVal  = DatabaseManager::getInstance().getConfigurationByName(QString("HeaderLine_%1").arg(panelDbKeys[i])).getValue();
        QString blockVal = DatabaseManager::getInstance().getConfigurationByName(QString("HeaderBlock_%1").arg(panelDbKeys[i])).getValue();
        QString textVal  = DatabaseManager::getInstance().getConfigurationByName(QString("HeaderText_%1").arg(panelDbKeys[i])).getValue();
        if (!lineVal.isEmpty())  ColorCache::setLineOverride(panelDbKeys[i], lineVal);
        if (!blockVal.isEmpty()) ColorCache::setBlockOverride(panelDbKeys[i], blockVal);
        if (!textVal.isEmpty())  ColorCache::setTextOverride(panelDbKeys[i], textVal);
    }

    ColorCache::setClockColor1(DatabaseManager::getInstance().getConfigurationByName("ClockColor1").getValue());
    ColorCache::setClockColor2(DatabaseManager::getInstance().getConfigurationByName("ClockColor2").getValue());
    ColorCache::setClockShadow(DatabaseManager::getInstance().getConfigurationByName("ClockShadowColor").getValue());

    // Wire interface color buttons.
    auto wireColorBtn = [this](QPushButton* btn, QLabel* swatch, const QString& dbKey, bool alpha) {
        QObject::connect(btn, &QPushButton::clicked, [this, swatch, dbKey, alpha]() {
            openColorPicker(swatch, dbKey, alpha);
        });
    };

    wireColorBtn(btnPVW, this->swatchPVW, "PVWButtonColor", false);
    wireColorBtn(btnSTEP, this->swatchSTEP, "STEPButtonColor", false);
    wireColorBtn(btnPreviewBorder, this->swatchPreviewBorder, "PreviewBorderColor", true);
    wireColorBtn(btnAutostepHL, this->swatchAutostepHighlight, "AutostepHighlightColor", true);
    wireColorBtn(btnActiveInd, this->swatchActiveIndicator, "ActiveIndicatorColor", true);
    wireColorBtn(btnSectionLine, this->swatchLibrarySectionLine, "LibrarySectionLineColor", false);

    // Wire header color swatches (clickable swatches — click opens color picker).
    auto wireSwatchClick = [this](QLabel* swatch, const QString& dbKey, bool alpha) {
        swatch->installEventFilter(this);
        swatch->setProperty("dbKey", dbKey);
        swatch->setProperty("alpha", alpha);
    };

    wireSwatchClick(this->swatchMasterLine,  "HeaderLineMaster",  false);
    wireSwatchClick(this->swatchMasterBlock, "HeaderBlockMaster", false);
    wireSwatchClick(this->swatchMasterText,  "HeaderTextMaster",  false);
    wireSwatchClick(this->swatchRundownLine,  "HeaderLineRundown",  false);
    wireSwatchClick(this->swatchRundownBlock, "HeaderBlockRundown", false);
    wireSwatchClick(this->swatchRundownText,  "HeaderTextRundown",  false);

    for (int i = 0; i < HEADER_PANEL_COUNT; i++)
    {
        wireSwatchClick(this->swatchLine[i],  QString("HeaderLine_%1").arg(panelDbKeys[i]),  false);
        wireSwatchClick(this->swatchBlock[i], QString("HeaderBlock_%1").arg(panelDbKeys[i]), false);
        wireSwatchClick(this->swatchText[i],  QString("HeaderText_%1").arg(panelDbKeys[i]),  false);
    }

    // ── Clock color pickers ──────────────────────────────────
    loadSwatch(this->swatchClock1, "ClockColor1", QColor(220, 220, 220, 230));
    loadSwatch(this->swatchClock2, "ClockColor2", QColor(80, 200, 200, 220));
    loadSwatch(this->swatchClockShadow, "ClockShadowColor", QColor(55, 55, 55, 255));

    wireColorBtn(btnClock1, this->swatchClock1, "ClockColor1", true);
    wireColorBtn(btnClock2, this->swatchClock2, "ClockColor2", true);
    wireColorBtn(btnClockShadow, this->swatchClockShadow, "ClockShadowColor", false);

    QString showLabels = DatabaseManager::getInstance().getConfigurationByName("ClockShowLabels").getValue();
    this->checkBoxClockShowLabels->setChecked(showLabels.isEmpty() || showLabels == "true");
    QObject::connect(this->checkBoxClockShowLabels, &QCheckBox::toggled, [](bool checked) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "ClockShowLabels", checked ? "true" : "false"));
    });

    QString clockStacked = DatabaseManager::getInstance().getConfigurationByName("ClockStacked").getValue();
    this->checkBoxClockStacked->setChecked(clockStacked == "true");
    QObject::connect(this->checkBoxClockStacked, &QCheckBox::toggled, [](bool checked) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "ClockStacked", checked ? "true" : "false"));
    });

    // ── Signal/slot wiring for ex-.ui widgets ────────────────
    QObject::connect(this->checkBoxFullscreen, SIGNAL(stateChanged(int)), this, SLOT(startFullscreenChanged(int)));
    QObject::connect(this->comboBoxTheme, SIGNAL(currentTextChanged(QString)), this, SLOT(themeChanged(QString)));
    QObject::connect(this->spinBoxFontSize, SIGNAL(valueChanged(int)), this, SLOT(fontSizeChanged(int)));
    QObject::connect(this->checkBoxUseDropFrameNotation, SIGNAL(stateChanged(int)), this, SLOT(useDropFrameNotationChanged(int)));
    QObject::connect(this->checkBoxAutoRefresh, SIGNAL(stateChanged(int)), this, SLOT(autoSynchronizeChanged(int)));
    QObject::connect(this->spinBoxRefreshInterval, SIGNAL(valueChanged(int)), this, SLOT(synchronizeIntervalChanged(int)));
    QObject::connect(this->checkBoxShowThumbnailTooltip, SIGNAL(stateChanged(int)), this, SLOT(showThumbnailTooltipChanged(int)));
    QObject::connect(this->checkBoxReverseOscTime, SIGNAL(stateChanged(int)), this, SLOT(reverseOscTimeChanged(int)));
    QObject::connect(this->checkBoxDisableInAndOutPoints, SIGNAL(stateChanged(int)), this, SLOT(disableInAndOutPointsChanged(int)));
    QObject::connect(this->checkBoxMarkUsedItems, SIGNAL(stateChanged(int)), this, SLOT(markUsedItemsChanged(int)));
    QObject::connect(this->lineEditRundownRepository, SIGNAL(editingFinished()), this, SLOT(rundownRepositoryChanged()));
    QObject::connect(this->lineEditRepositoryPort, SIGNAL(editingFinished()), this, SLOT(repositoryPortChanged()));
    QObject::connect(this->checkBoxUseFreezeOnLoad, SIGNAL(stateChanged(int)), this, SLOT(useFreezeOnLoadChanged(int)));
    QObject::connect(this->comboBoxDurationFormat, SIGNAL(currentTextChanged(QString)), this, SLOT(durationFormatChanged(QString)));
    QObject::connect(this->checkBoxStoreThumbnailsInDatabase, SIGNAL(stateChanged(int)), this, SLOT(storeThumbnailsInDatabaseChanged(int)));
    QObject::connect(this->pushButtonDeleteThumbnails, SIGNAL(clicked()), this, SLOT(deleteThumbnails()));
    QObject::connect(this->spinBoxOscRefreshRate, QOverload<int>::of(&QSpinBox::valueChanged), [](int value) {
        DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "OscRefreshRate", QString::number(value)));
    });
    QObject::connect(this->spinBoxUndoHistoryLimit, QOverload<int>::of(&QSpinBox::valueChanged), [](int value) {
        DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "UndoHistoryLimit", QString::number(value)));
        EventManager::getInstance().fireUndoLimitChangedEvent(value);
    });
}

void SettingsDialog::blockAllSignals(bool block)
{
    this->comboBoxLogLevel->blockSignals(block);
}

void SettingsDialog::loadDevice()
{
    this->treeWidgetDevice->clear();
    this->treeWidgetDevice->headerItem()->setText(1, "");
    this->treeWidgetDevice->setColumnHidden(0, true);
    this->treeWidgetDevice->setColumnWidth(1, 25);
    this->treeWidgetDevice->setColumnWidth(4, 50);

    QList<DeviceModel> models = DatabaseManager::getInstance().getDevice();
    foreach (DeviceModel model, models)
    {
        QTreeWidgetItem* treeItem = new QTreeWidgetItem(this->treeWidgetDevice);
        treeItem->setText(0, QString("%1").arg(model.getId()));
        treeItem->setIcon(1, QIcon(":/Graphics/Images/ServerSmall.png"));
        treeItem->setText(2, model.getName());
        treeItem->setText(3, model.getAddress());
        treeItem->setText(4, QString("%1").arg(model.getPort()));
        treeItem->setText(5, model.getDescription());
        treeItem->setText(6, model.getUsername());

        QString password = model.getPassword();
        treeItem->setText(7, password.replace(QRegularExpression("."), "*"));

        treeItem->setText(8, model.getVersion());
        treeItem->setText(9, model.getShadow());

        if (model.getChannels() > 0)
            treeItem->setText(10, QString("%1").arg(model.getChannels()));

        treeItem->setText(11, model.getChannelFormats());

        if (model.getPreviewChannel() > 0)
            treeItem->setText(12, QString("%1").arg(model.getPreviewChannel()));

        if (model.getLockedChannel() > 0)
            treeItem->setText(13, QString("%1").arg(model.getLockedChannel()));
    }

    checkEmptyDeviceList();
}


void SettingsDialog::loadGpi()
{
    QList<GpiPortModel> inputs = DatabaseManager::getInstance().getGpiPorts();

    const QList<Playout::PlayoutType>& actions = Playout::enumConstants();
    this->comboBoxAction1->setCurrentIndex(actions.indexOf(inputs.at(0).getAction()));
    this->comboBoxAction2->setCurrentIndex(actions.indexOf(inputs.at(1).getAction()));
    this->comboBoxAction3->setCurrentIndex(actions.indexOf(inputs.at(2).getAction()));
    this->comboBoxAction4->setCurrentIndex(actions.indexOf(inputs.at(3).getAction()));
    this->comboBoxAction5->setCurrentIndex(actions.indexOf(inputs.at(4).getAction()));
    this->comboBoxAction6->setCurrentIndex(actions.indexOf(inputs.at(5).getAction()));
    this->comboBoxAction7->setCurrentIndex(actions.indexOf(inputs.at(6).getAction()));
    this->comboBoxAction8->setCurrentIndex(actions.indexOf(inputs.at(7).getAction()));

    this->comboBoxGpiVoltageChange1->setCurrentIndex(inputs.at(0).isRisingEdge() ? 0 : 1);
    this->comboBoxGpiVoltageChange2->setCurrentIndex(inputs.at(1).isRisingEdge() ? 0 : 1);
    this->comboBoxGpiVoltageChange3->setCurrentIndex(inputs.at(2).isRisingEdge() ? 0 : 1);
    this->comboBoxGpiVoltageChange4->setCurrentIndex(inputs.at(3).isRisingEdge() ? 0 : 1);
    this->comboBoxGpiVoltageChange5->setCurrentIndex(inputs.at(4).isRisingEdge() ? 0 : 1);
    this->comboBoxGpiVoltageChange6->setCurrentIndex(inputs.at(5).isRisingEdge() ? 0 : 1);
    this->comboBoxGpiVoltageChange7->setCurrentIndex(inputs.at(6).isRisingEdge() ? 0 : 1);
    this->comboBoxGpiVoltageChange8->setCurrentIndex(inputs.at(7).isRisingEdge() ? 0 : 1);

    QList<GpoPortModel> outputs = DatabaseManager::getInstance().getGpoPorts();

    this->comboBoxGpoVoltageChange1->setCurrentIndex(outputs.at(0).isRisingEdge() ? 0 : 1);
    this->comboBoxGpoVoltageChange2->setCurrentIndex(outputs.at(1).isRisingEdge() ? 0 : 1);
    this->comboBoxGpoVoltageChange3->setCurrentIndex(outputs.at(2).isRisingEdge() ? 0 : 1);
    this->comboBoxGpoVoltageChange4->setCurrentIndex(outputs.at(3).isRisingEdge() ? 0 : 1);
    this->comboBoxGpoVoltageChange5->setCurrentIndex(outputs.at(4).isRisingEdge() ? 0 : 1);
    this->comboBoxGpoVoltageChange6->setCurrentIndex(outputs.at(5).isRisingEdge() ? 0 : 1);
    this->comboBoxGpoVoltageChange7->setCurrentIndex(outputs.at(6).isRisingEdge() ? 0 : 1);
    this->comboBoxGpoVoltageChange8->setCurrentIndex(outputs.at(7).isRisingEdge() ? 0 : 1);

    this->spinBoxPulseLength1->setValue(outputs.at(0).getPulseLengthMillis());
    this->spinBoxPulseLength2->setValue(outputs.at(1).getPulseLengthMillis());
    this->spinBoxPulseLength3->setValue(outputs.at(2).getPulseLengthMillis());
    this->spinBoxPulseLength4->setValue(outputs.at(3).getPulseLengthMillis());
    this->spinBoxPulseLength5->setValue(outputs.at(4).getPulseLengthMillis());
    this->spinBoxPulseLength6->setValue(outputs.at(5).getPulseLengthMillis());
    this->spinBoxPulseLength7->setValue(outputs.at(6).getPulseLengthMillis());
    this->spinBoxPulseLength8->setValue(outputs.at(7).getPulseLengthMillis());

    QString serialPort = DatabaseManager::getInstance().getConfigurationByName("GpiSerialPort").getValue();
    int baudRate = DatabaseManager::getInstance().getConfigurationByName("GpiBaudRate").getValue().toInt();

    this->lineEditSerialPort->setText(serialPort);
    this->comboBoxGpiBaudRate->setCurrentIndex(comboBoxGpiBaudRate->findText(QString("%1").arg(baudRate)));
}

void SettingsDialog::loadOscOutput()
{
    this->treeWidgetOscOutput->clear();
    this->treeWidgetOscOutput->headerItem()->setText(1, "");
    this->treeWidgetOscOutput->setColumnHidden(0, true);
    this->treeWidgetOscOutput->setColumnWidth(1, 25);
    this->treeWidgetOscOutput->setColumnWidth(4, 50);

    QList<OscOutputModel> models = DatabaseManager::getInstance().getOscOutput();
    foreach (OscOutputModel model, models)
    {
        QTreeWidgetItem* treeItem = new QTreeWidgetItem(this->treeWidgetOscOutput);
        treeItem->setText(0, QString("%1").arg(model.getId()));
        treeItem->setIcon(1, QIcon(":/Graphics/Images/ServerSmall.png"));
        treeItem->setText(2, model.getName());
        treeItem->setText(3, model.getAddress());
        treeItem->setText(4, QString("%1").arg(model.getPort()));
        treeItem->setText(5, model.getDescription());
    }

    checkEmptyOscOutputList();
}

void SettingsDialog::checkEmptyDeviceList()
{
    if (this->treeWidgetDevice->invisibleRootItem()->childCount() == 0)
    {
        this->tabWidgetSettings->setCurrentIndex(2);
        this->treeWidgetDevice->setStyleSheet("border-color: firebrick;");
    }
    else
        this->treeWidgetDevice->setStyleSheet("");
}

void SettingsDialog::checkEmptyOscOutputList()
{
    if (this->treeWidgetOscOutput->invisibleRootItem()->childCount() == 0)
        this->treeWidgetOscOutput->setStyleSheet("border-color: firebrick;");
    else
        this->treeWidgetOscOutput->setStyleSheet("");
}

void SettingsDialog::showImportDeviceDialog()
{
    QString path("./CasparCG.xml");

    QFile file(path);
    if (!file.exists())
        path = QFileDialog::getOpenFileName(this, "Import CasparCG Servers", "", "CasparCG (*.xml)");

    if (!path.isEmpty())
    {
        ImportDeviceDialog* dialog = new ImportDeviceDialog(this);
        dialog->setImportFile(path);
        if (dialog->exec() == QDialog::Accepted)
        {
            QList<DeviceModel> models = dialog->getDevice();
            foreach (DeviceModel model, models)
            {
                DatabaseManager::getInstance().insertDevice(DeviceModel(0, model.getName(), model.getAddress(),
                                                                        model.getPort(), model.getUsername(),
                                                                        model.getPassword(), model.getDescription(),
                                                                        "", model.getShadow(), 0, "", model.getPreviewChannel(),
                                                                        model.getLockedChannel()));
            }

            loadDevice();

            EventManager::getInstance().fireRefreshLibraryEvent(RefreshLibraryEvent());
        }
    }
}

void SettingsDialog::showAddDeviceDialog()
{
    DeviceDialog* dialog = new DeviceDialog(this);
    if (dialog->exec() == QDialog::Accepted)
    {
        QString error = DatabaseManager::getInstance().insertDevice(DeviceModel(0, dialog->getName(), dialog->getAddress(),
                                                                dialog->getPort().toInt(), dialog->getUsername(),
                                                                dialog->getPassword(), dialog->getDescription(),
                                                                "", dialog->getShadow(), 0, "", dialog->getPreviewChannel(),
                                                                dialog->getLockedChannel(), dialog->getTemplatePath(),
                                                                dialog->getMediaPath(), dialog->getServerPath()));
        if (!error.isEmpty())
        {
            QMessageBox::warning(this, "Add Device",
                QString("Failed to add device to database.\n\n%1").arg(error));
            return;
        }

        loadDevice();

        EventManager::getInstance().fireRefreshLibraryEvent(RefreshLibraryEvent());
    }
}

void SettingsDialog::showAddOscOutputDialog()
{
    OscOutputDialog* dialog = new OscOutputDialog(this);
    if (dialog->exec() == QDialog::Accepted)
    {
        DatabaseManager::getInstance().insertOscOutput(OscOutputModel(0, dialog->getName(), dialog->getAddress(),
                                                       dialog->getPort().toInt(), dialog->getDescription()));

        loadOscOutput();

        EventManager::getInstance().fireOscOutputChangedEvent(OscOutputChangedEvent());
    }
}

void SettingsDialog::removeDevice()
{
    if (this->treeWidgetDevice->selectedItems().count() == 0)
        return;

    DatabaseManager::getInstance().deleteDevice(this->treeWidgetDevice->currentItem()->text(0).toInt());
    delete this->treeWidgetDevice->currentItem();

    loadDevice();

    EventManager::getInstance().fireRefreshLibraryEvent(RefreshLibraryEvent());
}

void SettingsDialog::removeOscOutput()
{
    if (this->treeWidgetOscOutput->selectedItems().count() == 0)
        return;

    DatabaseManager::getInstance().deleteOscOutput(this->treeWidgetOscOutput->currentItem()->text(0).toInt());
    delete this->treeWidgetOscOutput->currentItem();

    loadOscOutput();

    EventManager::getInstance().fireOscOutputChangedEvent(OscOutputChangedEvent());
}

void SettingsDialog::deviceItemDoubleClicked(QTreeWidgetItem* current, int index)
{
    Q_UNUSED(index);

    DeviceModel model = DatabaseManager::getInstance().getDeviceById(current->text(0).toInt());

    DeviceDialog* dialog = new DeviceDialog(this);
    dialog->setDeviceModel(model);
    if (dialog->exec() == QDialog::Accepted)
    {
        DatabaseManager::getInstance().updateDevice(DeviceModel(model.getId(), dialog->getName(), dialog->getAddress(),
                                                                dialog->getPort().toInt(), dialog->getUsername(),
                                                                dialog->getPassword(), dialog->getDescription(),
                                                                model.getVersion(), dialog->getShadow(),
                                                                model.getChannels(), model.getChannelFormats(),
                                                                dialog->getPreviewChannel(), dialog->getLockedChannel(),
                                                                dialog->getTemplatePath(), dialog->getMediaPath(),
                                                                dialog->getServerPath()));

        loadDevice();

        EventManager::getInstance().fireRefreshLibraryEvent(RefreshLibraryEvent());
    }
}


void SettingsDialog::oscOutputItemDoubleClicked(QTreeWidgetItem* current, int index)
{
    Q_UNUSED(index);

    OscOutputModel model = DatabaseManager::getInstance().getOscOutputByAddress(current->text(3));

    OscOutputDialog* dialog = new OscOutputDialog(this);
    dialog->setDeviceModel(model);
    if (dialog->exec() == QDialog::Accepted)
    {
        DatabaseManager::getInstance().updateOscOutput(OscOutputModel(model.getId(), dialog->getName(), dialog->getAddress(),
                                                                      dialog->getPort().toInt(), dialog->getDescription()));

        loadOscOutput();

        EventManager::getInstance().fireOscOutputChangedEvent(OscOutputChangedEvent());
    }
}

void SettingsDialog::startFullscreenChanged(int state)
{
    QString isFullscreen = (state == Qt::Checked) ? "true" : "false";
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "StartFullscreen", isFullscreen));
}

void SettingsDialog::fontSizeChanged(int size)
{
    this->pendingFontSize = size;
    this->fontSizeDebounceTimer->start();
}

void SettingsDialog::flushPendingWrites()
{
    if (this->fontSizeDebounceTimer->isActive())
    {
        this->fontSizeDebounceTimer->stop();
        qApp->setStyleSheet(this->stylesheet + WidgetHeaderCSS::generate() +
                            QString(" QWidget { font-size: %1px; }").arg(this->pendingFontSize));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "FontSize", QString::number(this->pendingFontSize)));
    }

    if (this->sliderDbWriteTimer->isActive())
    {
        this->sliderDbWriteTimer->stop();
        for (auto it = this->pendingSliderDbWrites.constBegin();
             it != this->pendingSliderDbWrites.constEnd(); ++it)
        {
            DatabaseManager::getInstance().updateConfiguration(
                ConfigurationModel(0, it.key(), it.value()));
        }
        this->pendingSliderDbWrites.clear();
    }
}

void SettingsDialog::autoSynchronizeChanged(int state)
{
    QString isAutoSynchronize = (state == Qt::Checked) ? "true" : "false";
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "AutoRefreshLibrary", isAutoSynchronize));

    this->labelInterval->setEnabled((isAutoSynchronize == "true") ? true : false);
    this->spinBoxRefreshInterval->setEnabled((isAutoSynchronize == "true") ? true : false);
    this->labelSeconds->setEnabled((isAutoSynchronize == "true") ? true : false);

    EventManager::getInstance().fireAutoRefreshLibraryEvent(AutoRefreshLibraryEvent((isAutoSynchronize == "true") ? true : false,
                                                                                    this->spinBoxRefreshInterval->value() * 1000));
}

void SettingsDialog::showThumbnailTooltipChanged(int state)
{
    QString showThumbnailTooltip = (state == Qt::Checked) ? "true" : "false";
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "ShowThumbnailTooltip", showThumbnailTooltip));
    RundownWidgetHelper::invalidateConfigCache();
}

void SettingsDialog::reverseOscTimeChanged(int state)
{
    QString reverseOscTime = (state == Qt::Checked) ? "true" : "false";
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "ReverseOscTime", reverseOscTime));
    RundownWidgetHelper::invalidateConfigCache();
}

void SettingsDialog::enableOscInputControlChanged(int state)
{
    QString enableOscInputControl = (state == Qt::Checked) ? "true" : "false";
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "EnableOscInputControl", enableOscInputControl));

    this->labelOscInputControlPort->setEnabled((state == Qt::Checked) ? true : false);
    this->lineEditOscInputControlPort->setEnabled((state == Qt::Checked) ? true : false);
}

void SettingsDialog::enableOscInputMonitorChanged(int state)
{
    QString enableOscInputMonitor = (state == Qt::Checked) ? "true" : "false";
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "EnableOscInputMonitor", enableOscInputMonitor));

    this->labelOscInputMonitorPort->setEnabled((state == Qt::Checked) ? true : false);
    this->lineEditOscInputMonitorPort->setEnabled((state == Qt::Checked) ? true : false);
}

void SettingsDialog::enableOscInputWebSocketChanged(int state)
{
    QString enableOscInputWebSocket = (state == Qt::Checked) ? "true" : "false";
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "EnableOscInputWebSocket", enableOscInputWebSocket));

    this->labelOscInputWebSocketPort->setEnabled((state == Qt::Checked) ? true : false);
    this->lineEditOscInputWebSocketPort->setEnabled((state == Qt::Checked) ? true : false);
}

void SettingsDialog::disableInAndOutPointsChanged(int state)
{
    QString disableInAndOutPoints = (state == Qt::Checked) ? "true" : "false";
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "DisableInAndOutPoints", disableInAndOutPoints));
}

void SettingsDialog::synchronizeIntervalChanged(int interval)
{
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "RefreshLibraryInterval", QString("%1").arg(interval)));

    EventManager::getInstance().fireAutoRefreshLibraryEvent(AutoRefreshLibraryEvent(this->checkBoxAutoRefresh->checkState(), interval * 1000));
}

void SettingsDialog::updateGpi(int gpi, const QComboBox* voltage, const QComboBox* action)
{
    if (!isVisible()) // During construction of dialog we don't want to rewrite
        return;       // values in the database that are already there.

    bool risingEdge = voltage->currentIndex() == 0;
    Playout::PlayoutType playoutType = Playout::enumConstants().at(action->currentIndex());

    DatabaseManager::getInstance().updateGpiPort(GpiPortModel(gpi, risingEdge, playoutType));

    GpiManager::getInstance().getGpiDevice()->setupGpiPort(gpi, risingEdge);

    emit gpiBindingChanged(gpi, playoutType);
}

void SettingsDialog::gpi1Changed()
{
    updateGpi(0, comboBoxGpiVoltageChange1, comboBoxAction1);
}

void SettingsDialog::gpi2Changed()
{
    updateGpi(1, comboBoxGpiVoltageChange2, comboBoxAction2);
}

void SettingsDialog::gpi3Changed()
{
    updateGpi(2, comboBoxGpiVoltageChange3, comboBoxAction3);
}

void SettingsDialog::gpi4Changed()
{
    updateGpi(3, comboBoxGpiVoltageChange4, comboBoxAction4);
}

void SettingsDialog::gpi5Changed()
{
    updateGpi(4, comboBoxGpiVoltageChange5, comboBoxAction5);
}

void SettingsDialog::gpi6Changed()
{
    updateGpi(5, comboBoxGpiVoltageChange6, comboBoxAction6);
}

void SettingsDialog::gpi7Changed()
{
    updateGpi(6, comboBoxGpiVoltageChange7, comboBoxAction7);
}

void SettingsDialog::gpi8Changed()
{
    updateGpi(7, comboBoxGpiVoltageChange8, comboBoxAction8);
}

void SettingsDialog::updateGpo(int gpo, const QComboBox* voltage, const QSpinBox* pulseLength)
{
    if (!isVisible()) // During construction of dialog we don't want to rewrite
        return;       // values in the database that are already there.

    bool risingEdge = voltage->currentIndex() == 0;
    int pulseLengthMillis = pulseLength->value();

    qDebug() << "GPO " << gpo
             << " changed -- rising edge: " << risingEdge
             << " pulse length: " << pulseLengthMillis << "ms";

    DatabaseManager::getInstance().updateGpoPort(GpoPortModel(gpo, risingEdge, pulseLengthMillis));

    GpiManager::getInstance().getGpiDevice()->setupGpoPort(gpo, pulseLengthMillis, risingEdge);
}

void SettingsDialog::gpo1Changed()
{
    updateGpo(0, comboBoxGpoVoltageChange1, spinBoxPulseLength1);
}

void SettingsDialog::gpo2Changed()
{
    updateGpo(1, comboBoxGpoVoltageChange2, spinBoxPulseLength2);
}

void SettingsDialog::gpo3Changed()
{
    updateGpo(2, comboBoxGpoVoltageChange3, spinBoxPulseLength3);
}

void SettingsDialog::gpo4Changed()
{
    updateGpo(3, comboBoxGpoVoltageChange4, spinBoxPulseLength4);
}

void SettingsDialog::gpo5Changed()
{
    updateGpo(4, comboBoxGpoVoltageChange5, spinBoxPulseLength5);
}

void SettingsDialog::gpo6Changed()
{
    updateGpo(5, comboBoxGpoVoltageChange6, spinBoxPulseLength6);
}

void SettingsDialog::gpo7Changed()
{
    updateGpo(6, comboBoxGpoVoltageChange7, spinBoxPulseLength7);
}

void SettingsDialog::gpo8Changed()
{
    updateGpo(7, comboBoxGpoVoltageChange8, spinBoxPulseLength8);
}

void SettingsDialog::updateGpiDevice()
{
    if (!isVisible()) // During construction of dialog we don't want to rewrite
        return;       // values in the database that are already there.

    QString serialPort = this->lineEditSerialPort->text().trimmed();
    int baudRate = this->comboBoxGpiBaudRate->currentText().toInt();

    qDebug() << "GPO Device changed -- Serial port: "
             << serialPort << " Baud rate: " << baudRate;

    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "GpiSerialPort", serialPort));
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "GpiBaudRate", QString("%1").arg(baudRate)));

    GpiManager::getInstance().reinitialize();
}

void SettingsDialog::serialPortChanged()
{
    updateGpiDevice();
}

void SettingsDialog::baudRateChanged(QString baudRate)
{
    Q_UNUSED(baudRate);

    updateGpiDevice();
}

void SettingsDialog::oscMonitorPortChanged()
{
    QString oscMonitorPort = this->lineEditOscInputMonitorPort->text().trimmed();
    if (oscMonitorPort.isEmpty())
        oscMonitorPort = QString("%1").arg(Osc::DEFAULT_MONITOR_PORT);

    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "OscMonitorPort", oscMonitorPort));
}

void SettingsDialog::oscWebSocketPortChanged()
{
    QString oscWebSocketPort = this->lineEditOscInputWebSocketPort->text().trimmed();
    if (oscWebSocketPort.isEmpty())
        oscWebSocketPort = QString("%1").arg(Osc::DEFAULT_WEBSOCKET_PORT);

    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "OscWebSocketPort", oscWebSocketPort));
}

void SettingsDialog::oscControlPortChanged()
{
    QString oscControlPort = this->lineEditOscInputControlPort->text().trimmed();
    if (oscControlPort.isEmpty())
        oscControlPort = QString("%1").arg(Osc::DEFAULT_CONTROL_PORT);

    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "OscControlPort", oscControlPort));
}

void SettingsDialog::streamPortChanged()
{
    QString streamPort = this->lineEditStreamPort->text().trimmed();
    if (streamPort.isEmpty())
        streamPort = QString("%1").arg(Stream::DEFAULT_PORT);

    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "StreamPort", streamPort));
}

void SettingsDialog::repositoryPortChanged()
{
    QString repositoryPort = this->lineEditRepositoryPort->text().trimmed();
    if (repositoryPort.isEmpty())
        repositoryPort = QString("%1").arg(Repository::DEFAULT_PORT);

    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "RepositoryPort", repositoryPort));
}

void SettingsDialog::durationFormatChanged(QString text)
{
    Q_UNUSED(text);

    QString value = this->comboBoxDurationFormat->currentData().toString();
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "DurationFormat", value));
    EventManager::getInstance().fireUnitSettingsChangedEvent();
}

void SettingsDialog::logLevelChanged(int index)
{
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "LogLevel", this->comboBoxLogLevel->itemData(index).toString()));
}

void SettingsDialog::themeChanged(QString theme)
{
    Q_UNUSED(theme);

    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "Theme", this->comboBoxTheme->currentText()));
}

void SettingsDialog::rundownRepositoryChanged()
{
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "RundownRepository", this->lineEditRundownRepository->text()));
}

void SettingsDialog::storeThumbnailsInDatabaseChanged(int state)
{
    QString storeThumbnailsInDatabase = (state == Qt::Checked) ? "true" : "false";
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "StoreThumbnailsInDatabase", storeThumbnailsInDatabase));
}

void SettingsDialog::deleteThumbnails()
{
    EventManager::getInstance().fireStatusbarEvent(StatusbarEvent("Deleting thumbnails..."));

    DatabaseManager::getInstance().deleteThumbnails();

    EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(""));

    QMessageBox box(this);
    box.setWindowTitle("Database");
    box.setText(QString("Successfully deleted all thumbnails from the database."));
    box.setIconPixmap(QPixmap(":/Graphics/Images/Information.png"));
    box.setStandardButtons(QMessageBox::Ok);
    box.buttons().at(0)->setFocusPolicy(Qt::NoFocus);
    box.exec();
}

void SettingsDialog::markUsedItemsChanged(int state)
{
    QString markUsedItems = (state == Qt::Checked) ? "true" : "false";
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "MarkUsedItems", markUsedItems));
    RundownWidgetHelper::invalidateConfigCache();
}


void SettingsDialog::disableAudioInStreamChanged(int state)
{
    QString disableAudioInStream = (state == Qt::Checked) ? "true" : "false";
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "DisableAudioInStream", disableAudioInStream));
}

void SettingsDialog::networkCacheChanged(int value)
{
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "NetworkCache", QString("%1").arg(value)));
}

void SettingsDialog::streamQualityChanged(int quality)
{
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "StreamQuality", QString("%1").arg(100 - quality)));
}


void SettingsDialog::useFreezeOnLoadChanged(int state)
{
    QString useFreezeOnLoad = (state == Qt::Checked) ? "true" : "false";
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "UseFreezeOnLoad", useFreezeOnLoad));
    RundownWidgetHelper::invalidateConfigCache();
}

void SettingsDialog::useDropFrameNotationChanged(int state)
{
    QString useDropFrameNotation = (state == Qt::Checked) ? "true" : "false";
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "UseDropFrameNotation", useDropFrameNotation));
}

void SettingsDialog::setupHotkeyTab()
{
    QWidget* tabHotkeys = new QWidget();
    this->tabWidgetSettings->addTab(tabHotkeys, "Hotkeys");

    QVBoxLayout* mainLayout = new QVBoxLayout(tabHotkeys);

    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    QWidget* scrollContent = new QWidget();
    QGridLayout* grid = new QGridLayout(scrollContent);

    QLabel* headerAction = new QLabel("Action");
    QLabel* headerPrimary = new QLabel("Primary Hotkey");
    QLabel* headerAlt = new QLabel("Alternative Hotkey");
    headerAction->setStyleSheet("font-weight: bold;");
    headerPrimary->setStyleSheet("font-weight: bold;");
    headerAlt->setStyleSheet("font-weight: bold;");
    grid->addWidget(headerAction, 0, 0);
    grid->addWidget(headerPrimary, 0, 1);
    grid->addWidget(headerAlt, 0, 2);

    struct HotkeyRow {
        QString displayName;
        QString configSuffix;
    };

    QList<HotkeyRow> rows = {
        { "Stop",              "Stop" },
        { "Play",              "Play" },
        { "Play Now",          "PlayNow" },
        { "Load",              "Load" },
        { "Pause / Resume",    "PauseResume" },
        { "Next",              "Next" },
        { "Update",            "Update" },
        { "Invoke",            "Invoke" },
        { "Preview",           "Preview" },
        { "Clear",             "Clear" },
        { "Clear Video Layer", "ClearVideoLayer" },
        { "Clear Channel",     "ClearChannel" },
        { "Trigger Bank 1",    "Bank1" },
        { "Trigger Bank 2",    "Bank2" },
        { "Trigger Bank 3",    "Bank3" },
        { "Trigger Bank 4",    "Bank4" },
        { "Trigger Bank 5",    "Bank5" },
        { "Trigger Bank 6",    "Bank6" },
        { "Trigger Bank 7",    "Bank7" },
        { "Trigger Bank 8",    "Bank8" },
        { "Trigger Bank 9",    "Bank9" },
        { "Toggle Preview Mode", "TogglePreview" },
        { "Toggle Autostep Mode", "ToggleAutostep" },
    };

    int row = 1;
    for (const auto& hr : rows)
    {
        QLabel* label = new QLabel(hr.displayName);
        grid->addWidget(label, row, 0);

        QString primaryKey = QString("Hotkey%1").arg(hr.configSuffix);
        QKeySequenceEdit* primaryEdit = new QKeySequenceEdit();
        primaryEdit->setProperty("configKey", primaryKey);
        grid->addWidget(primaryEdit, row, 1);
        this->hotkeyEdits[primaryKey] = primaryEdit;
        QObject::connect(primaryEdit, SIGNAL(keySequenceChanged(const QKeySequence&)),
                         this, SLOT(hotkeyEditChanged(const QKeySequence&)));

        QString altKey = QString("Hotkey%1Alt").arg(hr.configSuffix);
        QKeySequenceEdit* altEdit = new QKeySequenceEdit();
        altEdit->setProperty("configKey", altKey);
        grid->addWidget(altEdit, row, 2);
        this->hotkeyEdits[altKey] = altEdit;
        QObject::connect(altEdit, SIGNAL(keySequenceChanged(const QKeySequence&)),
                         this, SLOT(hotkeyEditChanged(const QKeySequence&)));

        row++;
    }

    grid->setRowStretch(row, 1);
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);

    QPushButton* restoreButton = new QPushButton("Restore Defaults");
    QObject::connect(restoreButton, SIGNAL(clicked()), this, SLOT(restoreDefaultHotkeysClicked()));
    mainLayout->addWidget(restoreButton);

    // Preview modifier dropdown.
    QHBoxLayout* modifierLayout = new QHBoxLayout();
    QLabel* modifierLabel = new QLabel("Preview Modifier Key (hold):");
    modifierLabel->setStyleSheet("font-weight: bold;");
    this->previewModifierCombo = new QComboBox();
    this->previewModifierCombo->addItems({"Shift", "Ctrl", "Alt", "None"});
    QString currentModifier = DatabaseManager::getInstance()
        .getConfigurationByName("PreviewModifier").getValue();
    int idx = this->previewModifierCombo->findText(currentModifier);
    if (idx >= 0)
        this->previewModifierCombo->setCurrentIndex(idx);
    QObject::connect(this->previewModifierCombo, &QComboBox::currentTextChanged, [this](const QString& text) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "PreviewModifier", text));
        emit hotkeyChanged();
    });
    modifierLayout->addWidget(modifierLabel);
    modifierLayout->addWidget(this->previewModifierCombo);
    modifierLayout->addStretch();
    mainLayout->addLayout(modifierLayout);

    loadHotkeys();
}

void SettingsDialog::loadHotkeys()
{
    QMapIterator<QString, QKeySequenceEdit*> it(this->hotkeyEdits);
    while (it.hasNext())
    {
        it.next();
        QString value = DatabaseManager::getInstance()
            .getConfigurationByName(it.key()).getValue();
        it.value()->blockSignals(true);
        it.value()->setKeySequence(QKeySequence::fromString(value));
        it.value()->blockSignals(false);
    }
}

void SettingsDialog::hotkeyEditChanged(const QKeySequence& keySequence)
{
    QKeySequenceEdit* edit = qobject_cast<QKeySequenceEdit*>(sender());
    if (!edit)
        return;

    QString configKey = edit->property("configKey").toString();
    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, configKey, keySequence.toString()));

    emit hotkeyChanged();
}

void SettingsDialog::restoreDefaultHotkeysClicked()
{
    QMap<QString, QString> defaults;
    defaults["HotkeyStop"] = "F1";              defaults["HotkeyStopAlt"] = "";
    defaults["HotkeyPlay"] = "F2";              defaults["HotkeyPlayAlt"] = "";
    defaults["HotkeyPlayNow"] = "";              defaults["HotkeyPlayNowAlt"] = "";
    defaults["HotkeyLoad"] = "F3";              defaults["HotkeyLoadAlt"] = "";
    defaults["HotkeyPauseResume"] = "F4";        defaults["HotkeyPauseResumeAlt"] = "";
    defaults["HotkeyNext"] = "F5";              defaults["HotkeyNextAlt"] = "";
    defaults["HotkeyUpdate"] = "F6";            defaults["HotkeyUpdateAlt"] = "";
    defaults["HotkeyInvoke"] = "F7";            defaults["HotkeyInvokeAlt"] = "";
    defaults["HotkeyPreview"] = "F8";           defaults["HotkeyPreviewAlt"] = "";
    defaults["HotkeyClear"] = "F10";            defaults["HotkeyClearAlt"] = "";
    defaults["HotkeyClearVideoLayer"] = "F11";   defaults["HotkeyClearVideoLayerAlt"] = "";
    defaults["HotkeyClearChannel"] = "F12";      defaults["HotkeyClearChannelAlt"] = "";
    defaults["HotkeyBank1"] = "Ctrl+1";          defaults["HotkeyBank1Alt"] = "";
    defaults["HotkeyBank2"] = "Ctrl+2";          defaults["HotkeyBank2Alt"] = "";
    defaults["HotkeyBank3"] = "Ctrl+3";          defaults["HotkeyBank3Alt"] = "";
    defaults["HotkeyBank4"] = "Ctrl+4";          defaults["HotkeyBank4Alt"] = "";
    defaults["HotkeyBank5"] = "Ctrl+5";          defaults["HotkeyBank5Alt"] = "";
    defaults["HotkeyBank6"] = "Ctrl+6";          defaults["HotkeyBank6Alt"] = "";
    defaults["HotkeyBank7"] = "Ctrl+7";          defaults["HotkeyBank7Alt"] = "";
    defaults["HotkeyBank8"] = "Ctrl+8";          defaults["HotkeyBank8Alt"] = "";
    defaults["HotkeyBank9"] = "Ctrl+9";          defaults["HotkeyBank9Alt"] = "";
    defaults["HotkeyTogglePreview"] = "Ctrl+P";   defaults["HotkeyTogglePreviewAlt"] = "";
    defaults["HotkeyToggleAutostep"] = "Ctrl+Shift+N"; defaults["HotkeyToggleAutostepAlt"] = "";

    QMapIterator<QString, QString> it(defaults);
    while (it.hasNext())
    {
        it.next();
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, it.key(), it.value()));
    }

    // Reset preview modifier to default.
    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, "PreviewModifier", "Shift"));
    if (this->previewModifierCombo)
        this->previewModifierCombo->setCurrentText("Shift");

    // Reset preview freeze template to default.
    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, "PreviewFreezeTemplate", "false"));
    if (this->previewFreezeTemplateCheck)
        this->previewFreezeTemplateCheck->setChecked(false);

    loadHotkeys();
    emit hotkeyChanged();
}

void SettingsDialog::updateChannelPreview()
{
    for (int i = 0; i < 5; i++)
    {
        int ch = i + 1;
        QColor color = QColor::fromHslF(ChannelColor::hue(ch) / 360.0, ChannelColor::saturation(), ChannelColor::lightness());
        this->channelPreview[i]->setStyleSheet(
            QString("background-color: %1; color: white; font-size: 10px; font-weight: bold; border-radius: 3px;").arg(color.name()));
    }
}

void SettingsDialog::openColorPicker(QLabel* swatch, const QString& dbKey, bool alpha)
{
    QColorDialog dialog(this);
    if (alpha)
        dialog.setOption(QColorDialog::ShowAlphaChannel);

    QColor current = swatch->property("currentColor").value<QColor>();
    if (current.isValid())
        dialog.setCurrentColor(current);

    if (dialog.exec() == QDialog::Accepted)
    {
        QColor color = dialog.selectedColor();
        swatch->setStyleSheet(QString("background-color: %1; border: 1px solid rgba(80,80,80,200); border-radius: 3px;").arg(color.name(QColor::HexArgb)));
        swatch->setProperty("currentColor", color);
        QString colorStr = color.name(QColor::HexArgb);
        DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, dbKey, colorStr));

        // Push to ColorCache so hot paths never re-read the DB.
        static const QHash<QString, void(*)(const QString&)> cacheSetters = {
            {"PVWButtonColor",          ColorCache::setPvwButton},
            {"STEPButtonColor",         ColorCache::setStepButton},
            {"PreviewBorderColor",      ColorCache::setPreviewBorder},
            {"AutostepHighlightColor",  ColorCache::setAutostepHighlight},
            {"ActiveIndicatorColor",    ColorCache::setActiveIndicator},
            {"LibrarySectionLineColor", ColorCache::setLibrarySectionLine},
            {"ClockColor1",             ColorCache::setClockColor1},
            {"ClockColor2",             ColorCache::setClockColor2},
            {"ClockShadowColor",        ColorCache::setClockShadow},
            {"HeaderLineMaster",        ColorCache::setHeaderLineMaster},
            {"HeaderBlockMaster",       ColorCache::setHeaderBlockMaster},
            {"HeaderTextMaster",        ColorCache::setHeaderTextMaster},
            {"HeaderLineRundown",       ColorCache::setHeaderLineRundown},
            {"HeaderBlockRundown",      ColorCache::setHeaderBlockRundown},
            {"HeaderTextRundown",       ColorCache::setHeaderTextRundown},
        };
        auto it = cacheSetters.find(dbKey);
        if (it != cacheSetters.end())
            (*it)(colorStr);

        // Per-widget header overrides: HeaderLine_<Panel>, HeaderBlock_<Panel>, HeaderText_<Panel>
        if (dbKey.startsWith("HeaderLine_"))
            ColorCache::setLineOverride(dbKey.mid(11), colorStr);
        else if (dbKey.startsWith("HeaderBlock_"))
            ColorCache::setBlockOverride(dbKey.mid(12), colorStr);
        else if (dbKey.startsWith("HeaderText_"))
            ColorCache::setTextOverride(dbKey.mid(11), colorStr);

        // Live-update the CSS for any header-related color change.
        if (dbKey.startsWith("Header") || dbKey == "LibrarySectionLineColor")
            qApp->setStyleSheet(this->stylesheet + WidgetHeaderCSS::generate());
    }
}

bool SettingsDialog::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonRelease)
    {
        QLabel* swatch = qobject_cast<QLabel*>(obj);
        if (swatch && swatch->property("dbKey").isValid())
        {
            QString dbKey = swatch->property("dbKey").toString();
            bool alpha = swatch->property("alpha").toBool();
            openColorPicker(swatch, dbKey, alpha);
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

// Rebuilt rather than updated, because the set of projects changes when devices or
// template folders do and there is no state here worth preserving across that.
void SettingsDialog::buildSheetProjectsGroup()
{
    if (this->sheetProjectsBox == nullptr)
        return;

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(this->sheetProjectsBox->layout());
    if (layout == nullptr)
        return;

    while (QLayoutItem* item = layout->takeAt(0))
    {
        if (item->widget() != nullptr)
            item->widget()->deleteLater();

        delete item;
    }

    SheetsProjectRegistry::getInstance().discover();
    const QList<SheetsProject>& projects = SheetsProjectRegistry::getInstance().projects();

    if (projects.isEmpty())
    {
        QLabel* none = new QLabel("No project.js found under any device's template path.", this->sheetProjectsBox);
        none->setStyleSheet("color: rgba(140, 140, 140, 200);");
        layout->addWidget(none);
        return;
    }

    foreach (const SheetsProject& project, projects)
    {
        bool found = false;
        bool useLocal = SheetsProjectRegistry::readLocalFlag(project.folder, &found);

        QWidget* row = new QWidget(this->sheetProjectsBox);
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);

        QLabel* nameLabel = new QLabel(project.name, row);
        nameLabel->setMinimumWidth(90);
        nameLabel->setToolTip(project.folder);
        rowLayout->addWidget(nameLabel, 0);

        // Each project reads with its own key, and the per-minute budget belongs to
        // the key, so this is also what decides which sheets share a budget.
        QLineEdit* keyEdit = new QLineEdit(SheetsProjectRegistry::readApiKey(project.folder), row);
        keyEdit->setPlaceholderText("Google API key");
        keyEdit->setToolTip(QString(
            "The API key in %1/project.js. Written as you type, so a half-typed key is a "
            "half-typed key on disk. Templates pick it up the next time they load.").arg(project.folder));
        rowLayout->addWidget(keyEdit, 1);

        QString folder = project.folder;
        QString name = project.name;

        QObject::connect(keyEdit, &QLineEdit::textEdited, this, [this, keyEdit, folder, name](const QString& value) {
            QString error;
            if (SheetsProjectRegistry::getInstance().writeApiKey(folder, value.trimmed(), &error))
            {
                keyEdit->setStyleSheet(QString());
                return;
            }

            // The file would not take it, so the field says so rather than looking saved.
            keyEdit->setStyleSheet("border: 1px solid rgba(200, 90, 80, 220);");
            EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(error, 6000, true));
        });

        QCheckBox* checkBox = new QCheckBox("Use cache", row);
        checkBox->setChecked(useLocal);
        checkBox->setEnabled(found);
        checkBox->setToolTip(found
            ? QString("local = %1 \xe2\x80\x94 ticked, this project's templates read the cache at localhost").arg(useLocal ? "true" : "false")
            : QString("No 'local = true/false' in %1/project.js, so there is nothing to switch.").arg(folder));

        QObject::connect(checkBox, &QCheckBox::toggled, this, [this, checkBox, folder, name](bool checked) {
            QString error;
            if (SheetsProjectRegistry::getInstance().writeLocalFlag(folder, checked, &error))
            {
                checkBox->setToolTip(QString("local = %1").arg(checked ? "true" : "false"));
                EventManager::getInstance().fireStatusbarEvent(
                    StatusbarEvent(QString("%1: local = %2").arg(name, checked ? "true" : "false")));
                return;
            }

            QSignalBlocker blocker(checkBox);
            checkBox->setChecked(!checked);
            EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(error, 6000, true));
        });

        rowLayout->addWidget(checkBox, 0);
        layout->addWidget(row);
    }
}

// The button carries the count, so it has to be re-read whenever the folder changes
// or something is removed from it.
void SettingsDialog::refreshSheetCacheSize()
{
    if (this->buttonClearSheetCache == nullptr)
        return;

    qint64 bytes = 0;
    int count = SheetCacheServer::cachedSheetCount(&bytes);

    if (count == 0)
    {
        this->buttonClearSheetCache->setText("Clear cache  (empty)");
        this->buttonClearSheetCache->setEnabled(false);
        return;
    }

    QString size = (bytes >= 1024 * 1024)
        ? QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1)
        : QString("%1 KB").arg(qMax(qint64(1), bytes / 1024));

    this->buttonClearSheetCache->setText(QString("Clear cache  (%1 file%2, %3)")
        .arg(count).arg(count == 1 ? "" : "s").arg(size));
    this->buttonClearSheetCache->setEnabled(true);
}
