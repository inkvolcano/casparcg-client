// The run sheet's start times: offsets from the top, or back-timed to a hard out.

#include "../src/Common/RunSheetRules.h"

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

using namespace RunSheetRules;

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Run sheet\n";

    // An opener of 30 s, a group header with no length of its own, a 4:05 clip, a 10 s sting.
    const QList<double> lengths = QList<double>() << 30 << -1 << 245 << 10;

    const QStringList offsets = starts(lengths, -1);
    expectTrue(offsets == (QStringList() << "+0:00" << "" << "+0:30" << "+4:35"),
               "without a hard out: offsets from the top - " + offsets.join(" | "));
    expectTrue(total(lengths) == 285, "the total leaves out rows without a length");

    const int hardOut = 21 * 3600;   // 21:00:00
    const QStringList times = starts(lengths, hardOut);
    expectTrue(times == (QStringList() << "20:55:15" << "" << "20:55:45" << "20:59:50"),
               "with a hard out: back-timed so the last item ends on it - " + times.join(" | "));

    const QStringList afterMidnight = starts(QList<double>() << 600 << 600, 5 * 60);   // hard out 00:05
    expectTrue(afterMidnight == (QStringList() << "23:45:00" << "23:55:00"), "a hard out just after midnight starts the evening before");

    expectTrue(starts(QList<double>(), -1).isEmpty(), "an empty rundown has no starts");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
