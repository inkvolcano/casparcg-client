#include <QtTest>

// The widgets delegate these rules to this header, and including them here
// would pull in their uic-generated ui_*.h, which this target does not build.
#include "AutoSaveNaming.h"

#include <QtCore/QDir>
#include <QtCore/QFile>

class AutoSaveTest : public QObject
{
    Q_OBJECT

private:
    // The file-reading half of RundownWidget::autoSaveOriginalPath, which is all
    // that method does before handing the line to AutoSaveNaming.
    QString firstLineOriginal(const QString& path)
    {
        QFile file(path);
        if (!file.open(QFile::ReadOnly))
            return QString();

        QByteArray firstLine = file.readLine(4096);
        file.close();

        return AutoSaveNaming::originalPathFromLine(firstLine);
    }

    // Writes a file shaped like a real recovery copy: the marker line the writer
    // puts in front, then the rundown.
    QString writeAutoSaveFile(const QString& path, const QString& originalPath)
    {
        QFile file(path);
        if (!file.open(QFile::WriteOnly | QFile::Truncate))
            return QString();

        QByteArray payload;
        payload.append("<!-- casparcg-client autosave of: ");
        payload.append(originalPath.toUtf8().toPercentEncoding());
        payload.append(" -->\n");
        payload.append("<?xml version=\"1.0\"?><items></items>");

        file.write(payload);
        file.close();

        return path;
    }

private slots:
    void anUnsavedRundownGetsANameOfItsOwn()
    {
        QCOMPARE(AutoSaveNaming::stemFor(""), QString("Untitled"));
        // The widget maps its own "no file yet" value onto an empty string before
        // calling this; the naming rule itself only ever sees the empty case.
        QCOMPARE(AutoSaveNaming::stemFor(QString()), QString("Untitled"));
    }

    void anOrdinaryPathKeepsItsName()
    {
        QCOMPARE(AutoSaveNaming::stemFor("C:/shows/Tonight.xml"), QString("Tonight"));
        QCOMPARE(AutoSaveNaming::stemFor("/home/op/late show.xml"), QString("late show"));
        QCOMPARE(AutoSaveNaming::stemFor("C:/shows/Show-2_final.xml"), QString("Show-2_final"));
    }

    void aStemCanNeverLeaveTheRecoveryFolder()
    {
        // Whatever a rundown was opened from ends up here, so the answer has to be
        // unusable as a path: no separators, no dots, no drive letter, no colon.
        QStringList nasty;
        nasty << "../../evil.xml"
              << "C:/Windows/System32/evil.xml"
              << "..\\..\\evil.xml"
              << "/etc/passwd"
              << "....//....//x.xml"
              << "con.xml"
              << "a:b:c.xml";

        foreach (const QString& path, nasty)
        {
            QString stem = AutoSaveNaming::stemFor(path);

            QVERIFY2(!stem.contains('/'), qPrintable(QString("'%1' -> '%2' kept a slash").arg(path, stem)));
            QVERIFY2(!stem.contains('\\'), qPrintable(QString("'%1' -> '%2' kept a backslash").arg(path, stem)));
            QVERIFY2(!stem.contains(':'), qPrintable(QString("'%1' -> '%2' kept a colon").arg(path, stem)));
            QVERIFY2(!stem.contains('.'), qPrintable(QString("'%1' -> '%2' kept a dot").arg(path, stem)));
            QVERIFY2(!stem.isEmpty(), qPrintable(QString("'%1' produced an empty stem").arg(path)));

            // The proof that matters: joining it to a directory stays inside it.
            QString joined = QDir::cleanPath(QString("/recovery/%1.xml").arg(stem));
            QVERIFY2(joined.startsWith("/recovery/"),
                     qPrintable(QString("'%1' escaped to '%2'").arg(path, joined)));
        }
    }

    void aStemOfNothingButPunctuationStillGetsAName()
    {
        // completeBaseName() of "..." is empty, and an empty filename would make
        // the write fail rather than land somewhere odd. It gets a name instead.
        QVERIFY(!AutoSaveNaming::stemFor("C:/shows/....xml").isEmpty());
        QVERIFY(!AutoSaveNaming::stemFor("///").isEmpty());
    }

    void theOriginalPathSurvivesTheMarker()
    {
        QString directory = QDir::tempPath() + "/casparcg-autosave-test";
        QDir().mkpath(directory);

        // A path with the things that would break a naive marker: spaces, a
        // non-ASCII character, and the "-->" the comment ends with.
        QString original = QString::fromUtf8("C:/shows/late \xc3\xa9" "dition/Tonight.xml");
        QString path = writeAutoSaveFile(directory + "/Tonight.xml", original);
        QVERIFY(!path.isEmpty());

        QCOMPARE(firstLineOriginal(path), original);

        QFile::remove(path);
        QDir().rmdir(directory);
    }

    void aRundownThatNeverHadAFileReportsNoOriginal()
    {
        QString directory = QDir::tempPath() + "/casparcg-autosave-test";
        QDir().mkpath(directory);

        QString path = writeAutoSaveFile(directory + "/Untitled.xml", "");
        QVERIFY(!path.isEmpty());

        // Empty, not garbage: the restore prompt uses this to decide whether to
        // name a file or say the rundown was never saved.
        QCOMPARE(firstLineOriginal(path), QString(""));

        QFile::remove(path);
        QDir().rmdir(directory);
    }

    void aFileWithoutTheMarkerIsNotMistakenForOne()
    {
        QString directory = QDir::tempPath() + "/casparcg-autosave-test";
        QDir().mkpath(directory);

        QString path = directory + "/Plain.xml";
        QFile file(path);
        QVERIFY(file.open(QFile::WriteOnly | QFile::Truncate));
        file.write("<?xml version=\"1.0\"?><items></items>");
        file.close();

        QCOMPARE(firstLineOriginal(path), QString());

        QFile::remove(path);
        QDir().rmdir(directory);
    }

    void aMissingFileIsNotACrash()
    {
        QCOMPARE(firstLineOriginal(QDir::tempPath() + "/no-such-autosave.xml"),
                 QString());
    }

};

int runAutoSaveTest(int argc, char* argv[])
{
    AutoSaveTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "AutoSaveTest.moc"
