// Watch folders: what is in the folder, what is new to the group, what has gone.

#include "../src/Common/WatchFolderRules.h"

#include <QtCore/QTextStream>

static int checks = 0;
static int failures = 0;

static void expect(const QString& actual, const QString& expected, const QString& what)
{
    checks++;
    if (actual == expected)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n        got      " << actual << "\n        expected " << expected << "\n";
}

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

using namespace WatchFolderRules;

static Media media(const QString& name, const QString& type = "MOVIE")
{
    Media m;
    m.name = name;
    m.type = type;
    return m;
}

static QString names(const QList<Media>& list)
{
    QStringList out;
    for (const Media& m : list)
        out << m.name;
    return out.join(",");
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Watch folders\n";

    expect(normaliseFolder(" incoming\\ "), "INCOMING", "a folder however it was typed");
    expect(normaliseFolder("/Incoming/Today/"), "INCOMING/TODAY", "leading and trailing slashes go, nesting stays");
    expect(normaliseFolder(""), "", "no folder");

    expectTrue(inFolder("INCOMING/CLIP", "incoming"), "a file in the folder");
    expectTrue(inFolder("INCOMING/TODAY/CLIP", "INCOMING"), "and in a folder inside it");
    expectTrue(!inFolder("INCOMINGS/CLIP", "INCOMING"), "not a folder that only starts the same");
    expectTrue(!inFolder("CLIP", "INCOMING"), "not a file outside it");
    expectTrue(!inFolder("INCOMING/CLIP", ""), "no folder watches nothing (not the whole server)");

    const QList<Media> all = { media("INCOMING/B_CLIP"), media("INCOMING/A_STILL", "STILL"), media("INCOMING/MUSIC", "AUDIO"),
                               media("OTHER/CLIP"), media("INCOMING/WEIRD", "UNKNOWN"), media("incoming/c_clip") };
    expect(names(fresh(all, "INCOMING", QStringList())), "INCOMING/A_STILL,INCOMING/B_CLIP,incoming/c_clip,INCOMING/MUSIC",
           "everything playable in the folder, in name order; other folders and unknown types left out");
    expect(names(fresh(all, "INCOMING", QStringList() << "INCOMING/B_CLIP" << "INCOMING/A_STILL")), "incoming/c_clip,INCOMING/MUSIC",
           "what the group had before is not added again - deleted by the operator included");
    expect(names(fresh(all, "INCOMING", QStringList() << "INCOMING/C_CLIP")), "INCOMING/A_STILL,INCOMING/B_CLIP,INCOMING/MUSIC",
           "seen without regard to case");
    expect(names(fresh(QList<Media>() << media("INCOMING/X") << media("INCOMING/X"), "INCOMING", QStringList())), "INCOMING/X",
           "a name listed twice is added once");
    expectTrue(fresh(all, "NOWHERE", QStringList()).isEmpty(), "an empty or missing folder adds nothing");

    expect(gone(all, "INCOMING", QStringList() << "INCOMING/B_CLIP" << "INCOMING/DELETED" << "ELSEWHERE/GONE").join(","), "INCOMING/DELETED",
           "items whose file left the folder; items from elsewhere are not this watch's business");

    expect(foldersOf(QStringList() << "A/B/CLIP" << "A/OTHER" << "C/D" << "TOP").join(","), "A,A/B,C",
           "every folder the names show, nested ones too, sorted");

    const QStringList seen = QStringList() << "INCOMING/A" << "INCOMING/B";
    expect(splitSeen(joinSeen(seen)).join(","), "INCOMING/A,INCOMING/B", "what was seen survives a save and a load");
    expect(splitSeen("incoming/a\n\nINCOMING/A\n ").join(","), "INCOMING/A", "blank lines and repeats dropped, capitals kept");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
