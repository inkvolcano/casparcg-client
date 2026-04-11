#pragma once

#include "Shared.h"
#include "ui_NdiPanelWidget.h"

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>

class NdiViewerWidget;

// Panel container that holds a grid of NdiViewerWidgets.
// Matches the tab-based pattern of LiveWidget/PreviewWidget with
// collapse/expand and a hamburger dropdown menu.
class WIDGETS_EXPORT NdiPanelWidget : public QWidget, Ui::NdiPanelWidget
{
    Q_OBJECT

public:
    explicit NdiPanelWidget(QWidget* parent = nullptr);
    ~NdiPanelWidget();

    // Called externally (e.g. from Settings) to change output count.
    void setOutputCount(int count);
    int outputCount() const;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    // Grid management.
    void rebuildGrid();
    void applyLayout(int cols, int rows);
    void saveConfig();
    void restoreConfig();
    QPair<int, int> defaultLayoutForCount(int count);
    QList<QPair<int, int>> validLayoutsForCount(int count);

    // Grid aspect ratio.
    void updateGridSize();

    // Menus.
    void setupMenus();

    Q_SLOT void toggleExpandCollapse();
    Q_SLOT void outputCountSelected(QAction* action);
    Q_SLOT void layoutSelected(QAction* action);
    Q_SLOT void muteAll();
    Q_SLOT void unmuteAll();
    Q_SLOT void onViewerSourceChanged();

    // Viewer grid.
    QGridLayout* gridLayout = nullptr;
    QList<NdiViewerWidget*> viewers;
    int outputCount_ = 1;
    int gridCols = 1;
    int gridRows = 1;

    // Collapse state.
    bool collapsed = false;

    // Performance settings.
    void applyPerformanceSettings();

    // Menus.
    QMenu* dropdownMenu = nullptr;
    QMenu* outputCountMenu = nullptr;
    QMenu* layoutMenu = nullptr;
    QMenu* qualityMenu = nullptr;
    QAction* expandCollapseAction = nullptr;

    // NDI availability.
    bool ndiAvailable = false;
    QLabel* errorLabel = nullptr;
};
