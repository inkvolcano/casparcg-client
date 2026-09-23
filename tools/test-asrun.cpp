// The as-run log's rules: which server commands are on-air actions, how a line is
// written as CSV, and which daily files are old enough to go.

#include "../src/Common/AsRunLog.h"

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

static AsRun::Entry parse(const QString& line, bool* ok)
{
    AsRun::Entry entry;
    *ok = AsRun::fromCommand(line, entry);
    return entry;
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "As-run log\n";

    bool ok = false;
    AsRun::Entry e;

    expectTrue(AsRun::words("PLAY 1-10 \"MY CLIP\" MIX 25") == QStringList() << "PLAY" << "1-10" << "MY CLIP" << "MIX" << "25",
               "a quoted name stays one word");
    expectTrue(AsRun::words("CG 1-20 INVOKE 1 \"say \\\"hi\\\"\"").last() == "say \"hi\"", "escaped quotes are unescaped");

    e = parse("PLAY 1-10 \"AMB_OPENER\" MIX 25 LINEAR RIGHT", &ok);
    expectTrue(ok && e.action == "Play" && e.what == "AMB_OPENER" && e.channel == 1 && e.layer == 10, "a clip played");
    e = parse("PLAY 2-10", &ok);
    expectTrue(ok && e.action == "Play" && e.what.isEmpty(), "playing what was loaded");
    e = parse("PLAY 1-20 [HTML] \"http://x/y.html\" CUT 1 Linear RIGHT", &ok);
    expectTrue(ok && e.what == "http://x/y.html", "an HTML page names its address");
    e = parse("PLAY 1-10 DECKLINK DEVICE 2 FORMAT 1080i5000", &ok);
    expectTrue(ok && e.what == "DECKLINK DEVICE 2", "a DeckLink input names its device");

    parse("LOADBG 1-10 \"NEXT_CLIP\" MIX 25", &ok);
    expectTrue(!ok, "a preload is not on air");
    e = parse("LOADBG 1-10 \"NEXT_CLIP\" MIX 25 LINEAR RIGHT AUTO", &ok);
    expectTrue(ok && e.action.startsWith("Queued") && e.what == "NEXT_CLIP", "a queued clip is logged as queued");
    e = parse("LOAD 1-10 \"FROZEN\"", &ok);
    expectTrue(ok && e.action.startsWith("Load"), "a load shows its first frame, so it counts");

    e = parse("STOP 1-10", &ok);
    expectTrue(ok && e.action == "Stop", "stop");
    e = parse("PAUSE 1-10", &ok);
    expectTrue(ok && e.action == "Pause", "pause");
    e = parse("CLEAR 1-10", &ok);
    expectTrue(ok && e.action == "Clear" && e.layer == 10, "clearing a layer");
    e = parse("CLEAR 1", &ok);
    expectTrue(ok && e.action == "Clear channel" && e.layer == -1, "clearing a channel");

    e = parse("CG 1-20 ADD 1 \"LOWER_THIRD\" 1 \"<templateData/>\"", &ok);
    expectTrue(ok && e.action == "Play template" && e.what == "LOWER_THIRD", "a template added and played");
    parse("CG 1-20 ADD 1 \"LOWER_THIRD\" 0", &ok);
    expectTrue(!ok, "a template added without playing is not on air yet");
    e = parse("CG 1-20 ADD 1 LOWER_THIRD \"1\" \"data\"", &ok);
    expectTrue(ok && e.what == "LOWER_THIRD", "the client's other CG ADD form reads the same");
    e = parse("CG 1-20 NEXT 1", &ok);
    expectTrue(ok && e.action == "Next", "a template stepped on");
    e = parse("CG 1-20 INVOKE 1 \"showScore\"", &ok);
    expectTrue(ok && e.action == "Invoke" && e.what == "showScore", "an invoke names its function");

    // A named list: a range-for over "QStringList() << ..." iterates a temporary
    // that is already destroyed, because operator<< returns a reference to it.
    const QStringList notAsRun = { "CLS", "TLS", "INFO", "THUMBNAIL LIST", "VERSION SERVER",
                                   "MIXER 1-10 OPACITY 0.5 25", "DATA LIST", "CALL 1-10 LOOP 1" };
    for (const QString& quiet : notAsRun)
    {
        parse(quiet, &ok);
        expectTrue(!ok, QString("not as-run: %1").arg(quiet));
    }

    expectTrue(AsRun::csvField("plain") == "plain", "a plain field is left alone");
    expectTrue(AsRun::csvField("a, b") == "\"a, b\"", "a comma is quoted");
    expectTrue(AsRun::csvField("say \"hi\"") == "\"say \"\"hi\"\"\"", "a quote is doubled");
    expectTrue(AsRun::csvField("two\nlines") == "\"two\nlines\"", "a line break is quoted");

    e = parse("PLAY 1-10 \"AMB, final\"", &ok);
    const QString line = AsRun::csvLine(QDateTime(QDate(2026, 9, 23), QTime(20, 15, 3, 42)), "Main", e, "PLAY 1-10 \"AMB, final\"");
    expectTrue(line == "2026-09-23,20:15:03.042,Main,1,10,Play,\"AMB, final\",\"PLAY 1-10 \"\"AMB, final\"\"\"",
               QString("a whole line: %1").arg(line));
    expectTrue(AsRun::header().split(',').count() == 8, "the header has eight columns");

    const QDate today(2026, 9, 23);
    expectTrue(AsRun::fileName(today) == "AsRun_2026-09-23.csv", "one file a day");
    const QStringList folder = QStringList() << "AsRun_2026-09-23.csv" << "AsRun_2026-08-24.csv" << "AsRun_2026-08-23.csv"
                                             << "AsRun_2026-10-01.csv" << "Client_2025-01-01.log" << "AsRun_2026-02-30.csv";
    const QStringList old = AsRun::expired(folder, today);
    expectTrue(old == QStringList() << "AsRun_2026-08-23.csv", QString("only the 31-day-old file goes: %1").arg(old.join(", ")));
    expectTrue(AsRun::expired(folder, today, 0).isEmpty(), "a keep of 0 deletes nothing");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
