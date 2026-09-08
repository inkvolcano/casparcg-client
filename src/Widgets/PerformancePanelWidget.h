#pragma once

#include "Shared.h"
#include "ui_PerformancePanelWidget.h"

#include <QtCore/QMap>
#include <QtCore/QTimer>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT PerformancePanelWidget : public QWidget, Ui::PerformancePanelWidget
{
    Q_OBJECT

public:
    explicit PerformancePanelWidget(QWidget* parent = nullptr);

private:
    void setupMenus();
    void updateStats();
    void discoverServers(quint64 totalSys);
    Q_SLOT void toggleExpandCollapse();

    void updateSheetsStrain();

    // Stat labels — individual cells per metric.
    QLabel* clientMemLabel = nullptr;
    QLabel* clientCpuLabel = nullptr;
    QLabel* serverMemLabel = nullptr;
    QLabel* serverCpuLabel = nullptr;
    QLabel* systemMemLabel = nullptr;
    QLabel* systemCpuLabel = nullptr;

    // Sheets strain: how much of each API key's per-minute budget is being spent.
    // One row per key, because that is what the budget belongs to — two projects on
    // separate keys, each half spent, are not one budget fully spent. A row is made
    // when its key is first seen and kept afterwards, so a key that falls quiet for a
    // moment does not make the panel jump.
    struct SheetsKeyRow
    {
        QWidget* row = nullptr;
        QLabel* nameLabel = nullptr;
        QProgressBar* meter = nullptr;
        QLabel* valueLabel = nullptr;
    };

    QWidget* sheetsBox = nullptr;
    QVBoxLayout* sheetsBoxLayout = nullptr;
    QMap<QString, SheetsKeyRow> sheetsKeyRows;

    SheetsKeyRow& sheetsRowFor(const QString& keyId);

    QGridLayout* gridLayout = nullptr;
    QTimer* updateTimer = nullptr;

    // Collapse state.
    bool collapsed = false;
    QToolButton* menuButton = nullptr;
    QMenu* dropdownMenu = nullptr;
    QAction* expandCollapseAction = nullptr;

    // CPU delta tracking.
    quint64 prevProcessKernel = 0;
    quint64 prevProcessUser = 0;
    quint64 prevSystemKernel = 0;
    quint64 prevSystemUser = 0;
    quint64 prevSystemIdle = 0;

    // CasparCG server discovery (aggregated).
    struct ServerEntry {
        quint64 pid = 0;
        quint64 prevKernel = 0;
        quint64 prevUser = 0;
    };

    QMap<quint64, ServerEntry> serverEntries;
};
