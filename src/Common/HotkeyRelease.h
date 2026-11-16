#pragma once

// Playout hotkeys that fire when the key is let go instead of when it goes down
// (Settings > Hotkeys, "Fire playout hotkeys on key release"; off by default).
//
// Holding the key is a moment to change your mind:
//   - Esc before letting go cancels it, and nothing is sent;
//   - the preview modifier (Shift by default) held when letting go sends it to
//     the preview channel instead. The redirect reads the keyboard when the
//     command runs, so it is the modifier at release that counts, whichever was
//     pressed first;
//   - another playout hotkey pressed while holding the first one replaces it.
//
// Off, nothing here runs: the hotkeys are the ordinary shortcuts and fire on the
// press, as before.
//
// The arithmetic - which binding a press is, and what a press or release does -
// is here, tested by tools/test-hotkeyrelease. MainWindow feeds it key events.

#include <QtCore/QList>
#include <QtCore/Qt>

namespace HotkeyRelease
{
    struct Binding
    {
        int key = 0;
        Qt::KeyboardModifiers modifiers;
        int id = -1;        // which action fires
    };

    inline bool isModifierKey(int key)
    {
        return key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt || key == Qt::Key_Meta
            || key == Qt::Key_AltGr;
    }

    // The binding a press is, or -1. The keypad flag is not part of a hotkey.
    inline int match(const QList<Binding>& bindings, int key, Qt::KeyboardModifiers modifiers)
    {
        const Qt::KeyboardModifiers wanted = modifiers & ~Qt::KeypadModifier;
        for (const Binding& binding : bindings)
        {
            if (binding.key == key && (binding.modifiers & ~Qt::KeypadModifier) == wanted)
                return binding.id;
        }
        return -1;
    }

    enum class Outcome
    {
        Ignored,    // not a hotkey matter: the event goes on as usual
        Armed,      // a hotkey is held, waiting for its release
        Swallowed,  // part of a held hotkey (a repeat): nothing else sees it
        Cancelled,  // Esc while holding: nothing will fire
        Fired       // released: run the action
    };

    class Arming
    {
        public:
            bool isArmed() const { return this->armedId >= 0; }
            int armedAction() const { return this->armedId; }
            int armedKey() const { return this->key; }

            // A key going down. `binding` is what match() made of it, -1 for nothing.
            Outcome press(int pressed, bool autoRepeat, int binding)
            {
                if (isArmed())
                {
                    if (pressed == this->key)
                        return Outcome::Swallowed;
                    if (pressed == Qt::Key_Escape)
                    {
                        clear();
                        return Outcome::Cancelled;
                    }
                    if (isModifierKey(pressed))
                        return Outcome::Ignored;
                    if (binding >= 0 && !autoRepeat)
                    {
                        // Another hotkey while holding: the newer one is meant.
                        this->key = pressed;
                        this->armedId = binding;
                        return Outcome::Armed;
                    }
                    return Outcome::Ignored;
                }

                // Held on after Esc: its repeats do not arm it again.
                if (binding >= 0 && autoRepeat)
                    return Outcome::Swallowed;

                if (binding >= 0)
                {
                    this->key = pressed;
                    this->armedId = binding;
                    return Outcome::Armed;
                }
                return Outcome::Ignored;
            }

            // A key going up. On Fired, `fired` is the action to run.
            Outcome release(int released, bool autoRepeat, int* fired)
            {
                if (fired != nullptr)
                    *fired = -1;

                if (!isArmed() || released != this->key)
                    return Outcome::Ignored;

                // Some systems send a release before every repeated press.
                if (autoRepeat)
                    return Outcome::Swallowed;

                if (fired != nullptr)
                    *fired = this->armedId;
                clear();
                return Outcome::Fired;
            }

            // The window lost the keyboard: a release that never comes fires nothing.
            void reset()
            {
                clear();
            }

        private:
            int key = 0;
            int armedId = -1;

            void clear()
            {
                this->key = 0;
                this->armedId = -1;
            }
    };
}
