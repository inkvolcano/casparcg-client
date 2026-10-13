#pragma once

#include "../Shared.h"

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

QT_BEGIN_NAMESPACE
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;
QT_END_NAMESPACE

// Shows a rundown row's reference image when the row is hovered, and puts a small
// picture mark on rows that have one, so it is clear which are worth hovering.
//
// Written from outside the item widgets, like SimpleModeMarker and
// MissingMediaScanner: every item widget has a "frameItem" to hang a mark and a
// tooltip on, so no item type has to know about this.
//
// The rows are swept when the rundown opens, after a paste or drop, after an undo
// rebuilds the rundown, and shortly after any item's picture changes - "shortly"
// so a linked clone, which takes the picture a moment later, is swept too.
class WIDGETS_EXPORT ReferenceImageMarker : public QObject
{
    Q_OBJECT

    public:
        explicit ReferenceImageMarker(QTreeWidget* tree, QObject* parent = nullptr);

        Q_SLOT void queueSweep();

    private:
        QPointer<QTreeWidget> tree;
        QTimer sweepTimer;

        void sweep();
        void walk(QTreeWidgetItem* item);
        void apply(QWidget* widget, const QString& dataUri);
};
