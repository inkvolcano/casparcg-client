// Which build is this, which build is published, and is that one newer.
//
// The client could never answer any of those. The arithmetic that now does lives
// away from the network so it can be tested without one, and every rule here is a
// way this has gone wrong in other software: a string comparison that calls 2.3.10
// older than 2.3.9, a tag nobody could parse reading as version zero and making a
// current machine look ancient, a checksums file handed to the wrong asset.

#include "../src/Common/ClientRelease.h"

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

static void everyShapeATagHasTakenParses()
{
    ClientRelease::Version upstream = ClientRelease::parse("v2.3.1");
    expectTrue(upstream.valid, "upstream's own shape parses");
    sameInt(upstream.major, 2, "major");
    sameInt(upstream.minor, 3, "minor");
    sameInt(upstream.revision, 1, "revision");
    sameInt(upstream.build, 0, "and no build, because an upstream tag carries none");

    ClientRelease::Version bare = ClientRelease::parse("2.3.1");
    expectTrue(bare.valid && bare.revision == 1, "the same without the v");

    ClientRelease::Version forked = ClientRelease::parse("v2.3.1-206");
    sameInt(forked.build, 206, "a fork tag carries the build");

    ClientRelease::Version spelled = ClientRelease::parse("v2.3.1-build206");
    sameInt(spelled.build, 206, "spelled out, too");

    // What the title bar says, so somebody can paste it into a bug report and have
    // it understood.
    ClientRelease::Version titleBar = ClientRelease::parse("2.3.1 build 206");
    sameInt(titleBar.build, 206, "and the form the window title uses");
    sameInt(titleBar.major, 2, "without the build being read as the major version");
}

static void aBuildOnlyTagIsABuildNotAMajorVersion()
{
    // A fork release that moved nothing but the build. Read as a major version this
    // would be 206.0.0 - newer than everything, forever.
    ClientRelease::Version buildOnly = ClientRelease::parse("build-206");
    expectTrue(buildOnly.valid, "a build-only tag parses");
    sameInt(buildOnly.build, 206, "as a build");
    sameInt(buildOnly.major, 0, "and not as a major version");

    ClientRelease::Version running = ClientRelease::parse("2.3.1 build 206");
    expectTrue(!ClientRelease::isNewer(buildOnly, running),
               "so it does not read as newer than the build it names");
}

static void anUnreadableTagIsNeverNewer()
{
    // The failure that matters most. A tag nobody can parse must not read as
    // version zero either - that would make every client think it was ahead - nor
    // as anything that offers an update nobody meant to publish.
    ClientRelease::Version nonsense = ClientRelease::parse("nightly");
    expectTrue(!nonsense.valid, "a tag with no numbers in it is not a version");

    ClientRelease::Version running = ClientRelease::parse("2.3.1 build 206");
    expectTrue(!ClientRelease::isNewer(nonsense, running), "and is never newer");

    expectTrue(!ClientRelease::isNewer(ClientRelease::parse(""), running),
               "nor is an empty tag");
    expectTrue(!ClientRelease::isNewer(ClientRelease::parse("   "), running),
               "nor a blank one");

    // And nothing is newer than a running version we could not read either, because
    // the honest answer there is "no idea" rather than "yes, update".
    expectTrue(!ClientRelease::isNewer(ClientRelease::parse("v9.9.9"), ClientRelease::parse("")),
               "an unknown running version is never behind");
}

static void theComparisonIsNumericNotAlphabetical()
{
    ClientRelease::Version nine = ClientRelease::parse("2.3.9");
    ClientRelease::Version ten = ClientRelease::parse("2.3.10");

    expectTrue(ClientRelease::isNewer(ten, nine), "2.3.10 is newer than 2.3.9");
    expectTrue(!ClientRelease::isNewer(nine, ten), "and 2.3.9 is not newer than 2.3.10");

    // The same trap one component along, which is the one this fork will actually
    // hit: build 99 must not outrank build 100.
    ClientRelease::Version b99 = ClientRelease::parse("2.3.1 build 99");
    ClientRelease::Version b100 = ClientRelease::parse("2.3.1 build 100");

    expectTrue(ClientRelease::isNewer(b100, b99), "build 100 is newer than build 99");
    expectTrue(!ClientRelease::isNewer(b99, b100), "and not the other way about");
}

static void theBuildDecidesWhenNothingElseMoved()
{
    // The ordinary case for this fork: the semantic version has not moved in years
    // and the build has moved two hundred times. A comparison that stopped at the
    // revision would answer "no update" forever.
    ClientRelease::Version running = ClientRelease::parse("2.3.1 build 206");
    ClientRelease::Version published = ClientRelease::parse("v2.3.1-210");

    expectTrue(ClientRelease::isNewer(published, running), "a newer build alone is newer");
    expectTrue(!ClientRelease::isNewer(running, published), "and the older one is not");
    expectTrue(!ClientRelease::isNewer(running, running), "and a build is not newer than itself");

    // An upstream tag with no build must not look newer than a fork build of the
    // same version, or every venue would be offered a downgrade.
    expectTrue(!ClientRelease::isNewer(ClientRelease::parse("v2.3.1"), running),
               "an upstream tag of the same version is not an update");

    // But a genuinely newer upstream version is.
    expectTrue(ClientRelease::isNewer(ClientRelease::parse("v2.4.0"), running),
               "a newer upstream version is");
}

static void eachPlatformGetsItsOwnPackage()
{
    // The real asset list from upstream's v2.3.1 release.
    const QStringList assets = QStringList()
        << "casparcg-client-v2.3.1-macos-arm64.dmg"
        << "casparcg-client-v2.3.1-macos-x86_64.dmg"
        << "casparcg-client-v2.3.1-ubuntu22.deb"
        << "casparcg-client-v2.3.1-windows.zip"
        << "SHA256SUMS.txt";

    same(ClientRelease::assetFor("windows", assets),
         "casparcg-client-v2.3.1-windows.zip", "Windows takes the zip");
    same(ClientRelease::assetFor("linux", assets),
         "casparcg-client-v2.3.1-ubuntu22.deb", "Linux takes the deb");
    same(ClientRelease::assetFor("macos-arm64", assets),
         "casparcg-client-v2.3.1-macos-arm64.dmg", "an arm Mac takes the arm build");
    same(ClientRelease::assetFor("macos-x86_64", assets),
         "casparcg-client-v2.3.1-macos-x86_64.dmg", "and an intel Mac the intel one");

    // Handing a Mac the wrong architecture is a package that will not open; doing
    // it silently is worse than finding nothing.
    expectTrue(ClientRelease::assetFor("macos-arm64", QStringList()
                   << "casparcg-client-v2.3.1-macos-x86_64.dmg").isEmpty(),
               "an arm Mac is given nothing rather than the intel build");

    // The verifier is never the thing verified.
    expectTrue(ClientRelease::assetFor("windows", QStringList() << "SHA256SUMS.txt").isEmpty(),
               "the sums file is never chosen as a package");

    expectTrue(ClientRelease::assetFor("windows", QStringList()).isEmpty(),
               "a release with no assets offers nothing");
}

static void theChecksumIsReadForTheRightFile()
{
    const QString sums =
        "# generated by the release workflow\n"
        "aaaa111122223333444455556666777788889999aaaabbbbccccddddeeeeffff  "
        "casparcg-client-v2.3.1-macos-arm64.dmg\n"
        "1111222233334444555566667777888899990000aaaabbbbccccddddeeeeffff  "
        "casparcg-client-v2.3.1-windows.zip\n";

    same(ClientRelease::sha256For("casparcg-client-v2.3.1-windows.zip", sums),
         "1111222233334444555566667777888899990000aaaabbbbccccddddeeeeffff",
         "the hash for the asset being downloaded");

    expectTrue(ClientRelease::sha256For("casparcg-client-v2.3.1-linux.deb", sums).isEmpty(),
               "and nothing for an asset the file does not mention");
}

static void aMalformedSumsFileVerifiesNothing()
{
    // Every one of these has to come back empty rather than come back with
    // something that cannot match. The caller refuses a download it has no hash
    // for, so empty is the safe answer and a wrong-length string is not.
    expectTrue(ClientRelease::sha256For("app.zip", "deadbeef  app.zip").isEmpty(),
               "a hash that is too short is not a hash");
    expectTrue(ClientRelease::sha256For("app.zip",
                   QString(64, 'z') + "  app.zip").isEmpty(),
               "nor is one that is the right length but not hexadecimal");
    expectTrue(ClientRelease::sha256For("app.zip", "").isEmpty(), "an empty file has no hashes");
    expectTrue(ClientRelease::sha256For("app.zip", "app.zip").isEmpty(),
               "and a line with no hash on it is not one");
}

static void theSumsFileIsReadTheWayCoreutilsWritesIt()
{
    const QString hash(64, '1');

    // Binary mode marks the name with a star.
    same(ClientRelease::sha256For("app.zip", hash + " *app.zip"), hash,
         "the star of binary mode is not part of the name");

    // Written from a directory, the name carries a path this must not trip on.
    same(ClientRelease::sha256For("app.zip", hash + "  ./dist/app.zip"), hash,
         "a leading path is ignored");
    same(ClientRelease::sha256For("app.zip", hash + "  dist\\app.zip"), hash,
         "including a Windows one");

    // Upper case in, lower case out, so a comparison cannot fail on capitalisation.
    same(ClientRelease::sha256For("app.zip", QString(64, 'A') + "  app.zip"),
         QString(64, 'a'), "a hash is returned lower case");
}

static void aVersionCanBeShownBack()
{
    same(ClientRelease::describe(ClientRelease::parse("v2.3.1-206")), "2.3.1 build 206",
         "a full version reads as the title bar does");
    same(ClientRelease::describe(ClientRelease::parse("v2.3.1")), "2.3.1",
         "one with no build does not invent one");
    same(ClientRelease::describe(ClientRelease::parse("build-206")), "build 206",
         "and a build-only release says so");
    same(ClientRelease::describe(ClientRelease::parse("nightly")), "unknown",
         "something unreadable says so rather than showing zeroes");
}

static void buildsComeFromOneKnownPlace()
{
    // The reason this is a list in the binary rather than a text field: a build is
    // a program that runs on the playout machine, so its source is a
    // code-execution channel. A field anyone could edit would let a venue be
    // repointed at any repository at all.
    expectTrue(!ClientRelease::allowedSources().isEmpty(), "there is somewhere to fetch from");
    expectTrue(ClientRelease::isAllowedSource(ClientRelease::defaultSource()),
               "and the default is itself allowed, or nothing could ever be fetched");

    expectTrue(ClientRelease::isAllowedSource("inkvolcano/casparcg-builds"),
               "the builds repository is allowed");

    // The ones that matter. Each of these is somebody being pointed at a program
    // written by someone else.
    expectTrue(!ClientRelease::isAllowedSource("attacker/casparcg-builds"),
               "the same repository name under another owner is not");
    expectTrue(!ClientRelease::isAllowedSource("inkvolcano/casparcg-client"),
               "nor is another repository of the same owner");
    expectTrue(!ClientRelease::isAllowedSource(""), "nor is nothing at all");
    expectTrue(!ClientRelease::isAllowedSource("   "), "nor blank space");

    // A near miss must not pass. Trailing text after the repository name is how a
    // check that used startsWith rather than equality gets fooled.
    expectTrue(!ClientRelease::isAllowedSource("inkvolcano/casparcg-builds-evil"),
               "and neither does a name that merely starts the same way");
    expectTrue(!ClientRelease::isAllowedSource("evil.com/inkvolcano/casparcg-builds"),
               "nor one with the allowed name buried inside it");
}

static void aSourceIsReadHoweverItWasWritten()
{
    same(ClientRelease::normaliseSource("github:inkvolcano/casparcg-builds"),
         "inkvolcano/casparcg-builds", "the github: prefix is stripped");
    same(ClientRelease::normaliseSource("  inkvolcano/casparcg-builds  "),
         "inkvolcano/casparcg-builds", "and surrounding space");
    same(ClientRelease::normaliseSource("github:inkvolcano/casparcg-builds@main"),
         "inkvolcano/casparcg-builds", "a branch means nothing to a release");
    same(ClientRelease::normaliseSource("inkvolcano/casparcg-builds/"),
         "inkvolcano/casparcg-builds", "and a trailing slash is not part of the name");

    // GitHub does not care about case in an owner or a repository, so refusing one
    // that differs only in capitalisation would be a puzzle rather than a defence.
    expectTrue(ClientRelease::isAllowedSource("InkVolcano/CasparCG-Builds"),
               "capitalisation does not decide whether a source is allowed");
    expectTrue(ClientRelease::isAllowedSource("github:inkvolcano/casparcg-builds@main"),
               "and a fully written one is still allowed");
}

static void thisBuildKnowsWhatItIs()
{
    // Whatever it was compiled for, it must be one of the four the assets use, or
    // assetFor can never match anything.
    const QString platform = ClientRelease::platformKey();
    const QStringList known = QStringList()
        << "windows" << "linux" << "macos-arm64" << "macos-x86_64";

    expectTrue(known.contains(platform),
               QString("the compiled platform key \"%1\" is one the assets use").arg(platform));
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "Client release\n";

    everyShapeATagHasTakenParses();
    aBuildOnlyTagIsABuildNotAMajorVersion();
    anUnreadableTagIsNeverNewer();
    theComparisonIsNumericNotAlphabetical();
    theBuildDecidesWhenNothingElseMoved();
    eachPlatformGetsItsOwnPackage();
    theChecksumIsReadForTheRightFile();
    aMalformedSumsFileVerifiesNothing();
    theSumsFileIsReadTheWayCoreutilsWritesIt();
    aVersionCanBeShownBack();
    buildsComeFromOneKnownPlace();
    aSourceIsReadHoweverItWasWritten();
    thisBuildKnowsWhatItIs();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}
