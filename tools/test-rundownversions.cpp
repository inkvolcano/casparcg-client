// A rundown's last saves, kept beside it: naming, recognising, keeping ten.

#include "../src/Common/RundownVersions.h"

#include <QtCore/QTextStream>

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

using namespace RundownVersions;

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Rundown versions\n";

    const QString rundown = "D:/Shows/Late Show.xml";
    const QDateTime when(QDate(2026, 11, 16), QTime(14, 32, 5));

    expectTrue(folderFor(rundown) == "D:/Shows/Late Show.xml versions", "the folder sits beside the rundown, named after it");
    expectTrue(versionName(rundown, when) == "Late Show 2026-11-16 143205.xml", "a version is named by the rundown and the second it was kept");
    expectTrue(versionTime(rundown, versionName(rundown, when)) == when, "and the time reads back from the name");
    expectTrue(!versionTime(rundown, "Late Show notes.xml").isValid() && !versionTime(rundown, "Other 2026-11-16 143205.xml").isValid()
                   && !versionTime(rundown, "Late Show 2026-11-16 143205.txt").isValid(),
               "anything else in the folder is not a version");

    QStringList names;
    for (int i = 0; i < 13; i++)
        names << versionName(rundown, when.addSecs(-60 * i));
    names << "Late Show notes.xml" << "readme.txt";

    const QStringList versions = versionsAmong(rundown, names);
    expectTrue(versions.count() == 13 && versions.first() == versionName(rundown, when), "the versions, newest first");

    const QStringList gone = beyondKept(rundown, names);
    expectTrue(gone.count() == 3 && gone.last() == versionName(rundown, when.addSecs(-60 * 12)),
               "past the newest ten, the oldest three go");
    expectTrue(!gone.contains("Late Show notes.xml") && !gone.contains("readme.txt"), "and nothing that is not a version");
    expectTrue(beyondKept(rundown, names.mid(0, 4)).isEmpty(), "four versions: none go");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
