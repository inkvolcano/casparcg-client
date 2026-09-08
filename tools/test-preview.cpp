// Audio levels for the preview panel: how a decoded file becomes meters.
//
// Qt 6.5 cannot tap the audio a QMediaPlayer is playing, so the panel decodes the
// file separately and keeps peaks on a timeline that the meters index by playback
// position. Three things about that have to hold, and none of them is obvious
// from looking at a meter that appears to work:
//
//   A meter must never under-report. Peaks are what protect an operator from
//   clipping, so a slice keeps the largest sample it saw, not the last one and
//   not an average — an average would hide exactly the transient that matters.
//
//   "Not analysed yet" and "silent" are different answers. A long file is still
//   decoding while the front of it is already playing, and a position past the
//   decoded end must read as unknown so the panel can draw nothing, rather than
//   as digital silence, which would draw an empty meter and look like a fault.
//
//   The meter must fall slower than the audio does. Peaks are taken instantly so
//   nothing is missed, but an unrestrained fall makes the bar flicker rather than
//   read, so the fall is rate-limited and the rise is not.

#include "../src/Common/AudioLevelTrack.h"

#include <QtCore/QString>
#include <QtCore/QTextStream>
#include <QtCore/QVector>

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
    if (std::abs(actual - wanted) <= tolerance)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what
                        << "  (wanted " << wanted << " +/- " << tolerance
                        << ", got " << actual << ")\n";
}

static void sameInt(int actual, int wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what
                        << "  (wanted " << wanted << ", got " << actual << ")\n";
}

static void dbfsMapsTheWayAMeterExpects()
{
    near(AudioLevelTrack::dbfsFor(1.0), 0.0, 0.001, "full scale is 0 dBFS");
    near(AudioLevelTrack::dbfsFor(0.5), -6.02, 0.05, "half amplitude is about -6 dBFS");
    near(AudioLevelTrack::dbfsFor(0.25), -12.04, 0.05, "a quarter is about -12 dBFS");

    // log10(0) is negative infinity, which would poison every calculation
    // downstream and paint a bar of undefined length.
    near(AudioLevelTrack::dbfsFor(0.0), AudioLevelTrack::DB_MIN, 0.001,
         "digital silence reads as the floor, not negative infinity");
    near(AudioLevelTrack::dbfsFor(-0.5), AudioLevelTrack::DB_MIN, 0.001,
         "a negative peak cannot happen, and is floored rather than trusted");

    // Some codecs decode slightly over full scale. A meter that returned a
    // positive dB would draw past the top of its own track.
    near(AudioLevelTrack::dbfsFor(1.5), 0.0, 0.001, "over full scale is clamped to 0 dBFS");

    expectTrue(AudioLevelTrack::dbfsFor(0.00001) >= AudioLevelTrack::DB_MIN,
               "a very quiet sample still sits on or above the floor");
}

static void aSliceKeepsItsLoudestSample()
{
    AudioLevelTrack track;

    // All within one 20 ms slice, loud one in the middle: exactly the transient a
    // meter exists to show.
    track.addPeak(0, 0, 0.1);
    track.addPeak(5, 0, 0.9);
    track.addPeak(10, 0, 0.2);

    QVector<double> levels = track.dbfsAt(0);

    sameInt(levels.size(), 1, "one channel was recorded");
    near(levels.at(0), AudioLevelTrack::dbfsFor(0.9), 0.001,
         "the slice reports its peak, not the last sample in it");

    // Averaging these three would give about 0.4, which is nearly 7 dB quieter
    // than the truth — the difference between "fine" and "clipping".
    expectTrue(levels.at(0) > AudioLevelTrack::dbfsFor(0.5),
               "the peak is not diluted by the quiet samples around it");
}

static void positionsMapToTheRightSlice()
{
    AudioLevelTrack track;

    track.addPeak(0, 0, 0.2);
    track.addPeak(AudioLevelTrack::SLICE_MS, 0, 0.8);
    track.addPeak(AudioLevelTrack::SLICE_MS * 2, 0, 0.4);

    near(track.dbfsAt(0).at(0), AudioLevelTrack::dbfsFor(0.2), 0.001, "slice 0 reads its own peak");
    near(track.dbfsAt(AudioLevelTrack::SLICE_MS - 1).at(0), AudioLevelTrack::dbfsFor(0.2), 0.001,
         "the last millisecond of a slice still belongs to it");
    near(track.dbfsAt(AudioLevelTrack::SLICE_MS).at(0), AudioLevelTrack::dbfsFor(0.8), 0.001,
         "the first millisecond of the next slice belongs to the next one");
    near(track.dbfsAt(AudioLevelTrack::SLICE_MS * 2).at(0), AudioLevelTrack::dbfsFor(0.4), 0.001,
         "slice 2 reads its own peak");
}

static void unanalysedIsNotTheSameAsSilent()
{
    AudioLevelTrack track;
    track.addPeak(0, 0, 0.5);

    expectTrue(!track.dbfsAt(0).isEmpty(), "an analysed position has levels");

    // A long file meters from the start while the rest is still decoding. Past
    // the decoded end the answer is "no idea", and the panel draws no meter —
    // returning a floor would draw a silent meter, which reads as a fault.
    expectTrue(track.dbfsAt(60000).isEmpty(), "a position past the decoded end has no levels");
    expectTrue(track.dbfsAt(-1).isEmpty(), "a negative position has no levels");

    AudioLevelTrack empty;
    expectTrue(empty.dbfsAt(0).isEmpty(), "a file with no audio has no levels anywhere");
    expectTrue(empty.isEmpty(), "and reports itself as empty");
}

static void everyChannelIsKept()
{
    AudioLevelTrack track;

    track.addPeak(0, 0, 0.9);
    track.addPeak(0, 1, 0.1);

    sameInt(track.channels(), 2, "two channels were seen");

    QVector<double> levels = track.dbfsAt(0);
    sameInt(levels.size(), 2, "both channels are reported");
    expectTrue(levels.at(0) > levels.at(1), "the channels are not confused with each other");

    // A silent channel in a file that has one is still a channel, and drawing it
    // is how an operator sees that it is silent.
    track.addPeak(0, 5, 0.0);
    sameInt(track.channels(), 6, "a later channel widens the reading");
    sameInt(track.dbfsAt(0).size(), 6, "and every channel up to it is reported");
    near(track.dbfsAt(0).at(4), AudioLevelTrack::DB_MIN, 0.001,
         "a channel that was never written reads as silence, not as missing");
}

static void nonsenseIsRefusedRatherThanStored()
{
    AudioLevelTrack track;

    // A decoder handing back a channel count larger than the fixed width, or a
    // buffer with no timestamp, must not write outside the slice.
    track.addPeak(-1, 0, 0.9);
    track.addPeak(0, -1, 0.9);
    track.addPeak(0, 99, 0.9);

    expectTrue(track.isEmpty(), "nothing out of range was stored");
    sameInt(track.channels(), 0, "and no channel was invented");
}

static void aGapIsFilledRatherThanSkipped()
{
    AudioLevelTrack track;

    track.addPeak(0, 0, 0.5);
    track.addPeak(1000, 0, 0.5);

    // Decoded buffers arrive in order, but a dropped one must not shift every
    // later reading earlier — which would slide the meters out of step with the
    // picture for the rest of the file.
    near(track.dbfsAt(1000).at(0), AudioLevelTrack::dbfsFor(0.5), 0.001,
         "a peak after a gap is still found at its own position");
    near(track.dbfsAt(500).at(0), AudioLevelTrack::DB_MIN, 0.001,
         "the gap itself reads as silence");
    expectTrue(track.analysedToMs() >= 1000, "the track covers the whole span");
}

static void metersRiseAtOnceAndFallSlowly()
{
    const double fall = 1.6;

    // A peak must be shown the instant it happens. Anything else under-reports.
    near(AudioLevelTrack::applyBallistics(-40.0, -6.0, fall), -6.0, 0.001,
         "a rise is taken immediately");
    near(AudioLevelTrack::applyBallistics(-6.0, -6.0, fall), -6.0, 0.001,
         "an unchanged level stays put");

    // The fall is rate-limited so the bar can be read rather than flickering.
    near(AudioLevelTrack::applyBallistics(-6.0, -40.0, fall), -7.6, 0.001,
         "a fall is limited to one step");

    // ...but it must still arrive, and must not overshoot past the target.
    near(AudioLevelTrack::applyBallistics(-39.0, -40.0, fall), -40.0, 0.001,
         "a fall smaller than one step lands exactly on the target");

    double level = 0.0;
    for (int i = 0; i < 200; i++)
        level = AudioLevelTrack::applyBallistics(level, AudioLevelTrack::DB_MIN, fall);

    near(level, AudioLevelTrack::DB_MIN, 0.001, "a sustained fall reaches the floor and stops there");
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "Preview audio levels\n";

    dbfsMapsTheWayAMeterExpects();
    aSliceKeepsItsLoudestSample();
    positionsMapToTheRightSlice();
    unanalysedIsNotTheSameAsSilent();
    everyChannelIsKept();
    nonsenseIsRefusedRatherThanStored();
    aGapIsFilledRatherThanSkipped();
    metersRiseAtOnceAndFallSlowly();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}
