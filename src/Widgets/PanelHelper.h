#pragma once

#include "DatabaseManager.h"
#include "EventManager.h"
#include "Models/ConfigurationModel.h"

#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtWidgets/QMenu>
#include <QtWidgets/QWidget>

namespace PanelHelper
{

inline QString panelMode(const QString& panelId)
{
    QString mode = DatabaseManager::getInstance()
        .getConfigurationByName("PanelSizeMode_" + panelId).getValue();
    if (mode.isEmpty())
        return "fixed";
    return mode;
}

inline bool isPanelCollapsed(const QString& panelId)
{
    return DatabaseManager::getInstance()
        .getConfigurationByName("PanelCollapsed_" + panelId).getValue() == "true";
}

inline void setPanelCollapsed(const QString& panelId, bool collapsed)
{
    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, "PanelCollapsed_" + panelId, collapsed ? "true" : ""));

    // A spanned panel's height lives on its span cell in the layout grid — only
    // a rebuild can resize that row, so collapse/expand triggers one. Non-span
    // panels keep collapsing live without a rebuild.
    QString span = DatabaseManager::getInstance()
        .getConfigurationByName("PanelSpan_" + panelId).getValue();
    if (!span.isEmpty())
        EventManager::getInstance().fireRebuildLayout();
}

inline void applyExpandedHeight(QWidget* panel, const QString& panelId, int defaultHeight)
{
    QString mode = panelMode(panelId);

    if (mode == "expanding")
    {
        panel->setMinimumHeight(0);
        panel->setMaximumHeight(QWIDGETSIZE_MAX);
        QSizePolicy sp = panel->sizePolicy();
        sp.setVerticalPolicy(QSizePolicy::Preferred);
        panel->setSizePolicy(sp);
    }
    else if (mode == "resizable")
    {
        QString hStr = DatabaseManager::getInstance()
            .getConfigurationByName(panelId + "PanelHeight").getValue();
        int h = hStr.isEmpty() ? defaultHeight : hStr.toInt();
        panel->setFixedHeight(h);
    }
    else
    {
        panel->setFixedHeight(defaultHeight);
    }
}

inline QMenu* createSizeModeMenu(const QString& panelId, QWidget* parent)
{
    QMenu* menu = new QMenu("Size Mode", parent);
    QActionGroup* group = new QActionGroup(menu);
    group->setExclusive(true);

    auto addMode = [&](const QString& label, const QString& value) -> QAction* {
        QAction* action = menu->addAction(label);
        action->setCheckable(true);
        action->setData(value);
        group->addAction(action);
        return action;
    };

    addMode("Fixed", "fixed");
    addMode("Resizable", "resizable");
    addMode("Expanding", "expanding");

    // Check current mode.
    QString current = panelMode(panelId);
    for (QAction* a : group->actions())
        a->setChecked(a->data().toString() == current);

    // On change: save to DB and trigger layout rebuild.
    QObject::connect(group, &QActionGroup::triggered, parent, [panelId](QAction* action) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "PanelSizeMode_" + panelId, action->data().toString()));
        EventManager::getInstance().fireRebuildLayout();
    });

    return menu;
}

inline QString panelAnchor(const QString& panelId)
{
    QString anchor = DatabaseManager::getInstance()
        .getConfigurationByName("PanelAnchor_" + panelId).getValue();
    if (anchor.isEmpty())
        return "up";
    return anchor;
}

inline QMenu* createAnchorMenu(const QString& panelId, QWidget* parent)
{
    QMenu* menu = new QMenu("Anchor", parent);
    QActionGroup* group = new QActionGroup(menu);
    group->setExclusive(true);

    auto addOption = [&](const QString& label, const QString& value) -> QAction* {
        QAction* action = menu->addAction(label);
        action->setCheckable(true);
        action->setData(value);
        group->addAction(action);
        return action;
    };

    addOption("Up", "up");
    addOption("Down", "down");

    // Check current anchor.
    QString current = panelAnchor(panelId);
    for (QAction* a : group->actions())
        a->setChecked(a->data().toString() == current);

    // On change: save to DB and trigger layout rebuild.
    QObject::connect(group, &QActionGroup::triggered, parent, [panelId](QAction* action) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "PanelAnchor_" + panelId, action->data().toString()));
        EventManager::getInstance().fireRebuildLayout();
    });

    return menu;
}

// ── Layout helpers for move / span ──────────────────────────

inline QStringList columnOrder()
{
    QString orderStr = DatabaseManager::getInstance()
        .getConfigurationByName("LayoutColumnOrder").getValue();
    if (orderStr.isEmpty())
        orderStr = "panel1,mainwindow,panel2";
    return orderStr.split(",", Qt::SkipEmptyParts);
}

inline QString columnDbKey(const QString& col)
{
    if (col == "panel1") return "LayoutPanel1";
    if (col == "panel2") return "LayoutPanel2";
    if (col == "panel3") return "LayoutPanel3";
    if (col == "panel4") return "LayoutPanel4";
    return QString();
}

inline QStringList columnWidgets(const QString& col)
{
    QString key = columnDbKey(col);
    if (key.isEmpty()) return {};
    QString val = DatabaseManager::getInstance().getConfigurationByName(key).getValue();
    return val.split(",", Qt::SkipEmptyParts);
}

// Returns (columnId, index) for a panel, or ("", -1) if not found.
inline QPair<QString, int> findPanelColumn(const QString& panelId)
{
    QStringList cols = columnOrder();
    for (const QString& col : cols)
    {
        if (col == "mainwindow") continue;
        QStringList widgets = columnWidgets(col);
        int idx = widgets.indexOf(panelId);
        if (idx >= 0)
            return {col, idx};
    }
    return {QString(), -1};
}

// Find the next side panel in the given direction (-1 left, +1 right), skipping mainwindow.
inline QString adjacentSidePanel(const QString& currentCol, int direction)
{
    QStringList cols = columnOrder();
    int idx = cols.indexOf(currentCol);
    if (idx < 0) return QString();

    for (int i = idx + direction; i >= 0 && i < cols.size(); i += direction)
    {
        if (cols[i] != "mainwindow" && !columnDbKey(cols[i]).isEmpty())
            return cols[i];
    }
    return QString();
}

// Find the directly adjacent column (no skipping) — needed for span check.
inline QString directlyAdjacentSidePanel(const QString& currentCol, int direction)
{
    QStringList cols = columnOrder();
    int idx = cols.indexOf(currentCol);
    if (idx < 0) return QString();

    int next = idx + direction;
    if (next >= 0 && next < cols.size() && cols[next] != "mainwindow"
        && !columnDbKey(cols[next]).isEmpty())
        return cols[next];
    return QString();
}

// ── Span helpers ────────────────────────────────────────────

// Returns the stored span count (0 = no span). Backward compat: "right" → 1.
inline int panelSpanCount(const QString& panelId)
{
    QString val = DatabaseManager::getInstance()
        .getConfigurationByName("PanelSpan_" + panelId).getValue();
    if (val.isEmpty())
        return 0;
    if (val == "right")
        return 1; // backward compat
    bool ok = false;
    int count = val.toInt(&ok);
    return (ok && count > 0) ? count : 0;
}

inline bool panelHasSpan(const QString& panelId)
{
    return panelSpanCount(panelId) > 0;
}

inline QString panelSpanDirection(const QString& panelId)
{
    QString val = DatabaseManager::getInstance()
        .getConfigurationByName("PanelSpanDir_" + panelId).getValue();
    if (val == "left") return "left";
    return "right";
}

// Count of contiguous side panels in one direction only (not crossing mainwindow).
inline int maxSpanInDirection(const QString& col, const QString& direction)
{
    QStringList cols = columnOrder();
    int idx = cols.indexOf(col);
    if (idx < 0) return 0;

    int count = 0;
    if (direction == "left")
    {
        for (int i = idx - 1; i >= 0; i--)
        {
            if (cols[i] == "mainwindow" || columnDbKey(cols[i]).isEmpty())
                break;
            count++;
        }
    }
    else
    {
        for (int i = idx + 1; i < cols.size(); i++)
        {
            if (cols[i] == "mainwindow" || columnDbKey(cols[i]).isEmpty())
                break;
            count++;
        }
    }
    return count;
}

// Max span available from a column: count of contiguous side panels
// in the same group (not crossing mainwindow) minus 1.
inline int maxSpanFromColumn(const QString& col)
{
    QStringList cols = columnOrder();
    int idx = cols.indexOf(col);
    if (idx < 0) return 0;

    int groupSize = 1;
    // Count contiguous side panels to the right.
    for (int i = idx + 1; i < cols.size(); i++)
    {
        if (cols[i] == "mainwindow" || columnDbKey(cols[i]).isEmpty())
            break;
        groupSize++;
    }
    // Count contiguous side panels to the left.
    for (int i = idx - 1; i >= 0; i--)
    {
        if (cols[i] == "mainwindow" || columnDbKey(cols[i]).isEmpty())
            break;
        groupSize++;
    }
    return groupSize - 1; // own column doesn't count as "span"
}

// Returns ordered list of column names covered by a span from col with given count.
// Direction "right" expands right first then left; "left" expands left first then right.
// Never crosses mainwindow.
inline QStringList resolveSpanColumns(const QString& col, int spanCount,
                                      const QString& direction = "right")
{
    QStringList cols = columnOrder();
    int idx = cols.indexOf(col);
    if (idx < 0 || spanCount <= 0)
        return {col};

    QList<int> group;
    group.append(idx);

    int added = 0;

    if (direction == "left")
    {
        // Expand left first.
        for (int i = idx - 1; i >= 0 && added < spanCount; i--)
        {
            if (cols[i] == "mainwindow" || columnDbKey(cols[i]).isEmpty())
                break;
            group.prepend(i);
            added++;
        }

        // Expand right if more needed.
        for (int i = idx + 1; i < cols.size() && added < spanCount; i++)
        {
            if (cols[i] == "mainwindow" || columnDbKey(cols[i]).isEmpty())
                break;
            group.append(i);
            added++;
        }
    }
    else
    {
        // Expand right first.
        for (int i = idx + 1; i < cols.size() && added < spanCount; i++)
        {
            if (cols[i] == "mainwindow" || columnDbKey(cols[i]).isEmpty())
                break;
            group.append(i);
            added++;
        }

        // Expand left if more needed.
        for (int i = idx - 1; i >= 0 && added < spanCount; i--)
        {
            if (cols[i] == "mainwindow" || columnDbKey(cols[i]).isEmpty())
                break;
            group.prepend(i);
            added++;
        }
    }

    QStringList result;
    for (int ci : group)
        result.append(cols[ci]);
    return result;
}

// ── Consolidated panel menu ─────────────────────────────────

inline void addMoveActions(QMenu* menu, const QString& panelId, QWidget* parent)
{
    // ── Size Mode + Anchor ──
    menu->addMenu(createSizeModeMenu(panelId, parent));
    menu->addMenu(createAnchorMenu(panelId, parent));

    // ── Move submenu ──
    QMenu* moveMenu = menu->addMenu("Move");
    QAction* moveUp    = moveMenu->addAction("Up");
    QAction* moveDown  = moveMenu->addAction("Down");
    moveMenu->addSeparator();
    QAction* moveLeft  = moveMenu->addAction("Left");
    QAction* moveRight = moveMenu->addAction("Right");

    // ── Span actions ──
    menu->addSeparator();
    QAction* spanLeftAction  = menu->addAction("Span Left");
    QAction* spanRightAction = menu->addAction("Span Right");
    QAction* spanLessAction  = menu->addAction("Unspan");
    QAction* spanMoreAction  = menu->addAction("Span More");

    // Dynamically update when menu opens.
    QObject::connect(menu, &QMenu::aboutToShow, parent, [=]() {
        QPair<QString, int> pos = findPanelColumn(panelId);
        QString col = pos.first;
        int idx = pos.second;
        QStringList widgets = columnWidgets(col);
        int spanCount = panelSpanCount(panelId);
        bool hasSpan = spanCount > 0;

        moveUp->setEnabled(!col.isEmpty() && idx > 0);
        moveDown->setEnabled(!col.isEmpty() && idx < widgets.size() - 1);

        QString leftCol  = adjacentSidePanel(col, -1);
        QString rightCol = adjacentSidePanel(col, +1);
        moveLeft->setEnabled(!col.isEmpty() && !leftCol.isEmpty() && !hasSpan);
        moveRight->setEnabled(!col.isEmpty() && !rightCol.isEmpty() && !hasSpan);

        // Span Left/Right: show when not spanning and neighbor exists.
        bool hasLeft  = !col.isEmpty() && !directlyAdjacentSidePanel(col, -1).isEmpty();
        bool hasRight = !col.isEmpty() && !directlyAdjacentSidePanel(col, +1).isEmpty();
        spanLeftAction->setVisible(!hasSpan && hasLeft);
        spanRightAction->setVisible(!hasSpan && hasRight);

        // Unspan / Span Less: show when spanning.
        spanLessAction->setVisible(hasSpan);
        spanLessAction->setText(spanCount == 1 ? "Unspan" : "Span Less");

        // Span More: show when spanning and can expand further in current direction.
        QString dir = panelSpanDirection(panelId);
        int maxDir = col.isEmpty() ? 0 : maxSpanInDirection(col, dir);
        spanMoreAction->setVisible(hasSpan && spanCount < maxDir);
    });

    // Move Up / Down: swap with neighbor in same column.
    QObject::connect(moveUp, &QAction::triggered, parent, [panelId]() {
        QPair<QString, int> pos = findPanelColumn(panelId);
        if (pos.first.isEmpty() || pos.second <= 0) return;
        QStringList widgets = columnWidgets(pos.first);
        widgets.swapItemsAt(pos.second, pos.second - 1);
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, columnDbKey(pos.first), widgets.join(",")));
        EventManager::getInstance().fireRebuildLayout();
    });

    QObject::connect(moveDown, &QAction::triggered, parent, [panelId]() {
        QPair<QString, int> pos = findPanelColumn(panelId);
        if (pos.first.isEmpty() || pos.second < 0) return;
        QStringList widgets = columnWidgets(pos.first);
        if (pos.second >= widgets.size() - 1) return;
        widgets.swapItemsAt(pos.second, pos.second + 1);
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, columnDbKey(pos.first), widgets.join(",")));
        EventManager::getInstance().fireRebuildLayout();
    });

    // Move Left / Right: transfer widget to adjacent side panel.
    auto doMoveH = [](const QString& pid, int direction) {
        QPair<QString, int> pos = findPanelColumn(pid);
        if (pos.first.isEmpty()) return;
        QString targetCol = adjacentSidePanel(pos.first, direction);
        if (targetCol.isEmpty()) return;

        // Remove from source.
        QStringList src = columnWidgets(pos.first);
        src.removeAt(pos.second);
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, columnDbKey(pos.first), src.join(",")));

        // Append to target.
        QStringList dst = columnWidgets(targetCol);
        dst.append(pid);
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, columnDbKey(targetCol), dst.join(",")));

        EventManager::getInstance().fireRebuildLayout();
    };

    QObject::connect(moveLeft, &QAction::triggered, parent,
        [panelId, doMoveH]() { doMoveH(panelId, -1); });
    QObject::connect(moveRight, &QAction::triggered, parent,
        [panelId, doMoveH]() { doMoveH(panelId, +1); });

    // Span Left: start spanning left (set count to 1, direction to left).
    QObject::connect(spanLeftAction, &QAction::triggered, parent, [panelId]() {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "PanelSpanDir_" + panelId, "left"));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "PanelSpan_" + panelId, "1"));
        EventManager::getInstance().fireRebuildLayout();
    });

    // Span Right: start spanning right (set count to 1, direction to right).
    QObject::connect(spanRightAction, &QAction::triggered, parent, [panelId]() {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "PanelSpanDir_" + panelId, "right"));
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "PanelSpan_" + panelId, "1"));
        EventManager::getInstance().fireRebuildLayout();
    });

    // Span More: increment span count.
    QObject::connect(spanMoreAction, &QAction::triggered, parent, [panelId]() {
        int current = panelSpanCount(panelId);
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "PanelSpan_" + panelId, QString::number(current + 1)));
        EventManager::getInstance().fireRebuildLayout();
    });

    // Unspan / Span Less: decrement span count. Clear direction when fully unspanned.
    QObject::connect(spanLessAction, &QAction::triggered, parent, [panelId]() {
        int current = panelSpanCount(panelId);
        int next = current - 1;
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "PanelSpan_" + panelId, next > 0 ? QString::number(next) : ""));
        if (next <= 0)
            DatabaseManager::getInstance().updateConfiguration(
                ConfigurationModel(0, "PanelSpanDir_" + panelId, ""));
        EventManager::getInstance().fireRebuildLayout();
    });
}

} // namespace PanelHelper
