// Whether a rundown accepts a drop.
//
// Dragging items between two rundowns side by side already worked; it arrived
// with split view. The half that did not arrive was the other side of the lock.
//
// A locked rundown is one whose content belongs to somebody else — a repository
// rundown. It refuses key presses, and it refuses to be dragged from. Nothing
// refused a drop into one, so the lock held every door but that one, and the
// second pane put a new door right next to it.
//
// The rule is asserted here rather than left inline because two places have to
// agree about it: the handler that performs the drop, and the drag events that
// decide what the cursor looks like on the way in. If those disagree the
// operator gets a cursor that says "yes" over a target that then does nothing,
// which reads as the client having dropped the drag on the floor.

#include "../src/Common/DropRules.h"

#include <QtCore/QString>
#include <QtCore/QTextStream>

static int failures = 0;
static int checks = 0;

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

static void anUnlockedRundownTakesWhatIsOffered()
{
    expectTrue(DropRules::accepts(false, true, false), "an unlocked rundown takes a library item");
    expectTrue(DropRules::accepts(false, false, true), "and an item dragged from another rundown");
    expectTrue(DropRules::accepts(false, true, true), "and a drag carrying both");
}

static void aLockedRundownTakesNothing()
{
    // The bug this exists to close: the second pane made it easy to drag into a
    // locked rundown, and nothing stopped it.
    expectTrue(!DropRules::accepts(true, false, true), "a locked rundown refuses an item from another pane");
    expectTrue(!DropRules::accepts(true, true, false), "and refuses a library item");
    expectTrue(!DropRules::accepts(true, true, true), "and refuses a drag carrying both");
}

static void somethingElseEntirelyIsNotOurs()
{
    // A file dragged from the desktop, or a drag from another application. Not a
    // rundown's business either way, locked or not.
    expectTrue(!DropRules::accepts(false, false, false), "an unlocked rundown ignores a foreign drag");
    expectTrue(!DropRules::accepts(true, false, false), "and so does a locked one");

    expectTrue(!DropRules::isRundownPayload(false, false), "a foreign drag is not a rundown payload");
    expectTrue(DropRules::isRundownPayload(true, false), "a library item is");
    expectTrue(DropRules::isRundownPayload(false, true), "and so is a rundown item");
}

static void aRefusalThatMattersIsExplained()
{
    // Silence here is the failure mode that wastes an operator's time: the items
    // do not move and nothing says why.
    expectTrue(!DropRules::refusalReason(true, false, true).isEmpty(),
               "refusing a locked target says so");
    expectTrue(DropRules::refusalReason(true, false, true).contains("locked"),
               "and the message names the lock");

    // Nothing to explain when it worked.
    expectTrue(DropRules::refusalReason(false, false, true).isEmpty(),
               "an accepted drop explains nothing");

    // And nothing to explain about a drag that was never aimed at us — an
    // operator dragging a file across the window should not get a message.
    expectTrue(DropRules::refusalReason(false, false, false).isEmpty(),
               "a foreign drag over an unlocked rundown says nothing");
    expectTrue(DropRules::refusalReason(true, false, false).isEmpty(),
               "and nothing over a locked one either");
}

static void theCursorAndTheDropCannotDisagree()
{
    // dragEnterEvent and dragMoveEvent refuse on the lock; dropMimeData refuses
    // on accepts(). Both are driven by the same flag, so walking the whole truth
    // table is the cheap way to keep them from drifting apart.
    for (int locked = 0; locked <= 1; locked++)
    {
        for (int library = 0; library <= 1; library++)
        {
            for (int rundown = 0; rundown <= 1; rundown++)
            {
                const bool accepted = DropRules::accepts(locked, library, rundown);
                const bool cursorWouldAllow = !locked;
                const bool isOurs = DropRules::isRundownPayload(library, rundown);

                // Whenever the payload is ours, the cursor's answer and the
                // drop's answer must be the same one.
                if (isOurs)
                {
                    expectTrue(accepted == cursorWouldAllow,
                               QString("locked=%1 library=%2 rundown=%3: cursor and drop agree")
                                   .arg(locked).arg(library).arg(rundown));
                }
                else
                {
                    expectTrue(!accepted,
                               QString("locked=%1: a foreign drag is never accepted").arg(locked));
                }
            }
        }
    }
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "Rundown drop rules\n";

    anUnlockedRundownTakesWhatIsOffered();
    aLockedRundownTakesNothing();
    somethingElseEntirelyIsNotOurs();
    aRefusalThatMattersIsExplained();
    theCursorAndTheDropCannotDisagree();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}
