// Items that fire at a time of day: reading the time, when it is due, the countdown.

#include "../src/Common/TimeTriggerRules.h"

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

using namespace TimeTriggerRules;

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Time triggers\n";

    expectTrue(parse("19:59:50") == QTime(19, 59, 50) && parse(" 7:05:00 ") == QTime(7, 5, 0), "a time of day, with or without a leading zero");
    expectTrue(parse("19:59") == QTime(19, 59, 0), "minutes only: the top of that minute");
    expectTrue(!parse("25:00").isValid() && !parse("soon").isValid() && !parse("").isValid(), "anything else is no time");
    expectTrue(normalised("9:05") == "09:05:00" && normalised("x").isEmpty(), "kept in one form");

    const QDateTime look(QDate(2026, 11, 16), QTime(19, 59, 49));
    const QTime at(19, 59, 50);
    expectTrue(!due(at, look, look.addMSecs(900)), "not before its second");
    expectTrue(due(at, look, look.addSecs(1)), "due on the look that passes its second");
    expectTrue(due(at, look, look.addSecs(3)), "and when that look came a little late");
    expectTrue(!due(at, look.addSecs(1), look.addSecs(2)), "but only once: the next look is past it");
    expectTrue(!due(at, look.addSecs(-3600), look.addSecs(1)), "a client asleep for an hour does not fire what it slept through");

    const QDateTime beforeMidnight(QDate(2026, 11, 16), QTime(23, 59, 59));
    expectTrue(due(QTime(0, 0, 0), beforeMidnight, beforeMidnight.addSecs(1)), "a midnight time is due across the change of day");

    expectTrue(countdown(at, QDateTime(QDate(2026, 11, 16), QTime(19, 56, 38))) == "in 3:12", "minutes and seconds to go");
    expectTrue(countdown(at, QDateTime(QDate(2026, 11, 16), QTime(18, 55, 50))) == "in 1:04:00", "hours when it is over an hour");
    expectTrue(countdown(at, QDateTime(QDate(2026, 11, 16), QTime(20, 0, 0))) == "in 23:59:50", "gone by today: tomorrow's");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
