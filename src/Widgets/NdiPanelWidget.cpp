#include "NdiPanelWidget.h"
#include "NdiViewerWidget.h"

#include "Global.h"
#include "PanelHelper.h"

#include "DatabaseManager.h"
#include "Models/ConfigurationModel.h"
#include "NdiManager.h"

#include <QtCore/QJsonArray>
#include <QtCore/QTimer>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtGui/QActionGroup>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QToolButton>

NdiPanelWidget::NdiPanelWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
    setupMenus();

    this->collapsed = PanelHelper::isPanelCollapsed("NDI");
    if (this->collapsed)
        this->expandCollapseAction->setText("Expand");

    // Prevent the panel from pushing its QSplitter column wider.
    this->setSizePolicy(QSizePolicy::Ignored, this->sizePolicy().verticalPolicy());

    this->setFixedHeight(Panel::DEFAULT_LIVE_HEIGHT);

    // Reapply height after the event loop has resolved the initial layout.
    QTimer::singleShot(0, this, [this]() {
        PanelHelper::applyExpandedHeight(this, "NDI", Panel::DEFAULT_LIVE_HEIGHT);
    });

    // Create the grid layout inside the container widget.
    this->gridLayout = new QGridLayout(this->gridContainer);
    this->gridLayout->setSpacing(2);
    this->gridLayout->setContentsMargins(0, 0, 0, 0);

    // Center the gridContainer within its parent tab layout.
    this->verticalLayoutTab->setAlignment(this->gridContainer, Qt::AlignCenter);

    // Try to initialize NDI.
    ndiAvailable = NdiManager::getInstance().initialize();
    if (!ndiAvailable)
    {
        errorLabel = new QLabel(this->gridContainer);
        errorLabel->setText(
            "<div style='color: #ccc; text-align: center; padding: 20px;'>"
            "<b>NDI Runtime not installed</b><br><br>"
            "Download the free NDI Runtime from:<br>"
            "<a href='https://ndi.video/for-developers/ndi-sdk/' style='color: #6af;'>"
            "ndi.video/for-developers/ndi-sdk/</a></div>");
        errorLabel->setTextFormat(Qt::RichText);
        errorLabel->setOpenExternalLinks(true);
        errorLabel->setAlignment(Qt::AlignCenter);
        errorLabel->setWordWrap(true);
        this->gridLayout->addWidget(errorLabel, 0, 0);
        return;
    }

    // Restore configuration and build the grid.
    restoreConfig();
    rebuildGrid();
}

NdiPanelWidget::~NdiPanelWidget()
{
}

void NdiPanelWidget::setupMenus()
{
    this->dropdownMenu = new QMenu(this);
    this->dropdownMenu->setObjectName("panelMenu");

    PanelHelper::addMoveActions(this->dropdownMenu, "NDI", this);
    this->dropdownMenu->addSeparator();

    // Outputs submenu (1-9).
    this->outputCountMenu = new QMenu("Outputs", this);
    QActionGroup* countGroup = new QActionGroup(this);
    for (int i = 1; i <= 9; i++)
    {
        QAction* a = this->outputCountMenu->addAction(QString::number(i));
        a->setCheckable(true);
        a->setData(i);
        countGroup->addAction(a);
        if (i == 1)
            a->setChecked(true);
    }
    QObject::connect(this->outputCountMenu, &QMenu::triggered, this, &NdiPanelWidget::outputCountSelected);
    this->dropdownMenu->addMenu(this->outputCountMenu);

    // Layout submenu (dynamically built).
    this->layoutMenu = new QMenu("Layout", this);
    QObject::connect(this->layoutMenu, &QMenu::aboutToShow, this, [this]() {
        this->layoutMenu->clear();
        QActionGroup* layoutGroup = new QActionGroup(this->layoutMenu);
        QList<QPair<int, int>> layouts = validLayoutsForCount(outputCount_);
        for (const auto& l : layouts)
        {
            QString label = QString("%1 x %2").arg(l.first).arg(l.second);
            QAction* a = this->layoutMenu->addAction(label);
            a->setCheckable(true);
            a->setData(QPoint(l.first, l.second));
            layoutGroup->addAction(a);
            if (l.first == gridCols && l.second == gridRows)
                a->setChecked(true);
        }
    });
    QObject::connect(this->layoutMenu, &QMenu::triggered, this, &NdiPanelWidget::layoutSelected);
    this->dropdownMenu->addMenu(this->layoutMenu);

    this->dropdownMenu->addSeparator();
    this->dropdownMenu->addAction("Mute All", this, &NdiPanelWidget::muteAll);
    this->dropdownMenu->addAction("Unmute All", this, &NdiPanelWidget::unmuteAll);

    // Quality submenu.
    this->dropdownMenu->addSeparator();
    this->qualityMenu = new QMenu("Quality", this);

    // Bandwidth submenu.
    QMenu* bandwidthMenu = new QMenu("Bandwidth", this);
    QObject::connect(bandwidthMenu, &QMenu::aboutToShow, this, [this, bandwidthMenu]() {
        bandwidthMenu->clear();
        QString current = DatabaseManager::getInstance().getConfigurationByName("NdiBandwidth").getValue();
        if (current.isEmpty()) current = "high";
        QActionGroup* group = new QActionGroup(bandwidthMenu);
        for (auto& pair : QList<QPair<QString, QString>>{{"High (Full Quality)", "high"}, {"Low (Reduced)", "low"}})
        {
            QAction* a = bandwidthMenu->addAction(pair.first);
            a->setCheckable(true);
            a->setData(pair.second);
            a->setChecked(pair.second == current);
            group->addAction(a);
        }
    });
    QObject::connect(bandwidthMenu, &QMenu::triggered, this, [this](QAction* action) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "NdiBandwidth", action->data().toString()));
        applyPerformanceSettings();
    });
    this->qualityMenu->addMenu(bandwidthMenu);

    // Frame Rate Limit submenu.
    QMenu* fpsMenu = new QMenu("Frame Rate Limit", this);
    QObject::connect(fpsMenu, &QMenu::aboutToShow, this, [this, fpsMenu]() {
        fpsMenu->clear();
        QString currentStr = DatabaseManager::getInstance().getConfigurationByName("NdiFpsLimit").getValue();
        int current = currentStr.isEmpty() ? 0 : currentStr.toInt();
        QActionGroup* group = new QActionGroup(fpsMenu);
        for (auto& pair : QList<QPair<QString, int>>{{"Source Rate (Off)", 0}, {"30 fps", 30}, {"15 fps", 15}, {"10 fps", 10}, {"5 fps", 5}})
        {
            QAction* a = fpsMenu->addAction(pair.first);
            a->setCheckable(true);
            a->setData(pair.second);
            a->setChecked(pair.second == current);
            group->addAction(a);
        }
    });
    QObject::connect(fpsMenu, &QMenu::triggered, this, [this](QAction* action) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "NdiFpsLimit", QString::number(action->data().toInt())));
        applyPerformanceSettings();
    });
    this->qualityMenu->addMenu(fpsMenu);

    // Scaling submenu.
    QMenu* scalingMenu = new QMenu("Scaling", this);
    QObject::connect(scalingMenu, &QMenu::aboutToShow, this, [this, scalingMenu]() {
        scalingMenu->clear();
        QString current = DatabaseManager::getInstance().getConfigurationByName("NdiScalingQuality").getValue();
        if (current.isEmpty()) current = "smooth";
        QActionGroup* group = new QActionGroup(scalingMenu);
        for (auto& pair : QList<QPair<QString, QString>>{{"Smooth (High Quality)", "smooth"}, {"Fast (Low CPU)", "fast"}})
        {
            QAction* a = scalingMenu->addAction(pair.first);
            a->setCheckable(true);
            a->setData(pair.second);
            a->setChecked(pair.second == current);
            group->addAction(a);
        }
    });
    QObject::connect(scalingMenu, &QMenu::triggered, this, [this](QAction* action) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "NdiScalingQuality", action->data().toString()));
        applyPerformanceSettings();
    });
    this->qualityMenu->addMenu(scalingMenu);

    this->dropdownMenu->addMenu(this->qualityMenu);

    this->dropdownMenu->addSeparator();
    this->expandCollapseAction = this->dropdownMenu->addAction("Collapse", this, &NdiPanelWidget::toggleExpandCollapse);

    // Corner widget: hamburger button.
    QToolButton* toolButton = new QToolButton(this->tabWidgetNdi);
    toolButton->setObjectName("toolButtonNdiDropdown");
    toolButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    toolButton->setFixedSize(22, 22);
    toolButton->setMenu(this->dropdownMenu);
    toolButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetNdi->setCornerWidget(toolButton);
}

void NdiPanelWidget::setOutputCount(int count)
{
    count = qBound(1, count, 9);
    if (count == outputCount_)
        return;

    outputCount_ = count;

    // Update the checked action in the Outputs menu.
    for (QAction* a : this->outputCountMenu->actions())
    {
        a->setChecked(a->data().toInt() == count);
    }

    // Pick a default layout for the new count.
    auto layout = defaultLayoutForCount(count);
    gridCols = layout.first;
    gridRows = layout.second;

    rebuildGrid();
    saveConfig();
}

int NdiPanelWidget::outputCount() const
{
    return outputCount_;
}

void NdiPanelWidget::rebuildGrid()
{
    if (!ndiAvailable)
        return;

    // Save current source assignments.
    QList<QPair<QString, bool>> savedAssignments;
    for (NdiViewerWidget* v : viewers)
        savedAssignments.append(qMakePair(v->sourceName(), v->isMuted()));

    // Remove existing viewers from grid.
    for (NdiViewerWidget* v : viewers)
    {
        this->gridLayout->removeWidget(v);
        v->deleteLater();
    }
    viewers.clear();

    // Create new viewers.
    for (int i = 0; i < outputCount_; i++)
    {
        NdiViewerWidget* viewer = new NdiViewerWidget(this->gridContainer);
        QObject::connect(viewer, &NdiViewerWidget::sourceChanged, this, &NdiPanelWidget::onViewerSourceChanged);
        viewers.append(viewer);
    }

    // Restore source assignments where possible.
    QList<NdiSourceInfo> currentSources = NdiManager::getInstance().getSources();
    for (int i = 0; i < qMin(savedAssignments.size(), viewers.size()); i++)
    {
        const QString& srcName = savedAssignments[i].first;
        bool muted = savedAssignments[i].second;

        if (!srcName.isEmpty())
        {
            for (const NdiSourceInfo& src : currentSources)
            {
                if (src.name == srcName)
                {
                    viewers[i]->connectToSource(src);
                    break;
                }
            }
        }
        viewers[i]->setMuted(muted);
    }

    // Place viewers in grid and apply current performance settings.
    applyLayout(gridCols, gridRows);
    applyPerformanceSettings();
}

void NdiPanelWidget::applyPerformanceSettings()
{
    QString bwStr = DatabaseManager::getInstance().getConfigurationByName("NdiBandwidth").getValue();
    NDIlib_recv_bandwidth_e bw = (bwStr == "low") ? NDIlib_recv_bandwidth_lowest : NDIlib_recv_bandwidth_highest;

    QString fpsStr = DatabaseManager::getInstance().getConfigurationByName("NdiFpsLimit").getValue();
    int fps = fpsStr.isEmpty() ? 0 : fpsStr.toInt();

    QString scaleStr = DatabaseManager::getInstance().getConfigurationByName("NdiScalingQuality").getValue();
    Qt::TransformationMode mode = (scaleStr == "fast") ? Qt::FastTransformation : Qt::SmoothTransformation;

    for (NdiViewerWidget* v : viewers)
    {
        v->setBandwidth(bw);
        v->setFpsLimit(fps);
        v->setScalingQuality(mode);
    }
}

void NdiPanelWidget::applyLayout(int cols, int rows)
{
    gridCols = cols;
    gridRows = rows;

    // Clear grid layout.
    while (gridLayout->count() > 0)
    {
        QLayoutItem* item = gridLayout->takeAt(0);
        // Don't delete widgets — just remove from layout.
        Q_UNUSED(item);
    }

    // Reset old stretch factors so phantom columns/rows don't consume space.
    for (int c = 0; c < gridLayout->columnCount(); c++)
        gridLayout->setColumnStretch(c, 0);
    for (int r = 0; r < gridLayout->rowCount(); r++)
        gridLayout->setRowStretch(r, 0);

    // Place viewers into grid cells.
    int idx = 0;
    for (int r = 0; r < rows && idx < viewers.size(); r++)
    {
        for (int c = 0; c < cols && idx < viewers.size(); c++)
        {
            gridLayout->addWidget(viewers[idx], r, c);
            viewers[idx]->show();
            idx++;
        }
    }

    // Set equal stretch for all active columns and rows.
    for (int c = 0; c < cols; c++)
        gridLayout->setColumnStretch(c, 1);
    for (int r = 0; r < rows; r++)
        gridLayout->setRowStretch(r, 1);

    // Defer so child layouts have their final sizes.
    QTimer::singleShot(0, this, &NdiPanelWidget::updateGridSize);
}

QPair<int, int> NdiPanelWidget::defaultLayoutForCount(int count)
{
    // Prefer square or near-square layouts.
    switch (count)
    {
    case 1: return {1, 1};
    case 2: return {2, 1};
    case 3: return {3, 1};
    case 4: return {2, 2};
    case 5: return {5, 1};
    case 6: return {3, 2};
    case 7: return {7, 1};
    case 8: return {4, 2};
    case 9: return {3, 3};
    default: return {1, 1};
    }
}

QList<QPair<int, int>> NdiPanelWidget::validLayoutsForCount(int count)
{
    QList<QPair<int, int>> result;
    for (int c = 1; c <= count; c++)
    {
        if (count % c == 0)
        {
            int r = count / c;
            result.append({c, r});
        }
    }
    return result;
}

void NdiPanelWidget::saveConfig()
{
    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, "NdiOutputCount", QString::number(outputCount_)));
    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, "NdiLayout", QString("%1x%2").arg(gridCols).arg(gridRows)));

    // Save per-viewer source assignments as JSON.
    QJsonArray arr;
    for (NdiViewerWidget* v : viewers)
    {
        QJsonObject obj;
        obj["source"] = v->sourceName();
        obj["muted"] = v->isMuted();
        arr.append(obj);
    }
    QString json = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, "NdiOutputConfig", json));
}

void NdiPanelWidget::restoreConfig()
{
    // Output count.
    QString countStr = DatabaseManager::getInstance().getConfigurationByName("NdiOutputCount").getValue();
    outputCount_ = countStr.isEmpty() ? 1 : qBound(1, countStr.toInt(), 9);

    // Update menu check state.
    for (QAction* a : this->outputCountMenu->actions())
        a->setChecked(a->data().toInt() == outputCount_);

    // Layout.
    QString layoutStr = DatabaseManager::getInstance().getConfigurationByName("NdiLayout").getValue();
    bool validLayout = false;
    if (!layoutStr.isEmpty() && layoutStr.contains('x'))
    {
        QStringList parts = layoutStr.split('x');
        int cols = parts[0].toInt();
        int rows = parts[1].toInt();
        if (cols > 0 && rows > 0)
        {
            gridCols = cols;
            gridRows = rows;
            validLayout = true;
        }
    }
    if (!validLayout)
    {
        auto def = defaultLayoutForCount(outputCount_);
        gridCols = def.first;
        gridRows = def.second;
    }
}

void NdiPanelWidget::onViewerSourceChanged()
{
    saveConfig();
}

// ---- Grid aspect ratio ----

void NdiPanelWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Defer so child layouts have their final sizes.
    QTimer::singleShot(0, this, &NdiPanelWidget::updateGridSize);
}

void NdiPanelWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // After rebuildLayout() reparents and re-shows this widget,
    // recalculate the grid to match the new container dimensions.
    QTimer::singleShot(0, this, &NdiPanelWidget::updateGridSize);
}

void NdiPanelWidget::updateGridSize()
{
    if (!ndiAvailable || viewers.isEmpty())
        return;

    // Get the available space inside the tab content area.
    QWidget* parent = this->gridContainer->parentWidget();
    if (!parent)
        return;

    int availW = parent->width();
    int availH = parent->height();
    if (availW <= 0 || availH <= 0)
        return;

    int spacing = gridLayout->spacing();
    double cellAspect = 16.0 / 9.0;

    // Maximum cell size from available space.
    double cellW = static_cast<double>(availW - spacing * (gridCols - 1)) / gridCols;
    double cellH = static_cast<double>(availH - spacing * (gridRows - 1)) / gridRows;

    // Constrain each cell to 16:9.
    if (cellW / cellH > cellAspect)
        cellW = cellH * cellAspect;     // too wide, shrink width
    else
        cellH = cellW / cellAspect;     // too tall, shrink height

    int gridW = static_cast<int>(cellW * gridCols + spacing * (gridCols - 1));
    int gridH = static_cast<int>(cellH * gridRows + spacing * (gridRows - 1));

    // Clamp grid width to never exceed available width (prevents pushing column wider).
    gridW = qMin(gridW, availW);
    this->gridContainer->setFixedSize(qMax(gridW, 1), qMax(gridH, 1));
}

// ---- Collapse / expand ----

void NdiPanelWidget::toggleExpandCollapse()
{
    this->collapsed = !this->collapsed;
    PanelHelper::setPanelCollapsed("NDI", this->collapsed);

    this->expandCollapseAction->setText(this->collapsed ? "Expand" : "Collapse");

    if (this->collapsed)
    {
        this->gridContainer->hide();
        this->setFixedHeight(Panel::COMPACT_LIVE_HEIGHT);
    }
    else
    {
        this->gridContainer->show();
        PanelHelper::applyExpandedHeight(this, "NDI", Panel::DEFAULT_LIVE_HEIGHT);
        QTimer::singleShot(0, this, &NdiPanelWidget::updateGridSize);
    }
}

// ---- Menu actions ----

void NdiPanelWidget::outputCountSelected(QAction* action)
{
    int count = action->data().toInt();
    setOutputCount(count);
}

void NdiPanelWidget::layoutSelected(QAction* action)
{
    QPoint p = action->data().toPoint();
    applyLayout(p.x(), p.y());
    saveConfig();
}

void NdiPanelWidget::muteAll()
{
    for (NdiViewerWidget* v : viewers)
        v->setMuted(true);
    saveConfig();
}

void NdiPanelWidget::unmuteAll()
{
    for (NdiViewerWidget* v : viewers)
        v->setMuted(false);
    saveConfig();
}
