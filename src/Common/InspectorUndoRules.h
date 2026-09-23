#pragma once

// Which key presses in an Inspector field can be an edit, for Inspector undo.
//
// An Inspector edit becomes one undo step per field: the rundown is captured when
// work on a field starts - a click, or the first key that can change it - and the
// step is recorded when work moves to another field, another item is selected,
// or Undo is pressed. Capturing costs a serialisation of the whole rundown, so
// keys that cannot edit a field must not start one: the playout function keys an
// operator presses all show long, Escape (the panic key), Tab, and bare modifiers.
//
// src/Widgets/InspectorUndo does the tracking. tools/test-inspectorundo checks this.

#include <QtCore/Qt>

namespace InspectorUndoRules
{
    inline bool keyCanEdit(int key)
    {
        if (key >= Qt::Key_F1 && key <= Qt::Key_F35)
            return false;

        switch (key)
        {
            case Qt::Key_Escape:
            case Qt::Key_Tab:
            case Qt::Key_Backtab:
            case Qt::Key_Shift:
            case Qt::Key_Control:
            case Qt::Key_Alt:
            case Qt::Key_Meta:
            case Qt::Key_AltGr:
            case Qt::Key_CapsLock:
            case Qt::Key_NumLock:
            case Qt::Key_unknown:
                return false;
            default:
                return true;
        }
    }
}
