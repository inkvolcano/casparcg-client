#pragma once

#include "Shared.h"

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <functional>

class RundownTreeBaseWidget;

// Makes Inspector edits undoable, one step per field.
//
// Undo in the rundown works on snapshots of the whole rundown taken around each
// tree operation. An Inspector edit has no such operation around it: a field is
// changed a keystroke or a click at a time, from forty kinds of section. So this
// watches the Inspector panels instead:
//
//   - work on a field starts with a click on it, or with the first key that can
//     change it (InspectorUndoRules.h). The active rundown is captured then;
//   - it ends when work starts on another field, another item is selected, a
//     rundown operation starts, or Undo or Redo is asked for. If the rundown
//     changed in between, that is one undo step, "Edit <item>".
//
// Presses outside the Inspector do not end it, so a list's popup or a data
// dialog opened from a field still counts as that field. A gesture that changed
// nothing records nothing. The capture is taken fresh at the start of every
// gesture, so a change made some other way is never reverted by undoing an edit.
class WIDGETS_EXPORT InspectorUndo : public QObject
{
    Q_OBJECT

    public:
        InspectorUndo(const QList<QWidget*>& roots, std::function<RundownTreeBaseWidget*()> activeTree, QObject* parent);
        ~InspectorUndo();

        // Records the gesture in progress, if it changed anything.
        Q_SLOT void commit();

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        QList<QPointer<QWidget>> roots;
        std::function<RundownTreeBaseWidget*()> activeTree;

        QPointer<RundownTreeBaseWidget> tree;
        QPointer<QWidget> field;
        QString before;
        bool pending = false;

        QWidget* fieldOf(QObject* target) const;
        void begin(QWidget* field);
        void discard(RundownTreeBaseWidget* restored);
};
