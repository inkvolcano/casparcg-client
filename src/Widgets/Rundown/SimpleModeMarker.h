#pragma once

#include "../Shared.h"

#include <QtCore/QObject>

QT_BEGIN_NAMESPACE
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;
QT_END_NAMESPACE

// Marks the rundown rows that have a key on the Simple Mode grid.
//
// The flag was invisible from the rundown side. To find out whether a row had a
// button you selected it and read the Inspector, one row at a time - which for
// the question people actually ask ("is this whole block on the grid?") means
// clicking through twenty rows to find the one that is not.
//
// Written from outside the rundown widgets, the same way MissingMediaScanner
// works and for the same reason: every one of the forty-odd item widgets already
// has a "frameItem" to hang a badge on, so no item type has to know this exists
// and a type added later gets it for free.
//
// It also listens. The flag is set from the Inspector while the rundown is on
// screen, so a sweep alone would be right only until the next tick of a checkbox;
// each command is connected on the way past so its own row updates itself.
class WIDGETS_EXPORT SimpleModeMarker : public QObject
{
    Q_OBJECT

    public:
        explicit SimpleModeMarker(QObject* parent = nullptr);

        // Walks the tree, marking rows that are on the grid and clearing the rest.
        void sweep(QTreeWidget* tree);

    private:
        void apply(QWidget* widget, bool onGrid);
        void walk(QTreeWidget* tree, QTreeWidgetItem* item);
};
