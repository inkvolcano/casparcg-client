// Whether a panel is placed in the layout at all.
//
// The client builds every panel whether or not the layout asks for it, and some
// then work continuously. The Activity panel rebuilds a row on every
// playback-progress event — one per playing layer at the OSC polling rate, five
// to twenty a second during a show — and sweeps for stale rows twice a second,
// forever, for a panel that may be nowhere on screen. Server Status polls the
// sheet cache and the relay every two seconds on the same terms.
//
// Hiding a widget does not help: the signal still arrives and the handler still
// runs. The only thing that helps is not doing the work, and that needs a
// reliable answer to "is this panel anywhere".
//
// Getting it wrong in the safe direction costs a little wasted work. Getting it
// wrong in the other direction means a panel that is on screen and never
// updates, which looks like the client having frozen — so the cases below lean
// on the ways a column string can be untidy rather than absent.

#include "../src/Common/PanelPlacement.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
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

static QStringList columns(const QString& a = QString(), const QString& b = QString(),
                           const QString& c = QString(), const QString& d = QString())
{
    return QStringList() << a << b << c << d;
}

static void aPanelInAColumnIsPlaced()
{
    expectTrue(PanelPlacement::isPlaced("Activity", columns("Activity")),
               "a panel alone in a column is placed");
    expectTrue(PanelPlacement::isPlaced("Activity", columns("Library,Activity,Preview")),
               "and one in the middle of a list");
    expectTrue(PanelPlacement::isPlaced("Library", columns("Library,Activity")),
               "and one at the start");
    expectTrue(PanelPlacement::isPlaced("Preview", columns("Library,Activity,Preview")),
               "and one at the end");

    // Four columns, and it could be in any of them.
    expectTrue(PanelPlacement::isPlaced("TriggerBanks", columns("", "", "", "TriggerBanks")),
               "a panel in the fourth column is placed");
}

static void aPanelInNoColumnIsNotPlaced()
{
    expectTrue(!PanelPlacement::isPlaced("Activity", columns("Library,Preview")),
               "a panel in no column is not placed");
    expectTrue(!PanelPlacement::isPlaced("Activity", columns()),
               "and neither is one in an empty layout");
    expectTrue(!PanelPlacement::isPlaced("Activity", columns("", "", "", "")),
               "or a layout of four empty columns");
}

static void anUntidyColumnStillFindsThePanel()
{
    // This is the direction that matters. A false negative here means a panel
    // sitting on screen doing nothing, which reads as the client having frozen.
    expectTrue(PanelPlacement::isPlaced("Activity", columns("Library, Activity, Preview")),
               "spaces after commas do not hide a panel");
    expectTrue(PanelPlacement::isPlaced("Activity", columns("  Activity  ")),
               "nor space around the whole entry");
    expectTrue(PanelPlacement::isPlaced("Activity", columns("Library,,Activity")),
               "nor an empty entry from a double comma");
    expectTrue(PanelPlacement::isPlaced("Activity", columns(",Activity,")),
               "nor leading and trailing commas");
    expectTrue(PanelPlacement::isPlaced("activity", columns("Activity")),
               "and case does not hide it either");
    expectTrue(PanelPlacement::isPlaced("Activity", columns("ACTIVITY")),
               "in either direction");
}

static void aSimilarNameIsNotTheSamePanel()
{
    // "Inspector" and "SimpleInspector" are both real ids, and one contains the
    // other. A substring match would have SimpleInspector keep Inspector awake.
    expectTrue(!PanelPlacement::isPlaced("Inspector", columns("SimpleInspector")),
               "SimpleInspector does not count as Inspector");
    expectTrue(PanelPlacement::isPlaced("SimpleInspector", columns("SimpleInspector")),
               "but SimpleInspector counts as itself");

    expectTrue(!PanelPlacement::isPlaced("Activity", columns("ActivityLog")),
               "a longer name is a different panel");
    expectTrue(!PanelPlacement::isPlaced("Live", columns("AudioLevels")),
               "and a name inside another word is not a match");
}

static void nonsenseIsNotPlaced()
{
    expectTrue(!PanelPlacement::isPlaced("", columns("Activity")),
               "an empty id is never placed");
    expectTrue(!PanelPlacement::isPlaced("   ", columns("Activity")),
               "and neither is a blank one");
}

static void eachModeReadsItsOwnLayout()
{
    // Simple Mode keeps a separate arrangement, so a panel can be placed in one
    // and not the other. Reading the wrong keys would answer for the layout that
    // is not on screen.
    const QStringList normal = PanelPlacement::columnKeys(false);
    const QStringList simple = PanelPlacement::columnKeys(true);

    expectTrue(normal.contains("LayoutPanel1"), "the normal layout reads LayoutPanel1");
    expectTrue(normal.contains("LayoutPanel4"), "through LayoutPanel4");
    expectTrue(normal.size() == 4, "four columns and no more");

    expectTrue(simple.contains("SimpleLayoutPanel1"), "Simple Mode reads its own keys");
    expectTrue(simple.contains("SimpleLayoutPanel4"), "through the fourth");
    expectTrue(simple.size() == 4, "also four");

    // The two sets must not overlap, or one mode would answer for the other.
    foreach (const QString& key, normal)
        expectTrue(!simple.contains(key), QString("\"%1\" belongs to one mode only").arg(key));
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "Panel placement\n";

    aPanelInAColumnIsPlaced();
    aPanelInNoColumnIsNotPlaced();
    anUntidyColumnStillFindsThePanel();
    aSimilarNameIsNotTheSamePanel();
    nonsenseIsNotPlaced();
    eachModeReadsItsOwnLayout();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}
