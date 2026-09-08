// Finding OGraf graphics in a template folder, and naming them in the Library.
//
// The whole chain already worked once a graphic was in a rundown: a TEMPLATE item
// whose name resolves to a manifest previews as a graphic and fills the
// Inspector's typed fields from the manifest's schema. What was missing was any
// way to find one, because the Library lists what a server scanned and a server
// does not know what OGraf is.
//
// Two things decide whether this is worth having:
//
//   The name has to round-trip. What the Library puts on a rundown item is the
//   string the resolver will look up again later. If the two disagree by so much
//   as a suffix, an item dragged out of the Library previews as "not found" —
//   which looks like the graphic being broken rather than the name being wrong.
//
//   The walk has to stay cheap. A template folder can be enormous and the Library
//   refreshes on a timer, so the depth and the count are capped, and whole
//   categories of folder are never entered.

#include "../src/Common/OgrafLibrary.h"

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

static void theNameIsWhatTheResolverWillLookUp()
{
    // Ograf::findManifest joins the template path to the item's name and expects
    // to find "<name>.ograf.json", or a folder holding exactly one manifest. So
    // the name is the manifest path with the suffix taken off, and nothing else.
    same(OgrafLibrary::itemNameFor("l3rd.ograf.json"), "l3rd",
         "a manifest at the root becomes its bare name");
    same(OgrafLibrary::itemNameFor("SEVILLE/l3rd.ograf.json"), "SEVILLE/l3rd",
         "a manifest in a folder keeps the folder");
    same(OgrafLibrary::itemNameFor("a/b/scoreboard.ograf.json"), "a/b/scoreboard",
         "and keeps every folder above it");
}

static void theNameSurvivesHowThePathArrives()
{
    // relativeFilePath can hand back either separator depending on the platform
    // and how the path was built.
    same(OgrafLibrary::itemNameFor(QString("SEVILLE") + QChar(0x5C) + "l3rd.ograf.json"),
         "SEVILLE/l3rd", "a backslash separator is normalised");

    same(OgrafLibrary::itemNameFor("/l3rd.ograf.json"), "l3rd",
         "a leading separator is dropped");
    same(OgrafLibrary::itemNameFor("///l3rd.ograf.json"), "l3rd",
         "and so are several");

    // The suffix match is case-insensitive because the spec fixes the name, not
    // the case somebody typed it in.
    same(OgrafLibrary::itemNameFor("L3RD.OGRAF.JSON"), "L3RD",
         "an upper-case suffix is still a suffix");
}

static void theLibraryShowsTheGraphicsOwnName()
{
    // A manifest's name is written for people; the path is written for machines.
    same(OgrafLibrary::displayNameFor("Lower 3rd - Name", "SEVILLE/l3rd.ograf.json"),
         "Lower 3rd - Name", "the manifest name is preferred");

    // Without one, the file name is a better label than the whole path.
    same(OgrafLibrary::displayNameFor("", "SEVILLE/l3rd.ograf.json"), "l3rd",
         "with no name, the leaf of the path stands in");
    same(OgrafLibrary::displayNameFor("   ", "SEVILLE/l3rd.ograf.json"), "l3rd",
         "and a name of only spaces is no name");
    same(OgrafLibrary::displayNameFor("", "l3rd.ograf.json"), "l3rd",
         "a manifest at the root falls back to itself");
}

static void thingsInsideAGraphicAreNotOtherGraphics()
{
    // These are where a graphic keeps its dependencies. Descending into them
    // costs time and finds nothing, and node_modules alone can be tens of
    // thousands of directories.
    foreach (const QString& name, QStringList()
             << "node_modules" << "lib" << "libs" << "assets"
             << "fonts" << "images" << "img" << ".git" << ".svn")
    {
        expectTrue(OgrafLibrary::isIgnoredDirectory(name),
                   QString("\"%1\" is not descended into").arg(name));
    }

    // Case should not decide whether a folder is skipped.
    expectTrue(OgrafLibrary::isIgnoredDirectory("Node_Modules"), "case does not matter");
    expectTrue(OgrafLibrary::isIgnoredDirectory("LIB"), "in either direction");

    // Anything beginning with a dot is somebody's tooling.
    expectTrue(OgrafLibrary::isIgnoredDirectory(".vscode"), "a dot folder is skipped");

    // And a folder that might hold a graphic is entered.
    expectTrue(!OgrafLibrary::isIgnoredDirectory("SEVILLE"), "an ordinary folder is entered");
    expectTrue(!OgrafLibrary::isIgnoredDirectory("lower-thirds"), "and so is a plausible one");
    expectTrue(!OgrafLibrary::isIgnoredDirectory("library"), "a name merely starting with lib is not lib");
}

static void theWalkIsCapped()
{
    // The Library refreshes on a timer, so an unbounded walk of somebody's whole
    // media drive is not an option.
    expectTrue(OgrafLibrary::maxDepth() >= 2,
               "deep enough to find a graphic in its own folder");
    expectTrue(OgrafLibrary::maxDepth() <= 4,
               "shallow enough not to walk an asset tree");

    expectTrue(OgrafLibrary::maxGraphics() >= 100,
               "room for a real estate of graphics");
    expectTrue(OgrafLibrary::maxGraphics() <= 5000,
               "but a ceiling, so a wrong folder cannot fill the Library");
}

static void aNameAndAPathRoundTripTogether()
{
    // The pairing that matters: whatever is shown, the name stored on the item
    // must still resolve. Walk a few shapes and check the two stay consistent.
    struct Case { const char* path; const char* manifestName; const char* item; };
    const Case cases[] = {
        { "l3rd.ograf.json",                 "Lower 3rd",  "l3rd" },
        { "SEVILLE/l3rd.ograf.json",         "Lower 3rd",  "SEVILLE/l3rd" },
        { "SEVILLE/l3rd.ograf.json",         "",           "SEVILLE/l3rd" },
        { "scoreboard/manifest.ograf.json",  "Scoreboard", "scoreboard/manifest" },
    };

    for (const Case& c : cases)
    {
        const QString item = OgrafLibrary::itemNameFor(c.path);
        same(item, c.item, QString("\"%1\" resolves to \"%2\"").arg(c.path, c.item));

        // The display name never becomes the stored name by accident.
        const QString display = OgrafLibrary::displayNameFor(c.manifestName, c.path);
        expectTrue(!display.isEmpty(), QString("\"%1\" has something to show").arg(c.path));
    }
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "OGraf in the Library\n";

    theNameIsWhatTheResolverWillLookUp();
    theNameSurvivesHowThePathArrives();
    theLibraryShowsTheGraphicsOwnName();
    thingsInsideAGraphicAreNotOtherGraphics();
    theWalkIsCapped();
    aNameAndAPathRoundTripTogether();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}
