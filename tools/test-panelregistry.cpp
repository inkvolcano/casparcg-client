// One list of panels, and the things that used to keep their own copies of it.
//
// Adding a panel meant editing six shared files and getting fourteen scattered
// mentions right. The failure mode is not a crash: the panel half-works. It
// places but has no sizing row. It sizes but a named layout does not save it.
// Nobody reports that, because the panel it happens to is by definition one
// hardly anyone uses.
//
// It had already happened. iNews was in the layout editor's list, absent from the
// settings dialog's, and absent from LayoutPreset's - whose comment said it was
// written there so a preset and the editor could not drift apart. A second
// hand-written list is not a defence against a first one.
//
// So these tests are not really about PanelRegistry's lookups, which are three
// lines each. They are about the property that made the bug possible being gone:
// every consumer answering from the same list, and no id existing in one place
// and not another.

#include "../src/Common/PanelRegistry.h"
#include "../src/Common/LayoutPreset.h"

#include <QtCore/QSet>
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

static void same(const QString& actual, const QString& wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n         wanted: " << wanted
                        << "\n            got: " << actual << "\n";
}

static void sameInt(int actual, int wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n         wanted: " << wanted
                        << "\n            got: " << actual << "\n";
}

static void everyPanelIsFullyDescribed()
{
    const QList<PanelRegistry::Entry> panels = PanelRegistry::all();

    expectTrue(panels.size() >= 16, "every panel the client has is registered");

    QSet<QString> seenIds;
    QSet<QString> seenNames;

    foreach (const PanelRegistry::Entry& panel, panels)
    {
        expectTrue(!panel.id.isEmpty(), "a panel has an id");

        // A blank name would show as an empty row in the layout editor and an
        // empty label in Panel Sizing - placeable, but not by anyone who could
        // tell what they were placing.
        expectTrue(!panel.displayName.isEmpty(),
                   QString("\"%1\" has something to call itself").arg(panel.id));

        // Two panels sharing an id is one panel, silently: the second's settings
        // land on the first's keys.
        expectTrue(!seenIds.contains(panel.id),
                   QString("\"%1\" is registered once").arg(panel.id));
        seenIds.insert(panel.id);

        // Two rows reading "Preview" in the sizing list is a coin toss for the
        // operator.
        expectTrue(!seenNames.contains(panel.displayName),
                   QString("\"%1\" is not a name another panel already uses").arg(panel.displayName));
        seenNames.insert(panel.displayName);

        // A panel collapsing to nothing has no header left to click to get it
        // back.
        expectTrue(panel.compactHeight > 0,
                   QString("\"%1\" leaves something to click when collapsed").arg(panel.id));

        // A default shorter than the collapsed height would open the panel
        // smaller than collapsing it.
        expectTrue(panel.defaultHeight == 0 || panel.defaultHeight >= panel.compactHeight,
                   QString("\"%1\" opens no smaller than it collapses").arg(panel.id));

        // The sizing combo only has these three; anything else and the panel's
        // row would show whatever happened to be first.
        expectTrue(panel.defaultSizeMode == "fixed" || panel.defaultSizeMode == "resizable"
                       || panel.defaultSizeMode == "expanding",
                   QString("\"%1\" starts in a mode the combo offers").arg(panel.id));
    }
}

static void aNamedLayoutSavesEveryPanelThatCanBePlaced()
{
    // The bug, stated as a test. iNews was placeable and unsaved, and this is the
    // comparison nobody was making.
    const QStringList registered = PanelRegistry::ids();
    const QStringList saved = LayoutPreset::panelIds();

    sameInt(saved.size(), registered.size(), "a named layout covers exactly the registered panels");

    foreach (const QString& id, registered)
    {
        expectTrue(saved.contains(id),
                   QString("\"%1\" is saved by a named layout").arg(id));
    }

    // And the other direction: a preset claiming keys for a panel that no longer
    // exists would keep writing settings nothing reads.
    foreach (const QString& id, saved)
    {
        expectTrue(registered.contains(id),
                   QString("\"%1\" is a panel that still exists").arg(id));
    }
}

static void everyPanelOwnsItsKeys()
{
    // Placing a panel is not enough: the preset has to claim the six keys that
    // hold how it is sized, spanned, collapsed and anchored, or applying a layout
    // restores the arrangement without restoring the panel's shape in it.
    const QStringList keys = LayoutPreset::keysFor(LayoutPreset::scopePanel());

    foreach (const PanelRegistry::Entry& panel, PanelRegistry::all())
    {
        expectTrue(keys.contains("PanelSizeMode_" + panel.id),
                   QString("\"%1\" saves its size mode").arg(panel.id));
        expectTrue(keys.contains(panel.id + "PanelHeight"),
                   QString("\"%1\" saves its height").arg(panel.id));
        expectTrue(keys.contains("PanelSpan_" + panel.id),
                   QString("\"%1\" saves its span").arg(panel.id));
        expectTrue(keys.contains("PanelCollapsed_" + panel.id),
                   QString("\"%1\" saves whether it is collapsed").arg(panel.id));
    }
}

static void theOnesThatWereMissedAreThere()
{
    // Named rather than counted, because the count was right in two of the three
    // lists while the contents were wrong.
    expectTrue(PanelRegistry::contains("Duration"), "iNews is registered");
    same(PanelRegistry::displayName("Duration"), "iNews",
         "and is called what the operator calls it, not what the code does");
    expectTrue(LayoutPreset::panelIds().contains("Duration"),
               "and a named layout saves it");
    same(PanelRegistry::defaultSizeMode("Duration"), "fixed",
         "and it has a sizing mode to start from");
}

static void theHeightsAreTheOnesTheClientAlreadyUsed()
{
    // The registry replaced two if-chains in MainWindow. If it disagrees with them
    // then every existing layout resizes itself the first time it is rebuilt,
    // which is a worse outcome than the bug being fixed.
    sameInt(PanelRegistry::defaultHeight("Preview"), 188, "Preview keeps its height");
    sameInt(PanelRegistry::defaultHeight("Live"), 188, "Live keeps its height");
    sameInt(PanelRegistry::defaultHeight("NDI"), 188, "NDI still borrows Live's");
    sameInt(PanelRegistry::defaultHeight("AudioLevels"), 147, "Audio Levels keeps its height");
    sameInt(PanelRegistry::defaultHeight("Clock"), 58, "Clock keeps its height");
    sameInt(PanelRegistry::defaultHeight("Performance"), 110, "Performance keeps its height");
    sameInt(PanelRegistry::defaultHeight("HttpLog"), 200, "Http Log keeps its height");
    sameInt(PanelRegistry::defaultHeight("Sheets"), 320, "Google Sheets keeps its height");
    sameInt(PanelRegistry::defaultHeight("SimpleInspector"), 320, "Simple Inspector keeps its height");

    // The panels the old chain fell through on, which is not the same as an
    // unknown panel - it is a panel that takes what the column gives it.
    sameInt(PanelRegistry::defaultHeight("Library"), 0, "Library still takes what it is given");
    sameInt(PanelRegistry::defaultHeight("Inspector"), 0, "and so does Inspector");
    sameInt(PanelRegistry::defaultHeight("Duration"), 0, "and so does iNews");

    // Everything collapsed to the tab header before and still does.
    sameInt(PanelRegistry::compactHeight("Preview"), 25, "Preview collapses to its header");
    sameInt(PanelRegistry::compactHeight("Library"), 25, "and Library to its");
    sameInt(PanelRegistry::compactHeight("NDI"), 25, "and NDI to its");
}

static void theSizingModesAreTheOnesTheDialogAlreadyShowed()
{
    same(PanelRegistry::defaultSizeMode("AudioLevels"), "fixed", "Audio Levels is fixed");
    same(PanelRegistry::defaultSizeMode("Preview"), "resizable", "Preview is resizable");
    same(PanelRegistry::defaultSizeMode("Library"), "expanding", "Library expands");
    same(PanelRegistry::defaultSizeMode("Inspector"), "expanding", "Inspector expands");
    same(PanelRegistry::defaultSizeMode("Activity"), "expanding", "Activity expands");
    same(PanelRegistry::defaultSizeMode("ServerStatus"), "fixed", "Server Status is fixed");
    same(PanelRegistry::defaultSizeMode("TriggerBanks"), "fixed", "Trigger Banks is fixed");
    same(PanelRegistry::defaultSizeMode("Live"), "resizable", "Live is resizable");
    same(PanelRegistry::defaultSizeMode("NDI"), "resizable", "NDI is resizable");
    same(PanelRegistry::defaultSizeMode("Sheets"), "resizable", "Google Sheets is resizable");
    same(PanelRegistry::defaultSizeMode("SimpleInspector"), "resizable", "Simple Inspector is resizable");
    same(PanelRegistry::defaultSizeMode("Clock"), "fixed", "Clock is fixed");
    same(PanelRegistry::defaultSizeMode("StatusBar"), "fixed", "the Status Bar is fixed");
}

static void anUnknownIdIsHarmless()
{
    // Layout columns are stored as text in the user's database, so an id can
    // outlive the panel it named - after a downgrade, or a hand-edited setting.
    // Every lookup has to answer rather than assert.
    expectTrue(!PanelRegistry::contains("Teleprompter"), "an unregistered id is not registered");
    same(PanelRegistry::displayName("Teleprompter"), "Teleprompter",
         "and shows as itself rather than as blank");
    sameInt(PanelRegistry::defaultHeight("Teleprompter"), 0,
            "and asks for no particular height");
    sameInt(PanelRegistry::compactHeight("Teleprompter"), 25,
            "and still leaves a header to click");
    same(PanelRegistry::defaultSizeMode("Teleprompter"), "fixed",
         "and sits in the mode that disturbs the column least");

    // Empty is the case that actually arrives: a layout column of "Library,,Clock"
    // split on commas.
    same(PanelRegistry::displayName(""), "", "an empty id is not a panel");
    expectTrue(!PanelRegistry::contains(""), "and is not registered");
}

static void theIdsAreSafeToStore()
{
    // Panel ids are concatenated into configuration keys ("PanelSpan_" + id) and
    // joined with commas into the layout columns. A comma in an id would split one
    // panel into two; whitespace would make a key that never matches the one that
    // wrote it.
    foreach (const PanelRegistry::Entry& panel, PanelRegistry::all())
    {
        expectTrue(!panel.id.contains(','),
                   QString("\"%1\" survives being joined into a column").arg(panel.id));
        expectTrue(panel.id.trimmed() == panel.id,
                   QString("\"%1\" has no edge whitespace").arg(panel.id));
        expectTrue(!panel.id.contains(' '),
                   QString("\"%1\" makes a key that matches itself").arg(panel.id));
    }
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "Panel registry\n";

    everyPanelIsFullyDescribed();
    aNamedLayoutSavesEveryPanelThatCanBePlaced();
    everyPanelOwnsItsKeys();
    theOnesThatWereMissedAreThere();
    theHeightsAreTheOnesTheClientAlreadyUsed();
    theSizingModesAreTheOnesTheDialogAlreadyShowed();
    anUnknownIdIsHarmless();
    theIdsAreSafeToStore();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}
