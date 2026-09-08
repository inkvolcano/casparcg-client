#include <QtTest>

#include "Rundown/RundownTreeWidget.h"
#include "Rundown/RundownWidget.h"

#include <QtCore/QDir>
#include <QtCore/QFile>

class AutoSaveTest : public QObject
{
    Q_OBJECT

private:
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
        QCOMPARE(RundownTreeWidget::autoSaveStemFor(""), QString("Untitled"));
        QCOMPARE(RundownTreeWidget::autoSaveStemFor(Rundown::DEFAULT_NAME), QString("Untitled"));
    }

    void anOrdinaryPathKeepsItsName()
    {
        QCOMPARE(RundownTreeWidget::autoSaveStemFor("C:/shows/Tonight.xml"), QString("Tonight"));
        QCOMPARE(RundownTreeWidget::autoSaveStemFor("/home/op/late show.xml"), QString("late show"));
        QCOMPARE(RundownTreeWidget::autoSaveStemFor("C:/shows/Show-2_final.xml"), QString("Show-2_final"));
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
            QString stem = RundownTreeWidget::autoSaveStemFor(path);

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
        QVERIFY(!RundownTreeWidget::autoSaveStemFor("C:/shows/....xml").isEmpty());
        QVERIFY(!RundownTreeWidget::autoSaveStemFor("///").isEmpty());
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

        QCOMPARE(RundownWidget::autoSaveOriginalPath(path), original);

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
        QCOMPARE(RundownWidget::autoSaveOriginalPath(path), QString(""));

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

        QCOMPARE(RundownWidget::autoSaveOriginalPath(path), QString());

        QFile::remove(path);
        QDir().rmdir(directory);
    }

    void aMissingFileIsNotACrash()
    {
        QCOMPARE(RundownWidget::autoSaveOriginalPath(QDir::tempPath() + "/no-such-autosave.xml"),
                 QString());
    }

    void theIntervalIsClampedToSomethingSurvivable()
    {
        // Read straight from the database, so a hand-edited or absent value must
        // still land somewhere that neither hammers the disk nor never fires.
        int minutes = RundownWidget::autoSaveMinutes();

        QVERIFY2(minutes >= 1 && minutes <= 60,
                 qPrintable(QString("autoSaveMinutes() = %1").arg(minutes)));
    }
};

int runAutoSaveTest(int argc, char* argv[])
{
    AutoSaveTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "AutoSaveTest.moc"
