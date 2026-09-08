// Deciding that a rundown item points at media that is not there.
//
// The whole value of this warning is that it is trustworthy. A marker that
// appears on rows which are perfectly fine teaches an operator to ignore it,
// and then it is worse than nothing, because it is a warning that will be
// ignored on the night it is right.
//
// So the interesting cases here are not the obvious "the file is missing" ones.
// They are the ones where the honest answer is "I cannot tell":
//
//   A client that has never connected to a server has an empty library. If an
//   empty library counted as "checked and not found", opening any rundown on a
//   fresh install would flag every single row.
//
//   A media path pointing at the server's own C:\ is meaningless on a different
//   machine. A configured path that this computer cannot see is not an authority
//   on anything and must not be treated as one.
//
//   The two sources disagree constantly and legitimately — a library scanned
//   before a file was added, media that lives somewhere only the server sees —
//   so either one vouching is enough.

#include "../src/Common/MediaCheck.h"

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

static QString verdictName(MediaCheck::Verdict verdict)
{
    switch (verdict)
    {
        case MediaCheck::Verdict::NotApplicable: return "NotApplicable";
        case MediaCheck::Verdict::Present:       return "Present";
        case MediaCheck::Verdict::Missing:       return "Missing";
        case MediaCheck::Verdict::Unknown:       return "Unknown";
    }

    return "?";
}

static void sameVerdict(MediaCheck::Verdict actual, MediaCheck::Verdict wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what
                        << "  (wanted " << verdictName(wanted) << ", got " << verdictName(actual) << ")\n";
}

static MediaCheck::Evidence evidence(bool inLibrary, bool libraryUsable, bool onDisk, bool pathUsable)
{
    MediaCheck::Evidence e;
    e.inLibrary = inLibrary;
    e.libraryUsable = libraryUsable;
    e.onDisk = onDisk;
    e.pathUsable = pathUsable;

    return e;
}

static void onlyItemsThatNameAFileAreChecked()
{
    expectTrue(MediaCheck::typeUsesMedia("MOVIE"), "a movie names a file");
    expectTrue(MediaCheck::typeUsesMedia("STILL"), "so does a still");
    expectTrue(MediaCheck::typeUsesMedia("AUDIO"), "and audio");
    expectTrue(MediaCheck::typeUsesMedia("TEMPLATE"), "and a template");
    expectTrue(MediaCheck::typeUsesMedia("IMAGESCROLLER"), "and an image scroller");

    // Everything else has nothing that can be missing, and marking one would be
    // a warning about a thing that does not exist.
    expectTrue(!MediaCheck::typeUsesMedia("COMMANDGATEWAY"), "a gateway names no file");
    expectTrue(!MediaCheck::typeUsesMedia("GROUP"), "nor does a group");
    expectTrue(!MediaCheck::typeUsesMedia("SHELLCOMMAND"), "nor a shell command");
    expectTrue(!MediaCheck::typeUsesMedia("PLAYOUTCOMMAND"), "nor a playout command");
    expectTrue(!MediaCheck::typeUsesMedia(""), "nor an item with no type at all");

    sameVerdict(MediaCheck::verdictFor("COMMANDGATEWAY", "anything", evidence(false, true, false, true)),
                MediaCheck::Verdict::NotApplicable,
                "a gateway is never checked, whatever the evidence says");
}

static void anEmptyNameIsUnfinishedRatherThanBroken()
{
    // A freshly dragged-in item has no name yet. Marking it would put a warning
    // on every new row the moment it is added.
    sameVerdict(MediaCheck::verdictFor("MOVIE", "", evidence(false, true, false, true)),
                MediaCheck::Verdict::NotApplicable, "an empty name is not a broken item");
    sameVerdict(MediaCheck::verdictFor("MOVIE", "   ", evidence(false, true, false, true)),
                MediaCheck::Verdict::NotApplicable, "and neither is a name of only spaces");
}

static void eitherSourceVouchingIsEnough()
{
    sameVerdict(MediaCheck::verdictFor("MOVIE", "opener", evidence(true, true, true, true)),
                MediaCheck::Verdict::Present, "both sources agree it is there");

    // The library was scanned before the file was added, or the media lives
    // somewhere only this machine can see.
    sameVerdict(MediaCheck::verdictFor("MOVIE", "opener", evidence(false, true, true, true)),
                MediaCheck::Verdict::Present, "on disk but not in the library is still present");

    // The media lives on the server and this machine cannot see the folder.
    sameVerdict(MediaCheck::verdictFor("MOVIE", "opener", evidence(true, true, false, true)),
                MediaCheck::Verdict::Present, "in the library but not on this disk is still present");
}

static void missingIsOnlyClaimedWhenSomethingCouldHaveKnown()
{
    // The real case this feature exists for: a server that has scanned, a folder
    // this machine can see, and neither has it.
    sameVerdict(MediaCheck::verdictFor("MOVIE", "renamed-last-week", evidence(false, true, false, true)),
                MediaCheck::Verdict::Missing, "both sources looked and neither found it");

    sameVerdict(MediaCheck::verdictFor("MOVIE", "gone", evidence(false, true, false, false)),
                MediaCheck::Verdict::Missing, "the library alone is enough to say missing");

    sameVerdict(MediaCheck::verdictFor("MOVIE", "gone", evidence(false, false, false, true)),
                MediaCheck::Verdict::Missing, "a readable folder alone is enough to say missing");
}

static void nothingIsClaimedWhenNothingCouldHaveKnown()
{
    // A fresh install that has never connected to a server and has no media path
    // set. Every row would otherwise be flagged, which is how a warning becomes
    // noise and stops being read.
    sameVerdict(MediaCheck::verdictFor("MOVIE", "opener", evidence(false, false, false, false)),
                MediaCheck::Verdict::Unknown, "no library and no usable path claims nothing");

    // The path is set but points at the server's own drive, which does not exist
    // on this machine, so it is not an authority.
    sameVerdict(MediaCheck::verdictFor("TEMPLATE", "l3rd", evidence(false, false, false, false)),
                MediaCheck::Verdict::Unknown, "an unusable path is not evidence of absence");

    expectTrue(MediaCheck::verdictFor("MOVIE", "opener", evidence(false, false, false, false))
                   != MediaCheck::Verdict::Missing,
               "and it is certainly not reported as missing");
}

static void templatesAreLookedUpUnderTheTemplatePath()
{
    expectTrue(MediaCheck::typeUsesTemplatePath("TEMPLATE"), "a template uses the template path");

    // Everything else lives under media. Looking a clip up under the template
    // path would find nothing and report every clip missing.
    expectTrue(!MediaCheck::typeUsesTemplatePath("MOVIE"), "a movie uses the media path");
    expectTrue(!MediaCheck::typeUsesTemplatePath("STILL"), "and so does a still");
    expectTrue(!MediaCheck::typeUsesTemplatePath("AUDIO"), "and audio");
    expectTrue(!MediaCheck::typeUsesTemplatePath("IMAGESCROLLER"), "and an image scroller");
}

static void eachTypeIsTriedAgainstItsOwnExtensions()
{
    // CasparCG media names carry no extension, so the check has to try the ones
    // a server would have accepted. Trying a video's extensions for a template
    // would find nothing and report it missing.
    expectTrue(MediaCheck::extensionsFor("TEMPLATE").contains(".html"), "a template tries .html");
    expectTrue(MediaCheck::extensionsFor("AUDIO").contains(".wav"), "audio tries .wav");
    expectTrue(MediaCheck::extensionsFor("AUDIO").contains(".mp3"), "and .mp3");
    expectTrue(MediaCheck::extensionsFor("STILL").contains(".png"), "a still tries .png");
    expectTrue(MediaCheck::extensionsFor("IMAGESCROLLER").contains(".png"), "so does a scroller");
    expectTrue(MediaCheck::extensionsFor("MOVIE").contains(".mov"), "a movie tries .mov");
    expectTrue(MediaCheck::extensionsFor("MOVIE").contains(".mp4"), "and .mp4");

    // The lists must not be interchangeable, or the type would not matter.
    expectTrue(!MediaCheck::extensionsFor("MOVIE").contains(".html"), "a movie does not try .html");
    expectTrue(!MediaCheck::extensionsFor("TEMPLATE").contains(".mov"), "a template does not try .mov");
    expectTrue(!MediaCheck::extensionsFor("STILL").contains(".wav"), "a still does not try .wav");

    foreach (const QString& type, QStringList() << "MOVIE" << "STILL" << "AUDIO" << "TEMPLATE" << "IMAGESCROLLER")
        expectTrue(!MediaCheck::extensionsFor(type).isEmpty(), QString("%1 has extensions to try").arg(type));
}

static void theExplanationSaysWhichSourcesLooked()
{
    // "Missing" means something different with and without a server, and an
    // operator has to know which one they are looking at before deciding whether
    // to trust it.
    const QString both = MediaCheck::explain("MOVIE", "opener", evidence(false, true, false, true));
    expectTrue(both.contains("opener"), "the name is in the explanation");
    expectTrue(both.contains("library"), "and the library is mentioned");
    expectTrue(both.contains("path"), "and the path");

    const QString libraryOnly = MediaCheck::explain("MOVIE", "opener", evidence(false, true, false, false));
    expectTrue(libraryOnly.contains("No media path"), "when only the library looked, it says the disk was not checked");

    const QString diskOnly = MediaCheck::explain("MOVIE", "opener", evidence(false, false, false, true));
    expectTrue(diskOnly.contains("library is empty"), "when only the disk looked, it says the library was not checked");

    // Nothing to explain about an item that is fine.
    expectTrue(MediaCheck::explain("MOVIE", "opener", evidence(true, true, true, true)).isEmpty(),
               "a present item has no explanation");
    expectTrue(MediaCheck::explain("MOVIE", "opener", evidence(false, false, false, false)).isEmpty(),
               "and neither does an unknown one");
    expectTrue(MediaCheck::explain("GROUP", "opener", evidence(false, true, false, true)).isEmpty(),
               "and neither does a type that names no file");
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "Missing media\n";

    onlyItemsThatNameAFileAreChecked();
    anEmptyNameIsUnfinishedRatherThanBroken();
    eitherSourceVouchingIsEnough();
    missingIsOnlyClaimedWhenSomethingCouldHaveKnown();
    nothingIsClaimedWhenNothingCouldHaveKnown();
    templatesAreLookedUpUnderTheTemplatePath();
    eachTypeIsTriedAgainstItsOwnExtensions();
    theExplanationSaysWhichSourcesLooked();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}
