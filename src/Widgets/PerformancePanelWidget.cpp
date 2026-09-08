#include "PerformancePanelWidget.h"

#include "SheetDataResolver.h"

#include "Global.h"
#include "PanelHelper.h"

#include <QtCore/QScopeGuard>
#include <QtCore/QSet>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QToolButton>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#endif

#if defined(Q_OS_WIN)
static quint64 fileTimeToUint64(const FILETIME& ft)
{
    return (static_cast<quint64>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}
#endif

PerformancePanelWidget::PerformancePanelWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
    setupMenus();

    this->collapsed = PanelHelper::isPanelCollapsed("Performance");
    if (this->collapsed)
        this->expandCollapseAction->setText("Expand");

    this->setFixedHeight(Panel::DEFAULT_PERFORMANCE_HEIGHT);

    // Build stat grid inside contentWidget.
    this->gridLayout = new QGridLayout(this->contentWidget);
    this->gridLayout->setContentsMargins(8, 4, 8, 4);
    this->gridLayout->setHorizontalSpacing(8);
    this->gridLayout->setVerticalSpacing(2);

    QString headerStyle = "color: rgba(120, 120, 120, 180); font-size: 9px;";
    QString rowLabelStyle = headerStyle;

    // Row 0: column headers.
    QLabel* headerCpu = new QLabel("CPU", this->contentWidget);
    headerCpu->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    headerCpu->setStyleSheet(headerStyle);
    this->gridLayout->addWidget(headerCpu, 0, 1);

    QLabel* headerMem = new QLabel("MEM", this->contentWidget);
    headerMem->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    headerMem->setStyleSheet(headerStyle);
    this->gridLayout->addWidget(headerMem, 0, 2);

    // Helper to create a data row (row label + cpu cell + mem cell).
    auto createRow = [&](int row, const QString& name) -> QPair<QLabel*, QLabel*> {
        QLabel* nameLabel = new QLabel(name, this->contentWidget);
        nameLabel->setStyleSheet(rowLabelStyle);
        this->gridLayout->addWidget(nameLabel, row, 0);

        QLabel* cpuLabel = new QLabel("--", this->contentWidget);
        cpuLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        this->gridLayout->addWidget(cpuLabel, row, 1);

        QLabel* memLabel = new QLabel("--", this->contentWidget);
        memLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        this->gridLayout->addWidget(memLabel, row, 2);

        return {memLabel, cpuLabel};
    };

    auto clientPair = createRow(1, "Client");
    this->clientMemLabel = clientPair.first;
    this->clientCpuLabel = clientPair.second;

    auto serverPair = createRow(2, "Server");
    this->serverMemLabel = serverPair.first;
    this->serverCpuLabel = serverPair.second;

    auto systemPair = createRow(3, "System");
    this->systemMemLabel = systemPair.first;
    this->systemCpuLabel = systemPair.second;

    // Sheets strain. One meter per API key rather than one overall, because the
    // per-minute budget belongs to the key: two projects on separate keys each half
    // spent is not one budget fully spent. Hidden entirely until a sheet is touched.
    this->sheetsBox = new QWidget(this->contentWidget);
    this->sheetsBoxLayout = new QVBoxLayout(this->sheetsBox);
    this->sheetsBoxLayout->setContentsMargins(0, 2, 0, 0);
    this->sheetsBoxLayout->setSpacing(2);

    this->gridLayout->addWidget(this->sheetsBox, 4, 0, 1, 3);
    this->sheetsBox->setVisible(false);

    // Poll every 2 seconds.
    this->updateTimer = new QTimer(this);
    QObject::connect(this->updateTimer, &QTimer::timeout, this, &PerformancePanelWidget::updateStats);
    this->updateTimer->start(2000);

    // Prime the CPU delta counters with an initial reading.
    updateStats();
}

// ---- Collapse button ----

void PerformancePanelWidget::setupMenus()
{
    this->dropdownMenu = new QMenu(this);
    this->dropdownMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->dropdownMenu, "Performance", this);
    this->dropdownMenu->addSeparator();
    this->expandCollapseAction = this->dropdownMenu->addAction("Collapse", this, &PerformancePanelWidget::toggleExpandCollapse);

    this->menuButton = new QToolButton(this->tabWidgetPerformance);
    this->menuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->menuButton->setFixedSize(22, 22);
    this->menuButton->setMenu(this->dropdownMenu);
    this->menuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetPerformance->setCornerWidget(this->menuButton);
}

// ---- Stats collection ----

static QString thresholdColor(double percent)
{
    if (percent >= 75.0)
        return "rgba(239, 83, 80, 230)";    // red
    if (percent >= 50.0)
        return "rgba(255, 202, 40, 230)";   // yellow
    return "rgba(102, 187, 106, 230)";       // green
}

static void colorizeLabel(QLabel* label, double percent)
{
    label->setStyleSheet(QString("color: %1;").arg(thresholdColor(percent)));
}

// Rows are created once per key and reused. Building them on the fly every couple
// of seconds would make the panel flicker and lose whatever the mouse was over.
PerformancePanelWidget::SheetsKeyRow& PerformancePanelWidget::sheetsRowFor(const QString& keyId)
{
    if (this->sheetsKeyRows.contains(keyId))
        return this->sheetsKeyRows[keyId];

    SheetsKeyRow entry;
    entry.row = new QWidget(this->sheetsBox);

    QHBoxLayout* rowLayout = new QHBoxLayout(entry.row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(6);

    entry.nameLabel = new QLabel(entry.row);
    entry.nameLabel->setStyleSheet("font-size: 10px; color: rgba(160, 160, 160, 200);");
    entry.nameLabel->setMinimumWidth(52);
    rowLayout->addWidget(entry.nameLabel, 0);

    entry.meter = new QProgressBar(entry.row);
    entry.meter->setRange(0, SheetDataResolver::quotaLimit());
    entry.meter->setValue(0);
    entry.meter->setTextVisible(false);
    entry.meter->setFixedHeight(6);
    rowLayout->addWidget(entry.meter, 1);

    entry.valueLabel = new QLabel(entry.row);
    entry.valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rowLayout->addWidget(entry.valueLabel, 0);

    this->sheetsBoxLayout->addWidget(entry.row);
    this->sheetsKeyRows.insert(keyId, entry);

    return this->sheetsKeyRows[keyId];
}

void PerformancePanelWidget::updateSheetsStrain()
{
    if (this->sheetsBox == nullptr)
        return;

    QList<SheetsKeyUsage> perKey = SheetDataResolver::getInstance().quotaUsageByKey();
    int limit = SheetDataResolver::quotaLimit();

    // Nothing has touched a sheet and nothing ever has \xe2\x80\x94 keep the panel as it was.
    if (perKey.isEmpty() && this->sheetsKeyRows.isEmpty())
        return;

    this->sheetsBox->setVisible(true);

    QSet<QString> seen;
    foreach (const SheetsKeyUsage& usage, perKey)
    {
        seen.insert(usage.keyId);

        SheetsKeyRow& entry = sheetsRowFor(usage.keyId);
        int total = usage.total();

        // The project name is what an operator recognises; the digest is only a
        // fallback for a spreadsheet no discovered project claims.
        entry.nameLabel->setText(usage.project.isEmpty() ? usage.keyId.left(10) : usage.project);

        if (entry.meter->maximum() != limit)
            entry.meter->setRange(0, limit);
        entry.meter->setValue(qMin(total, limit));

        double percent = (limit > 0) ? (static_cast<double>(total) / static_cast<double>(limit) * 100.0) : 0.0;
        QString color = thresholdColor(percent);

        entry.meter->setStyleSheet(QString(
            "QProgressBar { background-color: rgba(60, 60, 60, 200); border: none; border-radius: 3px; }"
            "QProgressBar::chunk { background-color: %1; border-radius: 3px; }").arg(color));

        entry.valueLabel->setText(QString("%1/%2").arg(total).arg(limit));
        entry.valueLabel->setStyleSheet(QString("color: %1;").arg(color));

        QString tooltip = usage.project.isEmpty()
            ? QString("Sheet API reads in the last minute, for one key")
            : QString("Sheet API reads in the last minute \xe2\x80\x94 %1").arg(usage.project);

        tooltip += QString("\n%1 from this client, %2 from templates")
            .arg(usage.clientReads).arg(usage.templateReads);

        if (usage.externalReads > 0)
            tooltip += QString("\n%1 reported by other applications").arg(usage.externalReads);

        entry.row->setToolTip(tooltip);
        entry.row->setVisible(true);
    }

    // A key that has gone quiet keeps its row, showing zero. Removing it would make
    // the panel jump every time a project paused, and the row is the useful part.
    foreach (const QString& keyId, this->sheetsKeyRows.keys())
    {
        if (seen.contains(keyId))
            continue;

        SheetsKeyRow& entry = this->sheetsKeyRows[keyId];
        entry.meter->setValue(0);
        entry.valueLabel->setText(QString("0/%1").arg(limit));
        entry.valueLabel->setStyleSheet(QString("color: %1;").arg(thresholdColor(0.0)));
    }
}


void PerformancePanelWidget::updateStats()
{
    updateSheetsStrain();

#if defined(Q_OS_WIN)
    quint64 totalSys = 0;

    QString clientCpuText = "--";
    double clientCpuPercent = 0.0;
    double clientMemPercent = 0.0;
    double systemCpu = 0.0;
    double systemMemPercent = 0.0;
    DWORD systemMemLoad = 0;

    // --- Process CPU ---
    FILETIME createTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime))
    {
        quint64 kernel = fileTimeToUint64(kernelTime);
        quint64 user = fileTimeToUint64(userTime);

        if (prevProcessKernel != 0 || prevProcessUser != 0)
        {
            quint64 deltaKernel = kernel - prevProcessKernel;
            quint64 deltaUser = user - prevProcessUser;
            quint64 totalProcess = deltaKernel + deltaUser;

            FILETIME sysIdle, sysKernel, sysUser;
            if (GetSystemTimes(&sysIdle, &sysKernel, &sysUser))
            {
                quint64 sk = fileTimeToUint64(sysKernel);
                quint64 su = fileTimeToUint64(sysUser);
                quint64 si = fileTimeToUint64(sysIdle);

                quint64 deltaSysKernel = sk - prevSystemKernel;
                quint64 deltaSysUser = su - prevSystemUser;
                quint64 deltaSysIdle = si - prevSystemIdle;

                totalSys = deltaSysKernel + deltaSysUser;

                // Process CPU.
                if (totalSys > 0)
                    clientCpuPercent = static_cast<double>(totalProcess) / static_cast<double>(totalSys) * 100.0;
                clientCpuText = QString("%1%").arg(clientCpuPercent, 0, 'f', 1);

                // System CPU.
                if (totalSys > 0)
                    systemCpu = static_cast<double>(totalSys - deltaSysIdle) / static_cast<double>(totalSys) * 100.0;

                prevSystemKernel = sk;
                prevSystemUser = su;
                prevSystemIdle = si;
            }
        }
        else
        {
            // First call — prime system times.
            FILETIME sysIdle, sysKernel, sysUser;
            if (GetSystemTimes(&sysIdle, &sysKernel, &sysUser))
            {
                prevSystemKernel = fileTimeToUint64(sysKernel);
                prevSystemUser = fileTimeToUint64(sysUser);
                prevSystemIdle = fileTimeToUint64(sysIdle);
            }
        }

        prevProcessKernel = kernel;
        prevProcessUser = user;
    }

    // --- Process Memory ---
    SIZE_T processWorkingSet = 0;
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        processWorkingSet = pmc.WorkingSetSize;

    // --- System Memory ---
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem))
    {
        systemMemPercent = static_cast<double>(mem.dwMemoryLoad);
        systemMemLoad = mem.dwMemoryLoad;

        // Client memory colorization (percentage of total system memory).
        if (processWorkingSet > 0 && mem.ullTotalPhys > 0)
            clientMemPercent = static_cast<double>(processWorkingSet) / static_cast<double>(mem.ullTotalPhys) * 100.0;
    }

    // --- Client row ---
    colorizeLabel(this->clientMemLabel, clientMemPercent);
    this->clientMemLabel->setText(QString("%1%").arg(clientMemPercent, 0, 'f', 1));
    colorizeLabel(this->clientCpuLabel, clientCpuPercent);
    this->clientCpuLabel->setText(clientCpuText);

    // --- System row ---
    colorizeLabel(this->systemMemLabel, systemMemPercent);
    this->systemMemLabel->setText(QString("%1%").arg(systemMemLoad));
    colorizeLabel(this->systemCpuLabel, systemCpu);
    this->systemCpuLabel->setText(QString("%1%").arg(systemCpu, 0, 'f', 1));

    // --- CasparCG Server Discovery ---
    discoverServers(totalSys);

#else
    this->clientMemLabel->setText("N/A");
    this->clientCpuLabel->setText("N/A");
    this->systemMemLabel->setText("N/A");
    this->systemCpuLabel->setText("N/A");
#endif
}

// ---- CasparCG server discovery ----

void PerformancePanelWidget::discoverServers(quint64 totalSys)
{
#if defined(Q_OS_WIN)
    // Enumerate running processes and find CasparCG servers.
    QSet<quint64> activePids;
    DWORD myPid = GetCurrentProcessId();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);

        if (Process32FirstW(snapshot, &pe))
        {
            do
            {
                QString exeName = QString::fromWCharArray(pe.szExeFile).toLower();
                if (exeName.contains("casparcg") && !exeName.contains("client") && pe.th32ProcessID != myPid)
                    activePids.insert(pe.th32ProcessID);
            } while (Process32NextW(snapshot, &pe));
        }
        CloseHandle(snapshot);
    }

    // Remove entries for servers that are no longer running.
    QList<quint64> toRemove;
    for (auto it = this->serverEntries.begin(); it != this->serverEntries.end(); ++it)
    {
        if (!activePids.contains(it.key()))
            toRemove.append(it.key());
    }
    for (quint64 pid : toRemove)
        this->serverEntries.remove(pid);

    // Add new entries for newly discovered servers.
    for (quint64 pid : activePids)
    {
        if (this->serverEntries.contains(pid))
            continue;

        ServerEntry entry;
        entry.pid = pid;
        this->serverEntries.insert(pid, entry);
    }

    // Aggregate stats across all server processes.
    double totalCpu = 0.0;
    double totalMemMB = 0.0;
    bool hasCpuData = false;

    for (auto it = this->serverEntries.begin(); it != this->serverEntries.end(); ++it)
    {
        ServerEntry& entry = it.value();
        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, static_cast<DWORD>(entry.pid));
        if (!hProc)
            continue;
        auto cleanup = qScopeGuard([&]() { CloseHandle(hProc); });

        FILETIME createTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(hProc, &createTime, &exitTime, &kernelTime, &userTime))
        {
            quint64 kernel = fileTimeToUint64(kernelTime);
            quint64 user = fileTimeToUint64(userTime);

            if (entry.prevKernel != 0 || entry.prevUser != 0)
            {
                quint64 delta = (kernel - entry.prevKernel) + (user - entry.prevUser);
                if (totalSys > 0)
                    totalCpu += static_cast<double>(delta) / static_cast<double>(totalSys) * 100.0;
                hasCpuData = true;
            }

            entry.prevKernel = kernel;
            entry.prevUser = user;
        }

        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc)))
            totalMemMB += static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }

    // Update aggregated display.
    if (!this->serverEntries.isEmpty())
    {
        QString cpuText = hasCpuData ? QString("%1%").arg(totalCpu, 0, 'f', 1) : "--";

        // Server memory as percentage of total physical memory.
        double serverMemPercent = 0.0;
        MEMORYSTATUSEX mem;
        mem.dwLength = sizeof(mem);
        if (GlobalMemoryStatusEx(&mem) && mem.ullTotalPhys > 0 && totalMemMB > 0.0)
            serverMemPercent = (totalMemMB * 1024.0 * 1024.0) / static_cast<double>(mem.ullTotalPhys) * 100.0;
        QString memText = totalMemMB > 0.0 ? QString("%1%").arg(serverMemPercent, 0, 'f', 1) : "--";

        colorizeLabel(this->serverMemLabel, serverMemPercent);
        this->serverMemLabel->setText(memText);
        colorizeLabel(this->serverCpuLabel, totalCpu);
        this->serverCpuLabel->setText(cpuText);
    }
    else
    {
        this->serverMemLabel->setText("--");
        this->serverCpuLabel->setText("--");
    }

#endif
}

void PerformancePanelWidget::toggleExpandCollapse()
{
    this->collapsed = !this->collapsed;
    PanelHelper::setPanelCollapsed("Performance", this->collapsed);

    this->expandCollapseAction->setText(this->collapsed ? "Expand" : "Collapse");
    this->contentWidget->setVisible(!this->collapsed);

    if (this->collapsed)
        this->setFixedHeight(Panel::COMPACT_PERFORMANCE_HEIGHT);
    else
        this->setFixedHeight(Panel::DEFAULT_PERFORMANCE_HEIGHT);
}
