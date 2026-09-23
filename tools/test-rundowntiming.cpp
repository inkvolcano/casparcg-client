// Rundown timing arithmetic: hard-out times, over and under across midnight,
// clip and duration lengths, and what a saved rundown carries.

#include "../src/Common/RundownTiming.h"

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

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Rundown timing\n";

    using namespace RundownTiming;

    expectTrue(parseHardOut("21:00") == 21 * 3600, "21:00");
    expectTrue(parseHardOut("21:00:30") == 21 * 3600 + 30, "21:00:30");
    expectTrue(parseHardOut(" 9:05 ") == 9 * 3600 + 5 * 60, "9:05, with spaces");
    expectTrue(parseHardOut("") == -1 && parseHardOut("24:00") == -1 && parseHardOut("21:60") == -1 && parseHardOut("nine") == -1,
               "nothing, or not a time of day, is no hard out");
    expectTrue(formatTimeOfDay(21 * 3600 + 5) == "21:00:05", "a time of day is written in full");

    expectTrue(formatLength(0) == "0:00" && formatLength(65) == "1:05" && formatLength(3725) == "1:02:05", "lengths");
    expectTrue(formatLength(59.6) == "1:00", "rounded to the second");

    const int now = 20 * 3600;                       // 20:00:00
    expectTrue(overBy(now, 30 * 60, 21 * 3600) == -30 * 60, "ends 20:30 for a 21:00 hard out: 30 minutes under");
    expectTrue(overBy(now, 75 * 60, 21 * 3600) == 15 * 60, "ends 21:15: 15 minutes over");
    expectTrue(overBy(23 * 3600, 20 * 60, 30 * 60) == -70 * 60, "a 00:30 hard out at 23:00 is tonight's, not this morning's");
    expectTrue(overBy(30 * 60, 10 * 60, 23 * 3600 + 50 * 60) == 50 * 60, "and a 23:50 hard out at 00:30 was yesterday's");

    expectTrue(clipSeconds(120, 0, 0, 50) == 120, "a clip runs its own length");
    expectTrue(clipSeconds(120, 500, 0, 50) == 110, "less where it starts");
    expectTrue(clipSeconds(120, 0, 1500, 50) == 30, "or the Length set on it");
    expectTrue(clipSeconds(120, 0, 1500, 0) == 0, "a Length without a frame rate counts nothing, rather than a guess");
    expectTrue(clipSeconds(10, 1000, 0, 50) == 0, "a seek past the end counts nothing, not less than nothing");

    expectTrue(durationSeconds(5000, true, 0) == 5, "a duration in milliseconds");
    expectTrue(durationSeconds(250, false, 50) == 5, "a duration in frames");
    expectTrue(durationSeconds(0, true, 50) == 0 && durationSeconds(250, false, 0) == 0, "no duration, or no frame rate, is nothing");

    expectTrue(hardOutIn("<items><allowremotetriggering>false</allowremotetriggering><hardout>21:00:00</hardout><item/></items>") == "21:00:00",
               "a saved hard out is read back");
    expectTrue(hardOutIn("<items><hardout>9:5</hardout></items>").isEmpty(), "a damaged one is ignored");
    expectTrue(hardOutIn("<items><item/></items>").isEmpty(), "and a rundown without one has none");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
