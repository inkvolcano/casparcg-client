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

    // The presenter view: what is shown big, and what comes next.
    auto pick = [](bool onAir, double remaining = -1, qint64 firedAt = -1, bool disabled = false) {
        Pick p;
        p.onAir = onAir;
        p.remaining = remaining;
        p.firedAt = firedAt;
        p.disabled = disabled;
        return p;
    };

    QList<Pick> rows;
    rows << pick(false) << pick(true, -1, 500) << pick(true, 83.0) << pick(true, 6.0) << pick(false, -1, -1, true) << pick(false) << pick(false);
    expectTrue(primaryOnAir(rows) == 3, "shown big: of what is on air, the one ending soonest");
    expectTrue(primaryOnAir(QList<Pick>() << pick(true, -1, 100) << pick(true, -1, 900) << pick(false)) == 1,
               "with no end known for any, the one fired last");
    expectTrue(primaryOnAir(QList<Pick>() << pick(false) << pick(false)) == -1, "nothing on air: nothing big");

    expectTrue(nextItem(rows, 6) == 6, "next is what the operator has selected");
    expectTrue(nextItem(rows, 2) == 5, "the selection is already on air: the first usable item after the last one on air");
    expectTrue(nextItem(rows, 4) == 5, "a disabled selection is passed over");
    expectTrue(nextItem(rows, -1) == 5, "no selection: after the last on air, skipping the disabled one");
    expectTrue(nextItem(QList<Pick>() << pick(false, -1, -1, true) << pick(false), -1) == 1, "nothing on air: the first usable item");
    expectTrue(nextItem(QList<Pick>() << pick(false) << pick(true), -1) == -1, "the last item on air: nothing next");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
