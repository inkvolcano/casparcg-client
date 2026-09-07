// The other half of the path tests: installFile itself, writing to a real folder.
//
// test-paths.cpp proves the rules say no to the right things. This proves the
// function that acts on them does too, because between the rules and the disk sit
// a path join, a cleanPath, a prefix check and a QSaveFile, and any of those could
// undo the answer the rules gave.
//
// It writes into a temporary templates root and then checks that folder is the only
// place anything appeared. That last part is the real test: not "did the call return
// 400", but "is there a file anywhere it should not be".

#include "../src/Widgets/TemplateInstaller.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QMap>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTextStream>

static int failures = 0;
static int checks = 0;

static void expect(int actual, int wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what
                        << "  (expected " << wanted << ", got " << actual << ")\n";
}

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

static int install(const QString& pack, const QString& path,
                   const QByteArray& body, const QString& digest = QString())
{
    QString error;
    return TemplateInstaller::installFile(pack, path, body, &error, digest);
}

static QByteArray contentsOf(const QString& absolute)
{
    QFile file(absolute);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();

    QByteArray body = file.readAll();
    file.close();
    return body;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    // A sandbox with a marker file beside the templates root. Anything that escapes
    // the root lands next to that marker, where the sweep at the end will find it.
    QTemporaryDir sandbox;
    if (!sandbox.isValid())
    {
        out << "could not make a temporary folder\n";
        return 2;
    }

    QString root = QDir(sandbox.path()).filePath("templates");
    QDir().mkpath(root);
    QDir().mkpath(QDir(sandbox.path()).filePath("outside"));

    qputenv("CASPARCG_TEST_TemplatePushPath", root.toUtf8());

    out << "Installing files\n";
    expectTrue(TemplateInstaller::templatesRoot() == root, "the test root is in use");

    // ---- the ordinary cases ----
    expect(install("SEVILLE", "calendar.html", "<h1>one</h1>"), 200, "a plain file");
    expectTrue(contentsOf(QDir(root).filePath("SEVILLE/calendar.html")) == QByteArray("<h1>one</h1>"),
               "the bytes written are the bytes given");

    expect(install("SEVILLE", "css/site.css", "body{}"), 200, "a nested file");
    expectTrue(QFile::exists(QDir(root).filePath("SEVILLE/css/site.css")),
               "a missing folder is created");

    expect(install("SEVILLE", "css\\deep\\a.css", "x"), 200, "a windows-style path");
    expectTrue(QFile::exists(QDir(root).filePath("SEVILLE/css/deep/a.css")),
               "a windows-style path lands where a forward-slash one would");

    // Replacing a file leaves the new content and nothing of the old.
    expect(install("SEVILLE", "calendar.html", "<h1>two</h1>"), 200, "replacing a file");
    expectTrue(contentsOf(QDir(root).filePath("SEVILLE/calendar.html")) == QByteArray("<h1>two</h1>"),
               "a replaced file holds only the new bytes");

    // ---- the ones that must be refused ----
    expect(install("SEVILLE", "../escape.html", "x"), 400, "traversal in the path");
    expect(install("SEVILLE", "a/../../escape.html", "x"), 400, "traversal after a segment");
    expect(install("..", "escape.html", "x"), 400, "traversal in the pack name");
    expect(install("../outside", "escape.html", "x"), 400, "a pack name that climbs out");
    expect(install("SEVILLE", "/absolute.html", "x"), 400, "an absolute path");
    expect(install("SEVILLE", "C:/windows/evil.dll", "x"), 400, "a drive letter");
    expect(install("SEVILLE", "con.html", "x"), 400, "a device name");
    expect(install("SEVILLE", "a.html:hidden", "x"), 400, "an alternate data stream");
    expect(install("", "a.html", "x"), 400, "an empty pack name");
    expect(install("SEVILLE", "", "x"), 400, "an empty path");

    // ---- the files each client owns ----
    expect(install("SEVILLE", "project.js", "stolen"), 409, "project.js is refused");
    expect(install("SEVILLE", "EXTENSIONS.JSON", "stolen"), 409, "extensions.json is refused whatever the case");
    expectTrue(!QFile::exists(QDir(root).filePath("SEVILLE/project.js")),
               "a refused protected file is not written");

    // Deeper down the same name is template code and travels normally.
    expect(install("SEVILLE", "webcg/project.js", "code"), 200, "a nested project.js is ordinary");

    // ---- the digest, when one is given ----
    QByteArray body = "<h1>checked</h1>";
    QString good = TemplateInstaller::gitBlobSha(body);   // not the sha1 form: deliberately wrong
    expect(install("SEVILLE", "digest.html", body, good), 422,
           "a git blob digest is not the sha1 this expects");

    QString sha1 = QString::fromLatin1(
        QCryptographicHash::hash(body, QCryptographicHash::Sha1).toHex());
    expect(install("SEVILLE", "digest.html", body, sha1), 200, "the right digest is accepted");
    expect(install("SEVILLE", "digest.html", "different", sha1), 422, "a wrong digest is refused");
    expectTrue(contentsOf(QDir(root).filePath("SEVILLE/digest.html")) == body,
               "a refused write leaves the existing file alone");

    // ---- with nowhere configured, nothing is written anywhere ----
    qputenv("CASPARCG_TEST_TemplatePushPath", QByteArray());
    expect(install("SEVILLE", "nowhere.html", "x"), 500, "no templates folder configured");
    qputenv("CASPARCG_TEST_TemplatePushPath", root.toUtf8());

    // ---- the map every GitHub comparison is made against ----
    out << "\nPack digests\n";

    // This decides what a client fetches. If its keys do not look exactly like the
    // paths in a GitHub tree, nothing ever compares equal and every client
    // re-downloads every file on every poll, forever, without any error to show
    // for it. On Windows that is a plausible way to be wrong.
    QMap<QString, QString> gitStyle = TemplateInstaller::packDigests("SEVILLE", true);

    expectTrue(gitStyle.contains("calendar.html"), "a top-level file is keyed by its name");
    expectTrue(gitStyle.contains("css/site.css"), "a nested file is keyed with forward slashes");
    expectTrue(gitStyle.contains("css/deep/a.css"), "a deeper file too");

    foreach (const QString& key, gitStyle.keys())
        expectTrue(!key.contains('\\'), "no backslash in the key: " + key);

    // A GitHub tree gives Git blob hashes, so these have to be those and not sha1.
    expectTrue(gitStyle.value("calendar.html") == TemplateInstaller::gitBlobSha("<h1>two</h1>"),
               "a git-style digest is the git blob hash of the content");

    QMap<QString, QString> plain = TemplateInstaller::packDigests("SEVILLE", false);
    expectTrue(plain.value("calendar.html") == QString::fromLatin1(
                   QCryptographicHash::hash("<h1>two</h1>", QCryptographicHash::Sha1).toHex()),
               "a plain digest is the sha1 of the content");
    expectTrue(plain.value("calendar.html") != gitStyle.value("calendar.html"),
               "the two styles are not the same value");

    expectTrue(TemplateInstaller::packDigests("NOSUCHPACK", true).isEmpty(),
               "a pack that is not there gives nothing");
    expectTrue(TemplateInstaller::packDigests("../outside", true).isEmpty(),
               "a pack name that climbs out gives nothing");

    // The comparison the client actually performs, end to end: unchanged file reads
    // as unchanged, changed file reads as changed.
    expectTrue(gitStyle.value("calendar.html") == TemplateInstaller::gitBlobSha("<h1>two</h1>"),
               "an unchanged file compares equal");
    expectTrue(gitStyle.value("calendar.html") != TemplateInstaller::gitBlobSha("<h1>three</h1>"),
               "a changed file compares different");

    // ---- the question that actually matters ----
    out << "\nWhere did anything land\n";

    QStringList strays;
    QDirIterator sweep(sandbox.path(), QDir::Files, QDirIterator::Subdirectories);
    while (sweep.hasNext())
    {
        sweep.next();
        QString found = QDir::cleanPath(sweep.fileInfo().absoluteFilePath());
        if (!found.startsWith(QDir::cleanPath(root) + "/"))
            strays.append(found);
    }

    checks++;
    if (!strays.isEmpty())
    {
        failures++;
        out << "  FAIL  " << strays.count() << " file(s) landed outside the templates root:\n";
        foreach (const QString& stray, strays)
            out << "        " << stray << "\n";
    }

    // And everything that did land is where it was meant to be.
    QStringList wanted;
    wanted << "SEVILLE/calendar.html" << "SEVILLE/css/site.css" << "SEVILLE/css/deep/a.css"
           << "SEVILLE/digest.html" << "SEVILLE/webcg/project.js";

    QStringList actual;
    QDirIterator inside(root, QDir::Files, QDirIterator::Subdirectories);
    while (inside.hasNext())
    {
        inside.next();
        actual.append(QDir(root).relativeFilePath(inside.fileInfo().absoluteFilePath()));
    }

    wanted.sort();
    actual.sort();

    checks++;
    if (wanted != actual)
    {
        failures++;
        out << "  FAIL  the pack does not hold exactly what it should\n";
        out << "        expected: " << wanted.join(", ") << "\n";
        out << "        found:    " << actual.join(", ") << "\n";
    }

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    out.flush();

    return failures == 0 ? 0 : 1;
}
