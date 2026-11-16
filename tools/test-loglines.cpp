// Help > Show Log: the client's log read back into entries, and filtered.

#include "../src/Common/LogLines.h"

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

using namespace LogLines;

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Log lines\n";

    // Lines as the client writes them, from a real log of 2026-11-15.
    const QString log =
        "[2026-11-15 09:38:20.246] [8990] [D] Starting CasparCG Client 2.3.1.fc570e46a6\r\n"
        "[2026-11-15 09:38:23.785] [8990] [W] QObject::connect: No such slot RundownWidget::tabCloseRequested(int)\r\n"
        "[2026-11-15 09:38:24.595] [8990] [I] libpng warning: iCCP: known incorrect sRGB profile\r\n"
        "[2026-11-15 09:38:25.001] [8990] [C] Failed to execute sql query: SELECT 1\r\n"
        "    second line of the same message\r\n"
        "[2026-11-15 09:38:27.314] [8990] [D] Starting the client took 7061 ms\r\n";

    const QList<Entry> entries = parse(log);
    expectTrue(entries.count() == 5, QString("five entries: %1").arg(entries.count()));
    expectTrue(entries.at(1).level == 'W' && entries.at(1).time == "2026-11-15 09:38:23.785"
                   && entries.at(1).text.startsWith("QObject::connect"), "time, level and message read");
    expectTrue(entries.at(3).text == "Failed to execute sql query: SELECT 1\n    second line of the same message",
               "a line without a head belongs to the entry before it");

    int warnings = 0, errors = 0, matching = 0;
    for (const Entry& entry : entries)
    {
        warnings += passes(entry, Filter::WarningsAndErrors, QString()) ? 1 : 0;
        errors += passes(entry, Filter::Errors, QString()) ? 1 : 0;
        matching += passes(entry, Filter::Everything, "  STARTING ") ? 1 : 0;
    }
    expectTrue(warnings == 2 && errors == 1, "warnings and errors: 2; errors only: 1");
    expectTrue(matching == 2, "search ignores case and the spaces around it");
    expectTrue(passes(entries.at(3), Filter::Errors, "second line"), "and finds a word on a continuation line");

    // Following a log that is still being written: text arrives in pieces, cut
    // anywhere - here in the middle of the second line, then of the fourth.
    Reader reader;
    QList<Entry> followed = reader.feed(log.left(140));
    expectTrue(followed.isEmpty(), QString("an entry is finished only once the next one has fully arrived: %1").arg(followed.count()));
    followed << reader.feed(log.mid(140, 150));
    followed << reader.feed(log.mid(290));
    Entry writing;
    expectTrue(reader.peek(&writing) && writing.text.startsWith("Starting the client took"),
               "the entry still being written can be seen before it is finished");
    followed << reader.flush();
    expectTrue(followed.count() == entries.count() && followed.at(1).text == entries.at(1).text && followed.at(3).text == entries.at(3).text,
               "and the pieces read the same as the whole, no message cut in two");

    expectTrue(format(entries.at(1)).startsWith("09:38:23.785  W  QObject::connect"), "shown with its time, level and message");
    expectTrue(parse("not a log line\n").first().level == '?', "text that is not an entry is still shown");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
