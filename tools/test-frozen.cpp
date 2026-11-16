// When a playing clip counts as frozen.

#include "../src/Common/FrozenRules.h"

#include <QtCore/QString>
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

using namespace FrozenRules;

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Frozen output\n";

    const qint64 moved = 100000;   // when the clip's time last changed
    expectTrue(!isFrozen(moved + 2999, moved, 12.0, 60.0, true, false), "not yet: under three seconds since it moved");
    expectTrue(isFrozen(moved + 3000, moved, 12.0, 60.0, true, false), "playing, not moved for three seconds: frozen");
    expectTrue(!isFrozen(moved + 9000, moved, 12.0, 60.0, true, true), "a paused clip is not frozen");
    expectTrue(!isFrozen(moved + 9000, moved, 12.0, 60.0, false, false), "nor one that is not playing");
    expectTrue(!isFrozen(moved + 9000, moved, 59.8, 60.0, true, false), "nor one at its end, holding its last frame");
    expectTrue(isFrozen(moved + 9000, moved, 59.0, 60.0, true, false), "but a second before the end, it is");
    expectTrue(!isFrozen(moved + 9000, 0, 0.0, 60.0, true, false), "no time ever received (OSC off): nothing is flagged");
    expectTrue(isFrozen(moved + 9000, moved, 5.0, 0.0, true, false), "a stream with no length still counts once its time stops");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
