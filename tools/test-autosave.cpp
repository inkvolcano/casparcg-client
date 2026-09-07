// Naming a recovery copy, and reading back what it was a copy of.
//
// Auto-save writes a file per rundown with unsaved changes into one folder. Two
// strings decide where it lands and what it means, and both come from outside the
// program: the path the rundown was opened from, and the first line of a file
// found on disk at the next launch.
//
// The path is the dangerous one. It becomes a filename, so anything that survives
// into it can walk out of the recovery folder — and a rundown can legitimately be
// opened from anywhere, including a share, a UNC path, or somewhere with a name
// nobody sane would choose. The rule is a whitelist rather than a strip list,
// because a strip list only ever removes what somebody thought of.
//
// The marker is the awkward one. It has to survive a path containing the "-->"
// that ends an XML comment, a newline, and non-ASCII characters, which is why it
// is percent-encoded rather than written raw.

#include "../src/Common/AutoSaveNaming.h"

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>

static int failures = 0;
static int checks = 0;

static void same(const QString& actual, const QString& wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what
                        << "  (wanted \"" << wanted << "\", got \"" << actual << "\")\n";
}

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

static void namesARundownAfterItsFile()
{
    same(AutoSaveNaming::stemFor("C:/shows/Tonight.xml"), "Tonight",
         "an ordinary Windows path keeps its name");
    same(AutoSaveNaming::stemFor("/home/op/late show.xml"), "late show",
         "a space in a name is kept, because a filename may hold one");
    same(AutoSaveNaming::stemFor("C:/shows/Show-2_final.xml"), "Show-2_final",
         "dashes and underscores are kept");
    same(AutoSaveNaming::stemFor(""), "Untitled",
         "a rundown with no file gets a name of its own");
}

static void aStemCanNeverLeaveTheFolder()
{
    // Each of these is something a path could actually be. The question is not
    // whether the name looks tidy afterwards, it is whether joining it to the
    // recovery folder can ever land outside the recovery folder.
    QStringList paths;
    paths << "../../evil.xml"
          << "..\\..\\evil.xml"
          << "C:/Windows/System32/evil.xml"
          << "/etc/passwd"
          << "....//....//x.xml"
          << "a:b:c.xml"
          << "%2e%2e%2fevil.xml"
          << "rundown.xml\0hidden.exe"
          << "  ..  "
          << "....";

    foreach (const QString& path, paths)
    {
        QString stem = AutoSaveNaming::stemFor(path);

        expectTrue(!stem.contains('/'),
                   QString("no slash survives \"%1\" (got \"%2\")").arg(path, stem));
        expectTrue(!stem.contains('\\'),
                   QString("no backslash survives \"%1\" (got \"%2\")").arg(path, stem));
        expectTrue(!stem.contains(':'),
                   QString("no colon survives \"%1\" (got \"%2\")").arg(path, stem));
        expectTrue(!stem.contains('.'),
                   QString("no dot survives \"%1\" (got \"%2\")").arg(path, stem));
        expectTrue(!stem.contains('%'),
                   QString("no percent survives \"%1\" (got \"%2\")").arg(path, stem));
        expectTrue(!stem.isEmpty(),
                   QString("\"%1\" still produced a usable name").arg(path));

        // The claim that actually matters, stated the way the writer uses it.
        QString joined = QDir::cleanPath(QString("/recovery/%1.xml").arg(stem));
        expectTrue(joined.startsWith("/recovery/") && joined.count('/') == 2,
                   QString("\"%1\" stayed inside the folder (landed at \"%2\")").arg(path, joined));
    }
}

static void aPathOfNothingUsableStillGetsAName()
{
    // completeBaseName() of "...." is empty and every character of ".." is
    // rejected, so without the fallback these would produce an empty filename and
    // the write would fail rather than land anywhere. Nothing is lost by naming
    // them, because the real path travels inside the file.
    expectTrue(!AutoSaveNaming::stemFor("C:/shows/....xml").isEmpty(),
               "a name of only dots still gets a filename");
    expectTrue(!AutoSaveNaming::stemFor("///").isEmpty(),
               "a path of only separators still gets a filename");
    expectTrue(!AutoSaveNaming::stemFor("!!!.xml").isEmpty(),
               "a name of only punctuation still gets a filename");
}

static QByteArray markerLineFor(const QString& original)
{
    QByteArray line;
    line.append(AutoSaveNaming::marker());
    line.append(original.toUtf8().toPercentEncoding());
    line.append(" -->");

    return line;
}

static void theOriginalPathSurvivesTheRoundTrip()
{
    QStringList paths;
    paths << "C:/shows/Tonight.xml"
          << "/home/op/late show.xml"
          << "\\\\SERVER\\CasparCG\\rundowns\\Show.xml"
          << QString::fromUtf8("C:/shows/late \xc3\xa9" "dition/Tonight.xml")
          << "C:/shows/weird -- name.xml";

    foreach (const QString& original, paths)
    {
        same(AutoSaveNaming::originalPathFromLine(markerLineFor(original)), original,
             QString("\"%1\" came back unchanged").arg(original));
    }
}

static void aPathThatCouldEndTheCommentEarlyDoesNot()
{
    // "-->" inside the path is the case that breaks a marker written raw: the
    // parser would stop at the first one and hand back a truncated path, and the
    // XML after it would be inside the comment. Percent-encoding is why it does
    // not, and this is the test that would fail if that encoding were dropped.
    QString original = "C:/shows/a-->b/Tonight.xml";
    QByteArray line = markerLineFor(original);

    expectTrue(!line.contains("-->b"),
               "the encoded marker does not carry a raw \"-->\" from the path");
    same(AutoSaveNaming::originalPathFromLine(line), original,
         "a path containing \"-->\" survives the round trip");

    // A newline in a path would otherwise push the rest onto a second line, and
    // the reader only ever reads the first one.
    QString withNewline = "C:/shows/one\ntwo.xml";
    QByteArray newlineLine = markerLineFor(withNewline);
    expectTrue(!newlineLine.contains('\n'),
               "the encoded marker is a single line even for a path containing one");
    same(AutoSaveNaming::originalPathFromLine(newlineLine), withNewline,
         "a path containing a newline survives the round trip");
}

static void anUnsavedRundownIsToldApartFromANonMarker()
{
    // These two are different answers and the restore prompt acts on the
    // difference: one says "this rundown was never saved", the other says "this
    // file is not a recovery copy at all".
    same(AutoSaveNaming::originalPathFromLine(markerLineFor("")), QString(""),
         "a rundown that never had a file reports an empty path");
    expectTrue(!AutoSaveNaming::originalPathFromLine(markerLineFor("")).isNull(),
               "an empty original path is empty but not null");

    expectTrue(AutoSaveNaming::originalPathFromLine("<?xml version=\"1.0\"?>").isNull(),
               "a plain rundown is not mistaken for a recovery copy");
    expectTrue(AutoSaveNaming::originalPathFromLine("").isNull(),
               "an empty line is not mistaken for a recovery copy");
    expectTrue(AutoSaveNaming::originalPathFromLine("<!-- something else -->").isNull(),
               "another comment is not mistaken for a recovery copy");
}

static void atruncatedMarkerIsRefusedRatherThanGuessed()
{
    // A crash mid-write can leave a partial line. Better a null answer, which the
    // caller treats as "not a recovery copy", than a half-decoded path.
    expectTrue(AutoSaveNaming::originalPathFromLine(AutoSaveNaming::marker()).isNull(),
               "a marker with nothing after it is refused");
    expectTrue(AutoSaveNaming::originalPathFromLine(AutoSaveNaming::marker() + "C%3A%2Fshows").isNull(),
               "a marker with no terminator is refused");
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "Auto-save naming\n";

    namesARundownAfterItsFile();
    aStemCanNeverLeaveTheFolder();
    aPathOfNothingUsableStillGetsAName();
    theOriginalPathSurvivesTheRoundTrip();
    aPathThatCouldEndTheCommentEarlyDoesNot();
    anUnsavedRundownIsToldApartFromANonMarker();
    atruncatedMarkerIsRefusedRatherThanGuessed();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}
