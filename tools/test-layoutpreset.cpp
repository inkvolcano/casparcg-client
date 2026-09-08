// Named layouts: which settings a preset owns, and the round trip.
//
// A layout is a couple of dozen configuration rows rather than one setting, and
// two things about that are easy to get wrong in ways nobody notices until a
// show:
//
//   A preset must own exactly its own keys. Too few and applying a layout leaves
//   half the old arrangement behind; too many and applying a layout quietly
//   resets something that has nothing to do with layout — a server address, a
//   hotkey. The key set is therefore asserted, not assumed.
//
//   The two scopes must not overlap. The normal layout and the Simple Mode
//   layout are separate arrangements, and a key claimed by both would mean
//   applying one silently rearranged the other.
//
// The apply path writes every key the scope owns, including the ones the preset
// has nothing for, which is why "not in the preset" has to round-trip as absent
// rather than as an empty string that happens to look the same.

#include "../src/Common/LayoutPreset.h"

#include <QtCore/QMap>
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
    QTextStream(stdout) << "  FAIL  " << what
                        << "  (wanted \"" << wanted << "\", got \"" << actual << "\")\n";
}

static void sameInt(int actual, int wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what
                        << "  (wanted " << wanted << ", got " << actual << ")\n";
}

static void eachScopeOwnsItsOwnArrangement()
{
    const QStringList panel = LayoutPreset::keysFor(LayoutPreset::scopePanel());
    const QStringList simple = LayoutPreset::keysFor(LayoutPreset::scopeSimple());

    expectTrue(!panel.isEmpty(), "the normal scope owns something");
    expectTrue(!simple.isEmpty(), "the Simple Mode scope owns something");

    // The columns and their contents are the layout. Without these, applying a
    // preset would move nothing.
    expectTrue(panel.contains("LayoutColumnOrder"), "the normal scope owns the column order");
    expectTrue(panel.contains("LayoutPanel1"), "and the first column's contents");
    expectTrue(panel.contains("LayoutPanel4"), "and the fourth's");

    expectTrue(simple.contains("SimpleLayoutColumnOrder"), "Simple Mode owns its own column order");
    expectTrue(simple.contains("SimpleLayoutPanel1"), "and its own column contents");
    expectTrue(simple.contains("SimpleModeColumns"), "and the button grid width");
    expectTrue(simple.contains("SimpleModeRows"), "and its height");
}

static void theTwoScopesNeverClaimTheSameKey()
{
    // Held in a named list first: iterators taken from two separate temporaries
    // do not belong to the same container.
    const QStringList panelKeys = LayoutPreset::keysFor(LayoutPreset::scopePanel());
    const QSet<QString> panel(panelKeys.begin(), panelKeys.end());
    const QStringList simple = LayoutPreset::keysFor(LayoutPreset::scopeSimple());

    foreach (const QString& key, simple)
    {
        expectTrue(!panel.contains(key),
                   QString("\"%1\" is claimed by only one scope").arg(key));
    }
}

static void everyPanelIsSavedInEveryWayItCanBeArranged()
{
    const QStringList keys = LayoutPreset::keysFor(LayoutPreset::scopePanel());
    const QStringList panels = LayoutPreset::panelIds();

    expectTrue(panels.size() >= 15, "every panel the editor offers is listed");

    // Miss one of these and a layout comes back subtly wrong — the right panels
    // in the right columns, but the wrong heights, or a panel still collapsed.
    foreach (const QString& panel, panels)
    {
        expectTrue(keys.contains("PanelSizeMode_" + panel),
                   QString("%1: size mode is saved").arg(panel));
        expectTrue(keys.contains("PanelCollapsed_" + panel),
                   QString("%1: collapsed state is saved").arg(panel));
        expectTrue(keys.contains("PanelSpan_" + panel),
                   QString("%1: span is saved").arg(panel));
        expectTrue(keys.contains("PanelSpanDir_" + panel),
                   QString("%1: span direction is saved").arg(panel));
        expectTrue(keys.contains("PanelAnchor_" + panel),
                   QString("%1: anchor is saved").arg(panel));
        expectTrue(keys.contains(panel + "PanelHeight"),
                   QString("%1: height is saved").arg(panel));
    }
}

static void nothingOutsideTheLayoutIsOwned()
{
    // The apply path writes every key the scope owns. Anything in this list
    // appearing there would mean loading a layout also changed a server, a
    // hotkey, or whether shell commands may run.
    QStringList mustNotOwn;
    mustNotOwn << "AutoSaveEnabled" << "AllowShellCommands" << "PreviewLegacyMode"
               << "PreviewAutoPlayVideo" << "RelayUrl" << "RelayToken" << "RelayPacks"
               << "HotkeyPlay" << "PreviewModifier" << "DatabaseVersion" << "Theme"
               << "SheetsApiKey" << "RundownRepository" << "FontSize";

    const QStringList panel = LayoutPreset::keysFor(LayoutPreset::scopePanel());
    const QStringList simple = LayoutPreset::keysFor(LayoutPreset::scopeSimple());

    foreach (const QString& key, mustNotOwn)
    {
        expectTrue(!panel.contains(key), QString("the normal scope leaves \"%1\" alone").arg(key));
        expectTrue(!simple.contains(key), QString("Simple Mode leaves \"%1\" alone").arg(key));
    }
}

static void anUnknownScopeOwnsNothing()
{
    // Better to write nothing than to guess a scope and rewrite the wrong set.
    expectTrue(LayoutPreset::keysFor("").isEmpty(), "an empty scope owns nothing");
    expectTrue(LayoutPreset::keysFor("nonsense").isEmpty(), "an unknown scope owns nothing");

    expectTrue(LayoutPreset::isKnownScope(LayoutPreset::scopePanel()), "panel is a known scope");
    expectTrue(LayoutPreset::isKnownScope(LayoutPreset::scopeSimple()), "simple is a known scope");
    expectTrue(!LayoutPreset::isKnownScope("nonsense"), "and nonsense is not");
}

static void theRoundTripKeepsWhatWasSaved()
{
    QMap<QString, QString> values;
    values.insert("LayoutColumnOrder", "panel1,mainwindow,panel2");
    values.insert("LayoutPanel1", "Library,Preview");
    values.insert("PanelSpan_Library", "2");
    values.insert("LibraryPanelHeight", "480");
    values.insert("PanelCollapsed_Clock", "true");

    const QMap<QString, QString> back =
        LayoutPreset::deserialise(LayoutPreset::serialise(values), LayoutPreset::scopePanel());

    sameInt(back.size(), values.size(), "every saved key came back");
    same(back.value("LayoutColumnOrder"), "panel1,mainwindow,panel2", "the column order survived");
    same(back.value("LayoutPanel1"), "Library,Preview", "a column's contents survived");
    same(back.value("PanelSpan_Library"), "2", "a span survived");
    same(back.value("LibraryPanelHeight"), "480", "a height survived");
    same(back.value("PanelCollapsed_Clock"), "true", "a collapsed state survived");
}

static void aKeyTheScopeDoesNotOwnIsNotAppliedFromAPreset()
{
    // A preset carrying something else — hand-edited, or written by a version
    // that owned more keys — must not be able to write it back.
    const QString json =
        "{\"LayoutColumnOrder\":\"panel1\",\"AllowShellCommands\":\"true\",\"RelayToken\":\"secret\"}";

    const QMap<QString, QString> values = LayoutPreset::deserialise(json, LayoutPreset::scopePanel());

    expectTrue(values.contains("LayoutColumnOrder"), "the layout key is read");
    expectTrue(!values.contains("AllowShellCommands"), "a setting outside the scope is refused");
    expectTrue(!values.contains("RelayToken"), "and so is a token");
    sameInt(values.size(), 1, "exactly one key survived");
}

static void aSimplePresetCannotRewriteTheNormalLayout()
{
    // Same file, read as the other scope: the keys it carries are not that
    // scope's, so nothing is applied. This is what keeps the two arrangements
    // independent even if a preset is somehow stored under the wrong scope.
    const QString json = "{\"SimpleLayoutColumnOrder\":\"mainwindow\",\"SimpleModeColumns\":\"6\"}";

    expectTrue(LayoutPreset::deserialise(json, LayoutPreset::scopePanel()).isEmpty(),
               "a Simple Mode preset read as a normal one applies nothing");
    sameInt(LayoutPreset::deserialise(json, LayoutPreset::scopeSimple()).size(), 2,
            "and read as its own scope it applies both keys");
}

static void rubbishIsRefusedRatherThanHalfApplied()
{
    expectTrue(LayoutPreset::deserialise("", LayoutPreset::scopePanel()).isEmpty(),
               "an empty preset applies nothing");
    expectTrue(LayoutPreset::deserialise("not json at all", LayoutPreset::scopePanel()).isEmpty(),
               "unparseable data applies nothing");
    expectTrue(LayoutPreset::deserialise("[1,2,3]", LayoutPreset::scopePanel()).isEmpty(),
               "a JSON array is not a preset");

    // Everything in the Configuration table is text. A number or a boolean here
    // came from something other than this client, and writing "true" or "1" back
    // as a setting nobody chose is worse than skipping it.
    const QString typed = "{\"LayoutColumnOrder\":\"panel1\",\"LayoutPanel1\":42,\"LayoutPanel2\":true}";
    const QMap<QString, QString> values = LayoutPreset::deserialise(typed, LayoutPreset::scopePanel());

    sameInt(values.size(), 1, "only the string value was taken");
    expectTrue(!values.contains("LayoutPanel1"), "a number is not turned into a setting");
    expectTrue(!values.contains("LayoutPanel2"), "and neither is a boolean");
}

static void namesAreTidiedRatherThanTrusted()
{
    same(LayoutPreset::sanitiseName("  Studio A  "), "Studio A", "surrounding space goes");
    same(LayoutPreset::sanitiseName("Studio\tA"), "Studio A", "a tab becomes a space");
    same(LayoutPreset::sanitiseName("Studio\nA"), "Studio A", "and so does a newline");

    // A name goes into a combo box and a list. A control character would break
    // the row it is drawn in.
    expectTrue(!LayoutPreset::sanitiseName("Studio\x01A").contains(QChar(0x01)),
               "a control character is removed");

    expectTrue(LayoutPreset::sanitiseName("").isEmpty(), "an empty name stays empty");
    expectTrue(LayoutPreset::sanitiseName("     ").isEmpty(), "and so does one of only spaces");

    // Long enough to be a pasted paragraph rather than a name.
    expectTrue(LayoutPreset::sanitiseName(QString(500, 'x')).length() <= 60,
               "a very long name is cut to something a list can show");
}

static void namesCollideRegardlessOfCase()
{
    // Two presets called "Studio" and "studio" in one list is a trap, so the
    // save path treats them as the same name and offers to replace.
    expectTrue(LayoutPreset::sameName("Studio", "studio"), "case is ignored");
    expectTrue(LayoutPreset::sameName("Studio A", " Studio A "), "so is surrounding space");
    expectTrue(!LayoutPreset::sameName("Studio A", "Studio B"), "but different names differ");
    expectTrue(!LayoutPreset::sameName("Studio", ""), "and nothing matches an empty name");
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "Named layouts\n";

    eachScopeOwnsItsOwnArrangement();
    theTwoScopesNeverClaimTheSameKey();
    everyPanelIsSavedInEveryWayItCanBeArranged();
    nothingOutsideTheLayoutIsOwned();
    anUnknownScopeOwnsNothing();
    theRoundTripKeepsWhatWasSaved();
    aKeyTheScopeDoesNotOwnIsNotAppliedFromAPreset();
    aSimplePresetCannotRewriteTheNormalLayout();
    rubbishIsRefusedRatherThanHalfApplied();
    namesAreTidiedRatherThanTrusted();
    namesCollideRegardlessOfCase();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}
