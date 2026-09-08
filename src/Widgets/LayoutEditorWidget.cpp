#include "LayoutEditorWidget.h"

#include "DatabaseManager.h"
#include "Models/ConfigurationModel.h"

#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

const QStringList LayoutEditorWidget::allWidgetIds = {
    "AudioLevels", "Preview", "Library", "Duration", "StatusBar",
    "Clock", "ServerStatus", "Activity", "TriggerBanks", "Live", "NDI", "Performance", "HttpLog", "Sheets", "Inspector", "SimpleInspector"
};

QString LayoutEditorWidget::widgetDisplayName(const QString& id)
{
    static const QMap<QString, QString> names = {
        {"AudioLevels", "Audio Levels"}, {"Preview", "Preview"},
        {"Library", "Library"}, {"Duration", "iNews"},
        {"StatusBar", "Status Bar"}, {"Clock", "Clock"},
        {"ServerStatus", "Server Status"}, {"Activity", "Activity"},
        {"TriggerBanks", "Trigger Banks"}, {"Live", "Live"},
        {"NDI", "NDI"}, {"Performance", "Performance"}, {"HttpLog", "Http Log"}, {"Sheets", "Google Sheets"}, {"Inspector", "Inspector"},
        {"SimpleInspector", "Simple Inspector"}
    };
    return names.value(id, id);
}

QString LayoutEditorWidget::columnDisplayName(const QString& id)
{
    static const QMap<QString, QString> names = {
        {"panel1", "Panel 1"}, {"panel2", "Panel 2"},
        {"panel3", "Panel 3"}, {"panel4", "Panel 4"},
        {"mainwindow", "Main Window"}
    };
    return names.value(id, id);
}

LayoutEditorWidget::LayoutEditorWidget(QWidget* parent, const QString& keyPrefix)
    : QWidget(parent), keyPrefix(keyPrefix), availableList(nullptr)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // --- Column order strip ---
    mainLayout->addWidget(new QLabel("Column Order (drag to reorder):", this));

    QHBoxLayout* columnOrderRow = new QHBoxLayout();

    this->columnOrderList = new QListWidget(this);
    this->columnOrderList->setFlow(QListView::LeftToRight);
    this->columnOrderList->setDragDropMode(QAbstractItemView::InternalMove);
    this->columnOrderList->setDefaultDropAction(Qt::MoveAction);
    this->columnOrderList->setFixedHeight(40);
    this->columnOrderList->setSpacing(4);
    // Rebuild panel group boxes when column order is reordered via drag.
    QObject::connect(this->columnOrderList->model(), &QAbstractItemModel::rowsMoved,
                     this, &LayoutEditorWidget::rebuildPanelLists);

    columnOrderRow->addWidget(this->columnOrderList, 1);

    mainLayout->addLayout(columnOrderRow);

    // --- Panel widget lists area ---
    this->panelListsLayout = new QHBoxLayout();
    mainLayout->addLayout(this->panelListsLayout, 1);

    // --- Bottom row ---
    QHBoxLayout* bottomRow = new QHBoxLayout();
    bottomRow->addWidget(new QLabel("Drag widgets between panels. Changes apply on restart.", this));
    bottomRow->addStretch();

    QPushButton* resetButton = new QPushButton("Reset Default", this);
    QObject::connect(resetButton, &QPushButton::clicked,
                     this, &LayoutEditorWidget::resetDefaults);
    bottomRow->addWidget(resetButton);

    mainLayout->addLayout(bottomRow);

    loadFromConfig();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QStringList LayoutEditorWidget::gatherColumnOrder()
{
    QStringList order;
    for (int i = 0; i < this->columnOrderList->count(); i++)
        order.append(this->columnOrderList->item(i)->data(Qt::UserRole).toString());
    return order;
}

QMap<QString, QStringList> LayoutEditorWidget::gatherPanelWidgets()
{
    QMap<QString, QStringList> result;
    for (auto it = this->panelLists.constBegin(); it != this->panelLists.constEnd(); ++it)
    {
        QStringList widgets;
        for (int i = 0; i < it.value()->count(); i++)
            widgets.append(it.value()->item(i)->data(Qt::UserRole).toString());
        result[it.key()] = widgets;
    }
    return result;
}

QListWidget* LayoutEditorWidget::createWidgetList()
{
    QListWidget* list = new QListWidget(this);
    list->setDragDropMode(QAbstractItemView::DragDrop);
    list->setDefaultDropAction(Qt::MoveAction);
    list->setDragEnabled(true);
    list->viewport()->setAcceptDrops(true);
    list->setDropIndicatorShown(true);
    return list;
}

// ---------------------------------------------------------------------------
// Load / Save
// ---------------------------------------------------------------------------

void LayoutEditorWidget::loadFromConfig()
{
    // Read column order.
    QString orderStr = DatabaseManager::getInstance()
        .getConfigurationByName(this->keyPrefix + "LayoutColumnOrder").getValue();
    if (orderStr.isEmpty())
        orderStr = this->keyPrefix.isEmpty() ? "panel1,mainwindow,panel2" : "mainwindow";
    QStringList columns = orderStr.split(",", Qt::SkipEmptyParts);

    // Ensure all 5 entries are always present.
    QStringList allColumns = {"panel1", "panel2", "panel3", "panel4", "mainwindow"};
    for (const QString& col : allColumns)
    {
        if (!columns.contains(col))
            columns.append(col);
    }

    // Populate column order list.
    this->columnOrderList->clear();
    for (const QString& col : columns)
    {
        QListWidgetItem* item = new QListWidgetItem(columnDisplayName(col));
        item->setData(Qt::UserRole, col);
        this->columnOrderList->addItem(item);
    }

    // Read panel widget assignments for all panels.
    QMap<QString, QString> panelDbKeys = {
        {"panel1", this->keyPrefix + "LayoutPanel1"}, {"panel2", this->keyPrefix + "LayoutPanel2"},
        {"panel3", this->keyPrefix + "LayoutPanel3"}, {"panel4", this->keyPrefix + "LayoutPanel4"}
    };

    QMap<QString, QStringList> panelWidgets;
    QSet<QString> usedWidgets;

    for (auto it = panelDbKeys.constBegin(); it != panelDbKeys.constEnd(); ++it)
    {
        QString widgetStr = DatabaseManager::getInstance()
            .getConfigurationByName(it.value()).getValue();

        QStringList widgets;
        for (const QString& w : widgetStr.split(",", Qt::SkipEmptyParts))
        {
            QString trimmed = w.trimmed();
            if (!trimmed.isEmpty())
            {
                widgets.append(trimmed);
                usedWidgets.insert(trimmed);
            }
        }
        panelWidgets[it.key()] = widgets;
    }

    // Clear existing panel lists.
    QLayoutItem* child;
    while ((child = this->panelListsLayout->takeAt(0)) != nullptr)
    {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }
    this->panelLists.clear();

    // Create a group box + list for each active panel.
    for (const QString& col : columns)
    {
        if (col == "mainwindow")
            continue;

        QGroupBox* group = new QGroupBox(columnDisplayName(col), this);
        QVBoxLayout* groupLayout = new QVBoxLayout(group);
        QListWidget* list = createWidgetList();
        groupLayout->addWidget(list);

        if (panelWidgets.contains(col))
        {
            for (const QString& wid : panelWidgets[col])
            {
                QListWidgetItem* item = new QListWidgetItem(widgetDisplayName(wid));
                item->setData(Qt::UserRole, wid);
                list->addItem(item);
            }
        }

        this->panelListsLayout->addWidget(group);
        this->panelLists[col] = list;
    }

    // Available (unassigned) widgets.
    QGroupBox* availGroup = new QGroupBox("Hidden", this);
    QVBoxLayout* availLayout = new QVBoxLayout(availGroup);
    this->availableList = createWidgetList();
    availLayout->addWidget(this->availableList);

    for (const QString& wid : allWidgetIds)
    {
        if (!usedWidgets.contains(wid))
        {
            QListWidgetItem* item = new QListWidgetItem(widgetDisplayName(wid));
            item->setData(Qt::UserRole, wid);
            this->availableList->addItem(item);
        }
    }
    this->panelListsLayout->addWidget(availGroup);
}

void LayoutEditorWidget::saveToConfig()
{
    // Column order — filter out empty panels so they don't appear at runtime.
    QStringList columns = gatherColumnOrder();
    QMap<QString, QStringList> panelWidgets = gatherPanelWidgets();

    QStringList filteredColumns;
    for (const QString& col : columns)
    {
        if (col == "mainwindow")
        {
            filteredColumns.append(col);
            continue;
        }
        if (panelWidgets.contains(col) && !panelWidgets[col].isEmpty())
            filteredColumns.append(col);
    }

    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, this->keyPrefix + "LayoutColumnOrder", filteredColumns.join(",")));

    // Panel widget assignments (save all panels, including empty ones).
    QMap<QString, QString> panelDbKeys = {
        {"panel1", this->keyPrefix + "LayoutPanel1"}, {"panel2", this->keyPrefix + "LayoutPanel2"},
        {"panel3", this->keyPrefix + "LayoutPanel3"}, {"panel4", this->keyPrefix + "LayoutPanel4"}
    };

    for (auto it = panelDbKeys.constBegin(); it != panelDbKeys.constEnd(); ++it)
    {
        QString value;
        if (panelWidgets.contains(it.key()))
            value = panelWidgets[it.key()].join(",");
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, it.value(), value));
    }
}

// ---------------------------------------------------------------------------
// Panel list rebuild (called when column order changes)
// ---------------------------------------------------------------------------

void LayoutEditorWidget::rebuildPanelLists()
{
    // Gather current widget assignments before destroying the lists.
    QMap<QString, QStringList> oldPanelWidgets = gatherPanelWidgets();
    QStringList availableWidgets;
    if (this->availableList)
    {
        for (int i = 0; i < this->availableList->count(); i++)
            availableWidgets.append(this->availableList->item(i)->data(Qt::UserRole).toString());
    }

    // Current column order (after reorder/add/remove).
    QStringList columns = gatherColumnOrder();
    QSet<QString> activeColumns(columns.begin(), columns.end());

    // Widgets from removed panels go to Available.
    for (auto it = oldPanelWidgets.begin(); it != oldPanelWidgets.end(); ++it)
    {
        if (!activeColumns.contains(it.key()))
            availableWidgets.append(it.value());
    }

    // Clear existing panel lists.
    QLayoutItem* child;
    while ((child = this->panelListsLayout->takeAt(0)) != nullptr)
    {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }
    this->panelLists.clear();

    // Recreate group boxes in the new column order.
    for (const QString& col : columns)
    {
        if (col == "mainwindow")
            continue;

        QGroupBox* group = new QGroupBox(columnDisplayName(col), this);
        QVBoxLayout* groupLayout = new QVBoxLayout(group);
        QListWidget* list = createWidgetList();
        groupLayout->addWidget(list);

        if (oldPanelWidgets.contains(col))
        {
            for (const QString& wid : oldPanelWidgets[col])
            {
                QListWidgetItem* item = new QListWidgetItem(widgetDisplayName(wid));
                item->setData(Qt::UserRole, wid);
                list->addItem(item);
            }
        }

        this->panelListsLayout->addWidget(group);
        this->panelLists[col] = list;
    }

    // Recreate available list.
    QGroupBox* availGroup = new QGroupBox("Hidden", this);
    QVBoxLayout* availLayout = new QVBoxLayout(availGroup);
    this->availableList = createWidgetList();
    availLayout->addWidget(this->availableList);

    for (const QString& wid : availableWidgets)
    {
        QListWidgetItem* item = new QListWidgetItem(widgetDisplayName(wid));
        item->setData(Qt::UserRole, wid);
        this->availableList->addItem(item);
    }
    this->panelListsLayout->addWidget(availGroup);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void LayoutEditorWidget::resetDefaults()
{
    // Reset column order — always show all panels.
    this->columnOrderList->clear();
    QStringList defaultOrder = {"panel1", "mainwindow", "panel2", "panel3", "panel4"};
    for (const QString& col : defaultOrder)
    {
        QListWidgetItem* item = new QListWidgetItem(columnDisplayName(col));
        item->setData(Qt::UserRole, col);
        this->columnOrderList->addItem(item);
    }

    // Clear and rebuild panel lists with defaults.
    QLayoutItem* child;
    while ((child = this->panelListsLayout->takeAt(0)) != nullptr)
    {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }
    this->panelLists.clear();

    QMap<QString, QStringList> defaultWidgets = {
        {"panel1", {"AudioLevels", "Preview", "Library", "Duration", "StatusBar"}},
        {"panel2", {"Clock", "ServerStatus", "Activity", "TriggerBanks", "NDI", "Performance", "HttpLog", "Inspector"}}
    };

    for (const QString& col : defaultOrder)
    {
        if (col == "mainwindow")
            continue;

        QGroupBox* group = new QGroupBox(columnDisplayName(col), this);
        QVBoxLayout* groupLayout = new QVBoxLayout(group);
        QListWidget* list = createWidgetList();
        groupLayout->addWidget(list);

        for (const QString& wid : defaultWidgets.value(col))
        {
            QListWidgetItem* item = new QListWidgetItem(widgetDisplayName(wid));
            item->setData(Qt::UserRole, wid);
            list->addItem(item);
        }

        this->panelListsLayout->addWidget(group);
        this->panelLists[col] = list;
    }

    // Empty available list.
    QGroupBox* availGroup = new QGroupBox("Hidden", this);
    QVBoxLayout* availLayout = new QVBoxLayout(availGroup);
    this->availableList = createWidgetList();
    availLayout->addWidget(this->availableList);
    this->panelListsLayout->addWidget(availGroup);
}

