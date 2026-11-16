// Playout hotkeys on key release: which binding a press is, and what pressing,
// holding, Esc and letting go do.

#include "../src/Common/HotkeyRelease.h"

#include <QtCore/QString>
#include <QtCore/QTextStream>

static int checks = 0;
static int failures = 0;

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

using namespace HotkeyRelease;

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Hotkeys on release\n";

    // F2 is Play (0), with its Shift+F2 twin for the preview modifier; F1 is Stop (1).
    QList<Binding> bindings;
    bindings << Binding{Qt::Key_F2, Qt::NoModifier, 0} << Binding{Qt::Key_F2, Qt::ShiftModifier, 0}
             << Binding{Qt::Key_F1, Qt::NoModifier, 1};

    expectTrue(match(bindings, Qt::Key_F2, Qt::NoModifier) == 0, "F2 is Play");
    expectTrue(match(bindings, Qt::Key_F2, Qt::ShiftModifier) == 0, "Shift+F2 is Play too");
    expectTrue(match(bindings, Qt::Key_F2, Qt::ControlModifier) == -1, "Ctrl+F2 is nothing");
    expectTrue(match(bindings, Qt::Key_F1, Qt::KeypadModifier) == 1, "the keypad flag is not part of a hotkey");
    expectTrue(match(bindings, Qt::Key_A, Qt::NoModifier) == -1, "an unbound key is nothing");

    int fired = -2;

    // Press and let go: it fires on the release, not the press.
    Arming a;
    expectTrue(a.press(Qt::Key_F2, false, 0) == Outcome::Armed && a.isArmed(), "pressing F2 arms Play");
    expectTrue(a.press(Qt::Key_F2, true, 0) == Outcome::Swallowed, "holding it repeats nothing");
    expectTrue(a.release(Qt::Key_F2, true, &fired) == Outcome::Swallowed && fired == -1, "a repeat's release fires nothing");
    expectTrue(a.release(Qt::Key_F2, false, &fired) == Outcome::Fired && fired == 0, "letting go fires Play");
    expectTrue(!a.isArmed(), "and nothing is held after");
    expectTrue(a.release(Qt::Key_F2, false, &fired) == Outcome::Ignored && fired == -1, "a second release fires nothing");

    // Esc while holding: nothing fires, and holding on does not arm it again.
    Arming b;
    b.press(Qt::Key_F2, false, 0);
    expectTrue(b.press(Qt::Key_Escape, false, -1) == Outcome::Cancelled && !b.isArmed(), "Esc while holding cancels");
    expectTrue(b.press(Qt::Key_F2, true, 0) == Outcome::Swallowed && !b.isArmed(), "the held key's repeats do not arm it again");
    expectTrue(b.release(Qt::Key_F2, false, &fired) == Outcome::Ignored && fired == -1, "letting go after Esc fires nothing");
    expectTrue(b.press(Qt::Key_F2, false, 0) == Outcome::Armed, "the next press is a new one");

    // Shift while holding: passed on, so the preview modifier is seen; it fires on release.
    Arming c;
    c.press(Qt::Key_F2, false, 0);
    expectTrue(c.press(Qt::Key_Shift, false, -1) == Outcome::Ignored && c.isArmed(), "Shift while holding goes on to the preview modifier");
    expectTrue(c.release(Qt::Key_Shift, false, &fired) == Outcome::Ignored && c.isArmed(), "and letting go of it changes nothing held");
    expectTrue(c.release(Qt::Key_F2, false, &fired) == Outcome::Fired && fired == 0, "the release still fires");

    // Another hotkey while holding replaces the first.
    Arming d;
    d.press(Qt::Key_F2, false, 0);
    expectTrue(d.press(Qt::Key_F1, false, 1) == Outcome::Armed && d.armedAction() == 1, "pressing F1 while holding F2 means Stop");
    expectTrue(d.release(Qt::Key_F2, false, &fired) == Outcome::Ignored && fired == -1, "letting go of F2 then fires nothing");
    expectTrue(d.release(Qt::Key_F1, false, &fired) == Outcome::Fired && fired == 1, "letting go of F1 fires Stop");

    // Other keys while holding go on as usual.
    Arming e;
    e.press(Qt::Key_F2, false, 0);
    expectTrue(e.press(Qt::Key_A, false, -1) == Outcome::Ignored && e.isArmed(), "a plain key while holding goes on and keeps it held");

    // The window losing the keyboard.
    Arming f;
    f.press(Qt::Key_F2, false, 0);
    f.reset();
    expectTrue(f.release(Qt::Key_F2, false, &fired) == Outcome::Ignored && fired == -1, "a release after losing focus fires nothing");

    // Nothing held: other keys pass, Esc passes (it may be a hotkey of its own).
    Arming g;
    expectTrue(g.press(Qt::Key_Escape, false, -1) == Outcome::Ignored, "Esc with nothing held is just Esc");
    expectTrue(g.press(Qt::Key_Escape, false, 5) == Outcome::Armed, "or a hotkey, if Esc is bound");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
