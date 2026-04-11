#pragma once

#include "Shared.h"
#include "ui_PerformancePanelWidget.h"

#include <QtCore/QMap>
#include <QtCore/QTimer>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
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

    // Stat labels — individual cells per metric.
    QLabel* clientMemLabel = nullptr;
    QLabel* clientCpuLabel = nullptr;
    QLabel* serverMemLabel = nullptr;
    QLabel* serverCpuLabel = nullptr;
    QLabel* systemMemLabel = nullptr;
    QLabel* systemCpuLabel = nullptr;

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
