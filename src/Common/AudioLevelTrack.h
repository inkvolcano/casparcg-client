#pragma once

// Audio levels for a file, laid out on a timeline so they can be looked up by
// playback position.
//
// Qt 6.5 gives no way to tap the audio a QMediaPlayer is playing —
// QAudioBufferOutput arrived in 6.8 — so the preview panel cannot meter the
// stream as it goes past. It decodes the file's audio separately instead and
// keeps the peak per channel for each slice of the timeline. The meter then
// indexes that by the player's current position.
//
// Doing it this way rather than metering a live stream has three consequences
// worth having: the meter cannot drift out of sync, because it is addressed by
// position rather than by arrival; scrubbing shows the level at the point you
// scrub to; and a paused frame still shows what that moment sounds like.
//
// Header-only, like PanelFit.h and AutoSaveNaming.h, so the arithmetic can be
// compiled and tested without linking the widgets or Qt Multimedia.

#include <QtCore/QVector>

#include <cmath>

class AudioLevelTrack
{
    public:
        // One peak per channel per slice. 20 ms is 50 readings a second, which is
        // finer than the eye resolves on a meter and keeps an hour of stereo
        // under a megabyte.
        static const int SLICE_MS = 20;

        // The floor the meters draw from, matching AudioMeterWidget so the
        // preview's meters and the server's meters mean the same thing.
        static constexpr double DB_MIN = -61.0;

        // A normalised peak in 0..1 as dBFS, floored rather than running off to
        // negative infinity at digital silence.
        static double dbfsFor(double peak)
        {
            if (peak <= 0.0)
                return DB_MIN;

            double db = 20.0 * std::log10(peak);

            return db < DB_MIN ? DB_MIN : (db > 0.0 ? 0.0 : db);
        }

        void clear()
        {
            this->slices.clear();
            this->channelCount = 0;
        }

        bool isEmpty() const
        {
            return this->slices.isEmpty();
        }

        int channels() const
        {
            return this->channelCount;
        }

        // How much of the file has been analysed. The decode runs in the
        // background, so a long file meters correctly from the start while the
        // rest is still being read.
        qint64 analysedToMs() const
        {
            return static_cast<qint64>(this->slices.size()) * SLICE_MS;
        }

        // Records a peak. Called once per sample during decoding, so it keeps the
        // largest magnitude seen in the slice rather than averaging: a meter that
        // missed a transient would be lying about headroom.
        void addPeak(qint64 timeMs, int channel, double peak)
        {
            if (timeMs < 0 || channel < 0 || channel >= MAX_CHANNELS)
                return;

            if (channel >= this->channelCount)
                this->channelCount = channel + 1;

            int index = static_cast<int>(timeMs / SLICE_MS);

            // Decoded buffers arrive in order, but a gap costs nothing to fill and
            // saves every reader from having to wonder whether one is there.
            while (this->slices.size() <= index)
                this->slices.append(Slice());

            Slice& slice = this->slices[index];
            if (peak > slice.peak[channel])
                slice.peak[channel] = static_cast<float>(peak);
        }

        // The peaks at this position, one per channel, in dBFS. Empty when the
        // position has not been analysed — which the caller shows as no meter
        // rather than as silence, because those are different things.
        QVector<double> dbfsAt(qint64 timeMs) const
        {
            if (this->slices.isEmpty() || timeMs < 0)
                return QVector<double>();

            int index = static_cast<int>(timeMs / SLICE_MS);
            if (index >= this->slices.size())
                return QVector<double>();

            const Slice& slice = this->slices.at(index);

            QVector<double> levels;
            levels.reserve(this->channelCount);
            for (int i = 0; i < this->channelCount; i++)
                levels.append(dbfsFor(slice.peak[i]));

            return levels;
        }

        // A meter that jumped straight to each new reading would flicker. Peaks
        // are taken immediately so nothing is under-reported, and the fall is
        // rate-limited so the eye can follow it.
        static double applyBallistics(double previousDb, double targetDb, double fallDbPerFrame)
        {
            if (targetDb >= previousDb)
                return targetDb;

            double fallen = previousDb - fallDbPerFrame;

            return fallen < targetDb ? targetDb : fallen;
        }

    private:
        // Eight covers every layout CasparCG routinely carries, and fixing the
        // width keeps a slice a flat 32 bytes rather than a heap allocation each.
        static const int MAX_CHANNELS = 8;

        struct Slice
        {
            float peak[MAX_CHANNELS] = {0, 0, 0, 0, 0, 0, 0, 0};
        };

        QVector<Slice> slices;
        int channelCount = 0;
};
