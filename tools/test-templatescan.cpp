// What a template selection reads. The Inspector learns a template's functions
// by scanning its HTML, and did so on every selection; a 28 MB self-contained
// page took twenty seconds per click. Three rules keep that from happening:
// the file is read the fast way (QTextStream::readAll was the twenty seconds),
// a size above which a selection does not scan, and a cache so a scanned file
// is not read again while unchanged. All live in TemplateScan.h and are
// checked here on real files in a temporary folder.

#include "../src/Common/TemplateScan.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTextStream>
#include <QtCore/QThread>

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

static void writeBytes(const QString& path, qint64 bytes)
{
    QFile file(path);
    file.open(QIODevice::WriteOnly);
    file.write(QByteArray(int(bytes), 'x'));
    file.close();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    const qint64 MB = 1024 * 1024;

    out << "The size gate\n";

    expectTrue(TemplateScan::scanOnSelection(60 * 1024), "a 60 KB template is scanned on selection");
    expectTrue(TemplateScan::scanOnSelection(TemplateScan::SCAN_ON_SELECTION_LIMIT), "a file exactly at the limit is scanned");
    expectTrue(!TemplateScan::scanOnSelection(TemplateScan::SCAN_ON_SELECTION_LIMIT + 1), "one byte over is not");
    expectTrue(TemplateScan::scanOnSelection(28 * MB), "a 28 MB page with its media embedded is, now that reading it is cheap");
    expectTrue(!TemplateScan::scanOnSelection(100 * MB), "a 100 MB file is not");
    expectTrue(TemplateScan::skippedNotice(28 * MB).contains("28.0 MB"), "the notice says how large it is");
    expectTrue(TemplateScan::skippedNotice(28 * MB).contains("Discover Functions"), "and names the button that scans anyway");

    out << "\nReading the file\n";

    QTemporaryDir sandbox;
    {
        // A UTF-8 byte-order mark, Windows line endings and a non-ASCII name:
        // the mark goes, the endings stay, the name survives the decoding.
        const QString textPath = QDir(sandbox.path()).filePath("crlf.html");
        QFile textFile(textPath);
        textFile.open(QIODevice::WriteOnly);
        textFile.write("\xEF\xBB\xBF<script>\r\nwindow.debugData = { \"f0\": \"S\xC3\xA9ville\" }\r\n</script>\r\n");
        textFile.close();

        QString content;
        expectTrue(TemplateScan::readText(textPath, &content), "a file that exists is read");
        expectTrue(!content.startsWith(QChar(0xFEFF)), "the byte-order mark is dropped");
        expectTrue(content.startsWith("<script>\r\n"), "line endings are kept as written");
        expectTrue(content.contains(QString::fromUtf8("S\xC3\xA9ville")), "UTF-8 is decoded");
        expectTrue(QRegularExpression("window\\.debugData\\s*=\\s*\\{([^}]*)\\}").match(content).hasMatch(),
                   "and the declaration readers still find their object");

        QString untouched = "before";
        expectTrue(!TemplateScan::readText(QDir(sandbox.path()).filePath("absent.html"), &untouched), "a missing file is false");

        // The point of it: a page the size of the rolling graphics, read in
        // well under a second. The old way took ten to twenty-four.
        const QString bigPath = QDir(sandbox.path()).filePath("big.html");
        writeBytes(bigPath, 28 * MB);
        QElapsedTimer clock;
        clock.start();
        expectTrue(TemplateScan::readText(bigPath, &content) && content.size() == 28 * MB, "a 28 MB file is read whole");
        const qint64 took = clock.elapsed();
        expectTrue(took < 3000, QString("and in under three seconds, not twenty: %1 ms").arg(took));
    }

    out << "\nThe once-per-file cache\n";

    const QString path = QDir(sandbox.path()).filePath("t.html");
    writeBytes(path, 1000);

    TemplateScan::Cache cache;
    QStringList found;

    expectTrue(!cache.lookup(path, QFileInfo(path), &found), "a file never scanned is a miss");

    cache.remember(path, QFileInfo(path), QStringList() << "showLower" << "hideLower");
    expectTrue(cache.lookup(path, QFileInfo(path), &found), "the same unchanged file is a hit");
    expectTrue(found == (QStringList() << "showLower" << "hideLower"), "and gives back what was found");
    expectTrue(cache.count() == 1, "one entry per file");

    writeBytes(path, 1001);
    expectTrue(!cache.lookup(path, QFileInfo(path), &found), "a file that grew is a miss, so it is scanned again");

    cache.remember(path, QFileInfo(path), QStringList() << "changed");
    QThread::msleep(1100);   // file times are whole seconds on some file systems
    writeBytes(path, 1001);  // same size, later time
    expectTrue(!cache.lookup(path, QFileInfo(path), &found), "the same size saved later is a miss too");

    expectTrue(!cache.lookup(QDir(sandbox.path()).filePath("other.html"), QFileInfo(path), &found),
               "a different path is its own entry");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
