#pragma once

// Loudness (build 312): EBU R128 / ITU-R BS.1770-4 measured from a channel's
// own audio, and a labelled estimate when only the OSC peak levels are there.
//
//   Meter          - the real measurement, from samples: K-weighting (the
//                    two BS.1770 filters, worked out for the sample rate),
//                    momentary (400 ms), short-term (3 s) and integrated
//                    loudness with the absolute (-70 LUFS) and relative (-10 LU)
//                    gates. Fed from the channel's NDI audio.
//   PeakEstimate   - what can be said from CasparCG's OSC levels, which are
//                    per-frame peaks with no weighting: each peak treated as a
//                    sine's, gated the same way. It reads high on speech and
//                    anything else with a high crest factor, so it is always
//                    shown as an estimate.
//
// Pure, so tools/test-loudness checks it against the EBU Tech 3341 cases.

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <cmath>
#include <limits>
#include <vector>

namespace Loudness
{
    // No reading yet.
    inline double none()
    {
        return -std::numeric_limits<double>::infinity();
    }

    inline bool isReading(double lufs)
    {
        return std::isfinite(lufs);
    }

    inline double toLufs(double energy)
    {
        return energy > 0 ? -0.691 + 10.0 * std::log10(energy) : none();
    }

    inline double toEnergy(double lufs)
    {
        return isReading(lufs) ? std::pow(10.0, (lufs + 0.691) / 10.0) : 0.0;
    }

    // BS.1770 channel weights: surrounds count 1.41, the LFE not at all. Only a
    // six-channel signal is read as 5.1 (L R C LFE Ls Rs); anything else weighs
    // every channel 1.
    inline double channelWeight(int channel, int channels)
    {
        if (channels == 6)
        {
            if (channel == 3)
                return 0.0;
            if (channel == 4 || channel == 5)
                return 1.41;
        }
        return 1.0;
    }

    // The gated mean of 400 ms blocks (BS.1770-4 section 2.8).
    class Gate
    {
        public:
            void add(double blockEnergy)
            {
                if (blockEnergy > ABSOLUTE_ENERGY)
                    this->blocks.push_back(blockEnergy);
            }

            double integrated() const
            {
                if (this->blocks.empty())
                    return none();

                double sum = 0;
                for (double e : this->blocks)
                    sum += e;
                const double relative = toEnergy(toLufs(sum / this->blocks.size()) - 10.0);

                double gated = 0;
                size_t count = 0;
                for (double e : this->blocks)
                {
                    if (e > relative)
                    {
                        gated += e;
                        count++;
                    }
                }
                return count > 0 ? toLufs(gated / count) : none();
            }

            void reset()
            {
                this->blocks.clear();
            }

        private:
            static constexpr double ABSOLUTE_ENERGY = 1.1724653045822963e-7;   // toEnergy(-70)
            std::vector<double> blocks;
    };

    class Meter
    {
        public:
            void configure(int sampleRate, int channels)
            {
                if (sampleRate == this->rate && channels == this->channelCount)
                    return;

                this->rate = sampleRate;
                this->channelCount = channels;
                this->state.assign(qMax(0, channels), Filter());
                this->subBlockSamples = qMax(1, sampleRate / 10);

                // The two K-weighting stages for this rate (as libebur128 derives them).
                const double pi = 3.14159265358979323846;
                double f0 = 1681.974450955533;
                const double gain = 3.999843853973347;
                double q = 0.7071752369554196;
                double k = std::tan(pi * f0 / sampleRate);
                const double vh = std::pow(10.0, gain / 20.0);
                const double vb = std::pow(vh, 0.4996667741545416);
                double a0 = 1.0 + k / q + k * k;
                this->pb[0] = (vh + vb * k / q + k * k) / a0;
                this->pb[1] = 2.0 * (k * k - vh) / a0;
                this->pb[2] = (vh - vb * k / q + k * k) / a0;
                this->pa[1] = 2.0 * (k * k - 1.0) / a0;
                this->pa[2] = (1.0 - k / q + k * k) / a0;

                f0 = 38.13547087602444;
                q = 0.5003270373238773;
                k = std::tan(pi * f0 / sampleRate);
                a0 = 1.0 + k / q + k * k;
                this->ra[1] = 2.0 * (k * k - 1.0) / a0;
                this->ra[2] = (1.0 - k / q + k * k) / a0;

                reset();
            }

            void reset()
            {
                for (Filter& f : this->state)
                    f = Filter();
                this->subBlocks.clear();
                this->current = 0;
                this->currentSamples = 0;
                this->gate.reset();
                this->measuredSamples = 0;
            }

            // Planar samples, full scale 1.0: channel c's samples start at
            // data + c * stride.
            void addPlanar(const float* data, int stride, int samples)
            {
                if (this->channelCount <= 0 || data == nullptr)
                    return;

                for (int i = 0; i < samples; i++)
                {
                    double energy = 0;
                    for (int c = 0; c < this->channelCount; c++)
                    {
                        const double w = channelWeight(c, this->channelCount);
                        const double y = weigh(c, data[c * stride + i]);
                        if (w > 0)
                            energy += w * y * y;
                    }
                    this->current += energy;
                    if (++this->currentSamples == this->subBlockSamples)
                        closeSubBlock();
                }
                this->measuredSamples += samples;
            }

            // Interleaved samples, full scale 1.0.
            void addInterleaved(const float* data, int samples)
            {
                if (this->channelCount <= 0 || data == nullptr)
                    return;

                for (int i = 0; i < samples; i++)
                {
                    double energy = 0;
                    for (int c = 0; c < this->channelCount; c++)
                    {
                        const double w = channelWeight(c, this->channelCount);
                        const double y = weigh(c, data[i * this->channelCount + c]);
                        if (w > 0)
                            energy += w * y * y;
                    }
                    this->current += energy;
                    if (++this->currentSamples == this->subBlockSamples)
                        closeSubBlock();
                }
                this->measuredSamples += samples;
            }

            double momentary() const { return over(4); }
            double shortTerm() const { return over(30); }
            double integrated() const { return this->gate.integrated(); }

            double seconds() const
            {
                return this->rate > 0 ? double(this->measuredSamples) / this->rate : 0.0;
            }

        private:
            struct Filter
            {
                double x1 = 0, x2 = 0, y1 = 0, y2 = 0;   // pre-filter
                double u1 = 0, u2 = 0, z1 = 0, z2 = 0;   // high-pass
            };

            int rate = 0;
            int channelCount = 0;
            std::vector<Filter> state;
            double pb[3] = {0, 0, 0};
            double pa[3] = {1, 0, 0};
            double ra[3] = {1, 0, 0};

            int subBlockSamples = 0;
            double current = 0;
            int currentSamples = 0;
            QList<double> subBlocks;     // mean energy per 100 ms, newest last, 3 s kept
            Gate gate;
            qint64 measuredSamples = 0;

            double weigh(int c, double x)
            {
                Filter& f = this->state[c];
                const double y = this->pb[0] * x + this->pb[1] * f.x1 + this->pb[2] * f.x2 - this->pa[1] * f.y1 - this->pa[2] * f.y2;
                f.x2 = f.x1;
                f.x1 = x;
                f.y2 = f.y1;
                f.y1 = y;
                const double z = y - 2.0 * f.u1 + f.u2 - this->ra[1] * f.z1 - this->ra[2] * f.z2;
                f.u2 = f.u1;
                f.u1 = y;
                f.z2 = f.z1;
                f.z1 = z;
                return z;
            }

            void closeSubBlock()
            {
                this->subBlocks.append(this->current / this->currentSamples);
                while (this->subBlocks.count() > 30)
                    this->subBlocks.removeFirst();
                this->current = 0;
                this->currentSamples = 0;

                // A 400 ms gating block every 100 ms (75 % overlap).
                if (this->subBlocks.count() >= 4)
                    this->gate.add(meanOfLast(4));
            }

            double meanOfLast(int n) const
            {
                double sum = 0;
                for (int i = this->subBlocks.count() - n; i < this->subBlocks.count(); i++)
                    sum += this->subBlocks.at(i);
                return sum / n;
            }

            double over(int n) const
            {
                if (this->subBlocks.count() < n)
                    return none();
                return toLufs(meanOfLast(n));
            }
    };

    // From OSC peak levels (linear, full scale 1.0, one per audio channel), at
    // whatever rate they come. Time in milliseconds.
    class PeakEstimate
    {
        public:
            void add(const QList<double>& peaks, qint64 nowMs)
            {
                double energy = 0;
                for (int c = 0; c < peaks.count(); c++)
                {
                    const double p = peaks.at(c);
                    energy += channelWeight(c, peaks.count()) * p * p / 2.0;   // a sine's mean square
                }

                const qint64 slot = nowMs / 100;
                if (this->buckets.isEmpty() || slot != this->lastSlot)
                {
                    if (!this->buckets.isEmpty())
                    {
                        // Slots with no reading (a stalled stream) count as nothing heard.
                        for (qint64 s = this->lastSlot + 1; s < slot && s < this->lastSlot + 31; s++)
                            push(Bucket());
                    }
                    push(Bucket());
                    this->lastSlot = slot;
                }
                this->buckets.last().sum += energy;
                this->buckets.last().count++;
            }

            double shortTerm(qint64 nowMs) const
            {
                if (this->buckets.isEmpty() || nowMs / 100 - this->lastSlot > 30)
                    return none();
                return meanOf(30);
            }

            double integrated() const
            {
                return this->gate.integrated();
            }

            void reset()
            {
                this->buckets.clear();
                this->gate.reset();
                this->lastSlot = 0;
            }

        private:
            struct Bucket
            {
                double sum = 0;
                int count = 0;
            };
            QList<Bucket> buckets;
            qint64 lastSlot = 0;
            Gate gate;

            void push(const Bucket& bucket)
            {
                // The bucket being closed - the last one so far - makes a 400 ms
                // block with the three before it.
                if (this->buckets.count() >= 4)
                {
                    double sum = 0;
                    for (int i = this->buckets.count() - 4; i < this->buckets.count(); i++)
                    {
                        const Bucket& b = this->buckets.at(i);
                        sum += b.count > 0 ? b.sum / b.count : 0.0;
                    }
                    this->gate.add(sum / 4);
                }
                this->buckets.append(bucket);
                while (this->buckets.count() > 31)
                    this->buckets.removeFirst();
            }

            // Over the last n 100 ms slots, the one still filling among them.
            double meanOf(int n) const
            {
                const int end = this->buckets.count();
                const int first = qMax(0, end - n);
                double sum = 0;
                int counted = 0;
                for (int i = first; i < end; i++)
                {
                    const Bucket& b = this->buckets.at(i);
                    sum += b.count > 0 ? b.sum / b.count : 0.0;
                    counted++;
                }
                return counted > 0 ? toLufs(sum / counted) : none();
            }
    };

    // As shown: one decimal, or a dash before there is a reading.
    inline QString text(double lufs)
    {
        return isReading(lufs) ? QString::number(lufs, 'f', 1) : QString::fromUtf8("\xe2\x80\x93");
    }
}
