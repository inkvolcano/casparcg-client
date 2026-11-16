// The live run sheet page: what it says about one item right now.

#include "../src/Common/LiveRunSheetRules.h"

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

using namespace LiveRunSheetRules;

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Live run sheet\n";

    const qint64 now = 1000000;

    Live clip = liveState(true, 15.0, 60.0, false, -1, -1, now);
    expectTrue(clip.state == State::Playing && clip.remaining == 45.0, "a playing clip: 45 s left of 60, from the server's time");

    Live paused = liveState(true, 15.0, 60.0, true, now - 99000, 60.0, now);
    expectTrue(paused.state == State::Paused && paused.remaining == 45.0, "a paused clip says so, and keeps its time left");

    Live held = liveState(true, 60.0, 60.0, false, -1, -1, now);
    expectTrue(held.state == State::Playing && held.remaining == 0.0, "at its end: no time left, never less");

    Live still = liveState(false, 0, 0, false, now - 4000, 10.0, now);
    expectTrue(still.state == State::OnAir && qAbs(still.remaining - 6.0) < 0.001, "a still with a 10 s duration, fired 4 s ago: 6 s left");

    Live done = liveState(false, 0, 0, false, now - 11000, 10.0, now);
    expectTrue(done.state == State::Idle, "its time up: off air");

    Live lowerThird = liveState(false, 0, 0, false, now - 300000, -1, now);
    expectTrue(lowerThird.state == State::OnAir && lowerThird.remaining < 0, "fired with no length: on air, no end to count to");

    Live never = liveState(false, 0, 0, false, -1, 30.0, now);
    expectTrue(never.state == State::Idle, "never fired, or stopped since: nothing");

    expectTrue(stateText(State::Playing) == "playing" && stateText(State::OnAir) == "onair" && stateText(State::Idle).isEmpty(),
               "the words the page reads");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
