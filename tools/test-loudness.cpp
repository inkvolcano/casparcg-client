// Loudness: the R128 meter against EBU Tech 3341's cases, and the OSC
// estimate's behaviour.

#include "../src/Common/LoudnessMeter.h"

#include <QtCore/QTextStream>

#include <cmath>
#include <vector>

static int checks = 0;
static int failures = 0;

static void expectNear(double actual, double expected, double tolerance, const QString& what)
{
    checks++;
    if (std::isfinite(actual) && std::fabs(actual - expected) <= tolerance)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n        got " << actual << ", expected " << expected << " +- " << tolerance << "\n";
}

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

using namespace Loudness;

// Feeds seconds of a sine at a peak level in dBFS (silence for -inf) on the
// given channels, in planar blocks of 10 ms as NDI delivers them.
static void feed(Meter& meter, int rate, int channels, double seconds, double dbfs, double hz = 1000.0,
                 const QList<int>& silent = QList<int>(), double* phase = nullptr)
{
    const double amplitude = std::isfinite(dbfs) ? std::pow(10.0, dbfs / 20.0) : 0.0;
    const int block = rate / 100;
    const int total = int(seconds * rate);
    std::vector<float> planar(size_t(block) * channels);
    double local = 0;
    double& p = phase != nullptr ? *phase : local;
    for (int done = 0; done < total; done += block)
    {
        for (int i = 0; i < block; i++)
        {
            const float v = float(amplitude * std::sin(p));
            p += 2.0 * 3.14159265358979323846 * hz / rate;
            for (int c = 0; c < channels; c++)
                planar[size_t(c) * block + i] = silent.contains(c) ? 0.0f : v;
        }
        meter.addPlanar(planar.data(), block, block);
    }
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Loudness\n";

    // Tech 3341 case 1: stereo 1 kHz sine at -23 dBFS reads -23.0 LUFS.
    {
        Meter m;
        m.configure(48000, 2);
        feed(m, 48000, 2, 20.0, -23.0);
        expectNear(m.momentary(), -23.0, 0.1, "3341 case 1: momentary -23.0");
        expectNear(m.shortTerm(), -23.0, 0.1, "3341 case 1: short-term -23.0");
        expectNear(m.integrated(), -23.0, 0.1, "3341 case 1: integrated -23.0");
    }

    // Case 2: the same at -33 dBFS reads -33.0.
    {
        Meter m;
        m.configure(48000, 2);
        feed(m, 48000, 2, 20.0, -33.0);
        expectNear(m.integrated(), -33.0, 0.1, "3341 case 2: -33 dBFS reads -33.0");
    }

    // Case 3: -36, -23, -36 dBFS for 10, 60, 10 s: the quiet parts are gated out.
    {
        Meter m;
        m.configure(48000, 2);
        double phase = 0;
        feed(m, 48000, 2, 10.0, -36.0, 1000.0, QList<int>(), &phase);
        feed(m, 48000, 2, 60.0, -23.0, 1000.0, QList<int>(), &phase);
        feed(m, 48000, 2, 10.0, -36.0, 1000.0, QList<int>(), &phase);
        expectNear(m.integrated(), -23.0, 0.1, "3341 case 3: the relative gate leaves out the -36 dBFS parts");
    }

    // Case 4: -72, -36, -23, -36, -72 dBFS for 10, 10, 60, 10, 10 s.
    {
        Meter m;
        m.configure(48000, 2);
        double phase = 0;
        feed(m, 48000, 2, 10.0, -72.0, 1000.0, QList<int>(), &phase);
        feed(m, 48000, 2, 10.0, -36.0, 1000.0, QList<int>(), &phase);
        feed(m, 48000, 2, 60.0, -23.0, 1000.0, QList<int>(), &phase);
        feed(m, 48000, 2, 10.0, -36.0, 1000.0, QList<int>(), &phase);
        feed(m, 48000, 2, 10.0, -72.0, 1000.0, QList<int>(), &phase);
        expectNear(m.integrated(), -23.0, 0.1, "3341 case 4: below -70 LUFS is never counted");
    }

    // The K-weighting is right for the rate it is given.
    {
        Meter m;
        m.configure(44100, 2);
        feed(m, 44100, 2, 10.0, -23.0);
        expectNear(m.integrated(), -23.0, 0.1, "44.1 kHz reads the same");
    }

    // One channel of two: 3 dB lower.
    {
        Meter m;
        m.configure(48000, 2);
        feed(m, 48000, 2, 10.0, -20.0, 1000.0, QList<int>() << 1);
        expectNear(m.integrated(), -23.01, 0.1, "the same sine on one channel of two is 3 dB quieter");
    }

    // 5.1: the LFE does not count, the surrounds count 1.41.
    {
        Meter lfe;
        lfe.configure(48000, 6);
        feed(lfe, 48000, 6, 10.0, -20.0, 1000.0, QList<int>() << 0 << 1 << 2 << 4 << 5);
        expectTrue(!isReading(lfe.integrated()), "5.1: sound only on the LFE measures nothing");

        Meter surround;
        surround.configure(48000, 6);
        feed(surround, 48000, 6, 10.0, -23.0, 1000.0, QList<int>() << 0 << 1 << 2 << 3 << 5);
        expectNear(surround.integrated(), -26.01 + 10.0 * std::log10(1.41), 0.1, "5.1: a surround channel weighs 1.41");
    }

    // K-weighting: a low tone counts for less than the same level at 1 kHz.
    {
        Meter low;
        low.configure(48000, 2);
        feed(low, 48000, 2, 10.0, -23.0, 40.0);
        expectTrue(low.integrated() < -23.5, "a 40 Hz tone reads lower than the same level at 1 kHz");
    }

    // Before there is enough audio, there is no reading.
    {
        Meter m;
        m.configure(48000, 2);
        feed(m, 48000, 2, 0.3, -23.0);
        expectTrue(!isReading(m.momentary()), "less than 400 ms: no momentary yet");
        expectTrue(!isReading(m.shortTerm()), "less than 3 s: no short-term yet");
        feed(m, 48000, 2, 0.2, -23.0);
        expectTrue(isReading(m.momentary()), "after 400 ms: a momentary reading");
        m.reset();
        expectTrue(!isReading(m.integrated()) && m.seconds() == 0.0, "reset starts again");
    }

    // Silence measures nothing.
    {
        Meter m;
        m.configure(48000, 2);
        feed(m, 48000, 2, 5.0, -std::numeric_limits<double>::infinity());
        expectTrue(!isReading(m.integrated()) && !isReading(m.shortTerm()), "silence: no reading");
    }

    // Interleaved input reads the same as planar.
    {
        Meter m;
        m.configure(48000, 2);
        const double a = std::pow(10.0, -23.0 / 20.0);
        std::vector<float> data(48000 * 2);
        double phase = 0;
        for (int second = 0; second < 5; second++)
        {
            for (int i = 0; i < 48000; i++)
            {
                const float v = float(a * std::sin(phase));
                phase += 2.0 * 3.14159265358979323846 * 1000.0 / 48000;
                data[size_t(i) * 2] = v;
                data[size_t(i) * 2 + 1] = v;
            }
            m.addInterleaved(data.data(), 48000);
        }
        expectNear(m.integrated(), -23.0, 0.1, "interleaved samples read the same");
    }

    // The estimate from OSC peaks: a steady sine's peaks give the sine's loudness
    // without weighting (1 kHz: within the 0.7 dB the weighting adds there).
    {
        PeakEstimate e;
        const double peak = std::pow(10.0, -23.0 / 20.0);
        qint64 t = 1000000;
        for (int i = 0; i < 50 * 10; i++, t += 20)
            e.add(QList<double>() << peak << peak, t);
        expectNear(e.shortTerm(t), -23.0 - 0.69, 0.1, "estimate: steady peaks at -23 dBFS read about -23.7");
        expectNear(e.integrated(), -23.0 - 0.69, 0.1, "estimate: integrated the same");

        e.add(QList<double>() << peak << peak, t + 5000);
        expectTrue(isReading(e.shortTerm(t + 5000)), "estimate: after a gap, a new reading");
        expectTrue(!isReading(e.shortTerm(t + 9000)), "estimate: no readings for 4 s, no short-term");

        e.reset();
        expectTrue(!isReading(e.integrated()), "estimate: reset starts again");
    }

    expectTrue(text(-23.04) == "-23.0" && text(none()) == QString::fromUtf8("\xe2\x80\x93"), "shown with one decimal, or a dash");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
