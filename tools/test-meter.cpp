// What a level meter does between readings.
//
// The Audio Levels panel drew whatever number arrived last. Levels come in over
// OSC a handful of times a second and each replaces the one before it, so a
// single frame at -2 dB between two frames at -20 was drawn once and gone. The
// meter was showing a sequence of instants rather than a level, and the one event
// it exists to report - a clip - was the one it forgot fastest.
//
// These tests are about the two properties that fix: a peak is never
// under-reported, and a clip is never quietly dropped. Everything else here is
// about the ways a meter can lie while looking fine - decaying at the packet rate
// instead of in real time, a peak marker parked on the floor, a clock that steps
// backwards.

#include "../src/Common/MeterBallistics.h"

#include <QtCore/QString>
#include <QtCore/QTextStream>

#include <cmath>

static int failures = 0;
static int checks = 0;

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

static void near(double actual, double wanted, double tolerance, const QString& what)
{
    checks++;
    if (std::fabs(actual - wanted) <= tolerance)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n         wanted: " << wanted
                        << "\n            got: " << actual << "\n";
}

static void aFreshMeterIsSilentAndClean()
{
    MeterBallistics::Meter meter;
    meter.reset();

    near(meter.level(), MeterBallistics::dbMin(), 0.001, "a new meter reads silence");
    near(meter.peak(), MeterBallistics::dbMin(), 0.001, "and holds no peak");
    expectTrue(!meter.clipped(), "and is not clipped");

    // A marker sitting on the bottom of the meter before anything has played
    // reads as a fault rather than as silence.
    expectTrue(!meter.hasPeak(), "and draws no peak marker at all");
}

static void aTransientIsTakenWhole()
{
    // The property the whole change rests on. A meter that eased into a peak
    // would under-report it, and under-reporting headroom is the one error a
    // meter must not make.
    MeterBallistics::Meter meter;
    meter.reset();

    meter.addReading(-20.0, 0);
    meter.addReading(-2.0, 20);

    near(meter.level(), -2.0, 0.001, "a rise is taken immediately and in full");
    near(meter.peak(), -2.0, 0.001, "and the peak marker goes with it");
    expectTrue(meter.hasPeak(), "and there is now a marker to draw");
}

static void theFallIsRateLimitedNotInstant()
{
    MeterBallistics::Meter meter;
    meter.reset();

    meter.addReading(-2.0, 0);
    meter.addReading(-40.0, 100);

    // A tenth of a second at 30 dB/s is 3 dB, not the whole 38.
    near(meter.level(), -5.0, 0.001, "the bar falls at its rate, not straight to the reading");
    expectTrue(meter.level() > -40.0, "so a brief dip does not blank the meter");

    // Long enough and it does arrive, rather than hanging above the real level.
    // The readings keep coming, because a server reporting a steady -40 is a
    // different thing from a server that has stopped reporting.
    for (int t = 200; t <= 3000; t += 100)
        meter.addReading(-40.0, t);

    near(meter.level(), -40.0, 0.001, "and settles exactly on it rather than past it");
}

static void aSteadyLevelIsHeldButAStoppedStreamIsNot()
{
    // These look identical for one packet and are opposite things. A meter frozen
    // on the last level it saw reads as "still playing", which is worse than a
    // meter that is merely wrong.
    MeterBallistics::Meter reporting;
    reporting.reset();
    for (int t = 0; t <= 4000; t += 40)
        reporting.addReading(-25.0, t);

    near(reporting.level(), -25.0, 0.001, "a steady level is held for as long as it is reported");

    MeterBallistics::Meter stopped;
    stopped.reset();
    stopped.addReading(-25.0, 0);

    // Inside the staleness window the reading still stands, so a dropped packet
    // or two does not make the meter twitch.
    stopped.advanceTo(500);
    near(stopped.level(), -25.0, 0.001, "and a late packet does not make it twitch");

    for (int t = 1100; t <= 6000; t += 100)
        stopped.advanceTo(t);

    near(stopped.level(), MeterBallistics::dbMin(), 0.001,
         "but once the reports stop for good the meter goes quiet");
}

static void theFallIsInRealTimeNotPerPacket()
{
    // OSC arrives when it arrives. A decay measured per packet would fall at a
    // different speed on a busy server than an idle one, which is the sort of
    // thing that gets reported as "the meters feel wrong" and never diagnosed.
    MeterBallistics::Meter dense;
    dense.reset();
    dense.addReading(0.0, 0);
    for (int t = 10; t <= 500; t += 10)
        dense.addReading(MeterBallistics::dbMin(), t);

    MeterBallistics::Meter sparse;
    sparse.reset();
    sparse.addReading(0.0, 0);
    for (int t = 100; t <= 500; t += 100)
        sparse.addReading(MeterBallistics::dbMin(), t);

    near(dense.level(), sparse.level(), 0.001,
         "half a second of decay is the same whether it arrived in 50 packets or 5");
}

static void aMeterKeepsFallingWhenNothingArrives()
{
    // A stream that stops should decay to silence rather than freeze showing the
    // last level it saw, which would read as "still playing".
    MeterBallistics::Meter meter;
    meter.reset();
    meter.addReading(-6.0, 0);

    // Nothing happens while the reading is still fresh: a dropped packet must not
    // make the meter twitch.
    meter.advanceTo(500);
    near(meter.level(), -6.0, 0.001, "a fresh reading still stands");

    for (int t = 1100; t <= 5000; t += 100)
        meter.advanceTo(t);

    near(meter.level(), MeterBallistics::dbMin(), 0.001, "and reaches the floor");
    expectTrue(meter.level() >= MeterBallistics::dbMin(), "without going below it");
}

static void thePeakIsHeldLongEnoughToRead()
{
    MeterBallistics::Meter meter;
    meter.reset();
    meter.addReading(-3.0, 0);
    meter.addReading(MeterBallistics::dbMin(), 20);

    // Still there a second later: a marker that had already gone would not have
    // told anybody anything.
    meter.advanceTo(1000);
    near(meter.peak(), -3.0, 0.001, "the peak marker is still there a second on");

    // And it does eventually go, rather than describing a level that has gone.
    for (int t = 1100; t <= 4000; t += 100)
        meter.advanceTo(t);

    expectTrue(meter.peak() < -3.0, "and after the hold it starts to fall");
}

static void thePeakNeverSitsBelowTheBar()
{
    // A marker drawn under the top of the bar looks like a rendering fault.
    MeterBallistics::Meter meter;
    meter.reset();

    meter.addReading(-30.0, 0);
    for (int t = 100; t <= 3000; t += 100)
    {
        meter.addReading(-30.0, t);
        expectTrue(meter.peak() >= meter.level() - 0.001,
                   "the peak marker is never below the bar");
    }
}

static void aClipLatchesUntilItIsCleared()
{
    // The one thing the old meter forgot fastest. Silence afterwards must not
    // clear it: the question is "did it clip", not "is it clipping".
    MeterBallistics::Meter meter;
    meter.reset();

    meter.addReading(0.0, 0);
    expectTrue(meter.clipped(), "full scale clips");

    for (int t = 100; t <= 10000; t += 100)
        meter.addReading(MeterBallistics::dbMin(), t);

    expectTrue(meter.clipped(), "and ten seconds of silence does not clear it");

    meter.clearClip();
    expectTrue(!meter.clipped(), "only the operator clears it");

    meter.addReading(MeterBallistics::dbMin(), 11000);
    expectTrue(!meter.clipped(), "and it stays clear until something clips again");
}

static void whatDoesAndDoesNotCountAsAClip()
{
    // A hair under full scale is a real clip arriving through a float conversion.
    // A clip indicator that misses those is worse than one a tenth of a decibel
    // eager.
    MeterBallistics::Meter atFullScale;
    atFullScale.reset();
    atFullScale.addReading(-0.05, 0);
    expectTrue(atFullScale.clipped(), "a hair under full scale still clips");

    MeterBallistics::Meter loud;
    loud.reset();
    loud.addReading(-1.0, 0);
    expectTrue(!loud.clipped(), "but a loud level that is not at the top does not");

    MeterBallistics::Meter hot;
    hot.reset();
    hot.addReading(-0.2, 0);
    expectTrue(!hot.clipped(), "and neither does one just below the threshold");
}

static void readingsOutsideTheScaleAreBroughtIn()
{
    MeterBallistics::Meter meter;
    meter.reset();

    // A server reporting above full scale, or a conversion producing one.
    meter.addReading(6.0, 0);
    near(meter.level(), 0.0, 0.001, "a reading above full scale is held at the top");
    expectTrue(meter.clipped(), "and counts as a clip, which it is");

    MeterBallistics::Meter quiet;
    quiet.reset();
    quiet.addReading(-200.0, 0);
    near(quiet.level(), MeterBallistics::dbMin(), 0.001,
         "and one below the floor is held at the floor");
}

static void aClockGoingBackwardsDoesNotBreakIt()
{
    // A manual clock change or an NTP step must not drive the meter upwards or
    // stall its decay forever.
    MeterBallistics::Meter meter;
    meter.reset();
    meter.addReading(-6.0, 10000);

    const double before = meter.level();
    meter.advanceTo(5000);

    expectTrue(meter.level() <= before + 0.001, "a backwards clock never raises the bar");

    // And the meter is not stuck afterwards: once the stream stops for long
    // enough on the new clock, it still goes quiet rather than freezing.
    for (int t = 5100; t <= 20000; t += 100)
        meter.advanceTo(t);

    near(meter.level(), MeterBallistics::dbMin(), 0.001,
         "and it still decays once time moves forward again");
}

static void resetPutsEverythingBack()
{
    // The panel resets a meter when the device or channel changes. Anything left
    // behind would be the previous channel's level shown as this one's.
    MeterBallistics::Meter meter;
    meter.reset();
    meter.addReading(0.0, 0);
    meter.advanceTo(100);

    expectTrue(meter.clipped(), "a clip is showing");

    meter.reset();

    near(meter.level(), MeterBallistics::dbMin(), 0.001, "reset silences the bar");
    near(meter.peak(), MeterBallistics::dbMin(), 0.001, "and drops the peak");
    expectTrue(!meter.clipped(), "and clears the clip");
    expectTrue(!meter.hasPeak(), "and draws no marker");

    // Time starts again from the next reading rather than from the old clock,
    // which would otherwise apply a huge decay on the first advance.
    meter.addReading(-10.0, 999999);
    near(meter.level(), -10.0, 0.001, "and the first reading after it is taken whole");
}

static void theConstantsAreTheOnesTheMetersAlreadyUse()
{
    // AudioMeterWidget and AudioLevelTrack both draw from -61. A third floor
    // would mean the server's meters and the preview's meters disagreed about
    // what half way up means.
    near(MeterBallistics::dbMin(), -61.0, 0.001, "the floor matches the other meters");
    expectTrue(MeterBallistics::peakHoldMs() >= 1000,
               "the peak is held long enough to look up at");
    expectTrue(MeterBallistics::barFallDbPerSecond() > MeterBallistics::peakFallDbPerSecond(),
               "the bar falls faster than the marker, which is showing history");
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "Meter ballistics\n";

    aFreshMeterIsSilentAndClean();
    aTransientIsTakenWhole();
    theFallIsRateLimitedNotInstant();
    aSteadyLevelIsHeldButAStoppedStreamIsNot();
    theFallIsInRealTimeNotPerPacket();
    aMeterKeepsFallingWhenNothingArrives();
    thePeakIsHeldLongEnoughToRead();
    thePeakNeverSitsBelowTheBar();
    aClipLatchesUntilItIsCleared();
    whatDoesAndDoesNotCountAsAClip();
    readingsOutsideTheScaleAreBroughtIn();
    aClockGoingBackwardsDoesNotBreakIt();
    resetPutsEverythingBack();
    theConstantsAreTheOnesTheMetersAlreadyUse();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}
