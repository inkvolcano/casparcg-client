#pragma once

// What a level meter does between readings.
//
// The Audio Levels panel draws whatever number arrived last. The server sends
// levels over OSC a handful of times a second, and each one replaces the one
// before it, so the meter shows a sequence of instants rather than a level. Two
// things follow, and both matter on air:
//
//   A transient is invisible. A single frame at -2 dB between two frames at -20
//   is drawn for one refresh and then gone. By the time anybody looks up, the
//   meter is showing -20 and nothing says the peak happened.
//
//   Clipping leaves no trace at all. The one thing a meter exists to tell you is
//   the thing this one forgets fastest.
//
// So: peaks are taken immediately and the fall is rate-limited, which is what
// every hardware meter has done since VU. A peak marker sits at the highest
// recent reading for long enough to be read. And a clip latches - it stays lit
// until somebody clears it, because a clip you missed is exactly the clip you
// needed to know about.
//
// The falls are in dB per second rather than per frame, because OSC arrives when
// it arrives: a meter whose decay depended on the packet rate would fall at a
// different speed on a busy server than on an idle one, which is the sort of bug
// that gets diagnosed as "the meters feel wrong".
//
// Header-only, like PanelFit.h and AudioLevelTrack.h, so the state machine can be
// compiled and tested without linking the widgets, a server, or Qt Multimedia.

#include <QtCore/QtGlobal>

namespace MeterBallistics
{
    // The floor, matching AudioMeterWidget and AudioLevelTrack so the server's
    // meters and the preview's meters mean the same thing.
    inline double dbMin() { return -61.0; }

    // What counts as clipped. Not exactly 0.0: a converter that reports full
    // scale as 0 dBFS will land a hair under it through any float conversion, and
    // a clip indicator that misses real clips is worse than one that is a tenth
    // of a decibel eager.
    inline double clipDb() { return -0.1; }

    // How long the peak marker sits before it starts to fall. Long enough to look
    // up at, short enough not to describe a level that has gone.
    inline int peakHoldMs() { return 1500; }

    // After the hold, the marker falls slowly - it is showing history, and history
    // that vanishes is not worth drawing. The bar itself falls faster, because it
    // is showing now.
    inline double peakFallDbPerSecond() { return 12.0; }
    inline double barFallDbPerSecond() { return 30.0; }

    // How long a reading stands before the meter stops believing it.
    //
    // A quiet signal and a stopped stream look identical for one packet and are
    // opposite things. The server reports continuously while a layer plays, so a
    // level that stops arriving means playback stopped - and a meter frozen on
    // the last level it saw reads as "still playing", which is worse than a
    // meter that is wrong. Past this, the bar falls to silence on its own.
    inline int staleAfterMs() { return 1000; }

    class Meter
    {
        public:
            // Silence, no peak, not clipped. A meter that started at anything else
            // would show a reading before it had one.
            void reset()
            {
                this->levelDb = dbMin();
                this->peakDb = dbMin();
                this->targetDb = dbMin();
                this->lastReadingMs = -1;
                this->peakSetAtMs = -1;
                this->lastAdvanceMs = -1;
                this->isClipped = false;
                this->hasReading = false;
            }

            // A level from the server, in dBFS. Rises are instant: a meter that
            // eased into a transient would under-report it, which is the one
            // error a meter must not make.
            void addReading(double db, qint64 nowMs)
            {
                if (db < dbMin())
                    db = dbMin();
                if (db > 0.0)
                    db = 0.0;

                this->hasReading = true;

                // The target is adopted before the fall is applied, and the order
                // is not cosmetic. Falling towards the previous level first would
                // spend one packet interval at the old reading, and that lag is
                // proportional to the packet rate - which would make the decay
                // depend on how often the server reports, the one thing the
                // per-second rates exist to prevent.
                this->targetDb = db;
                this->lastReadingMs = nowMs;

                advanceTo(nowMs);

                if (db > this->levelDb)
                    this->levelDb = db;

                if (db >= this->peakDb)
                {
                    this->peakDb = db;
                    this->peakSetAtMs = nowMs;
                }

                // Latched. Silence afterwards does not clear it, because the
                // operator's question is "did it clip", not "is it clipping".
                if (db >= clipDb())
                    this->isClipped = true;
            }

            // Moves the fall on to this moment. Called by the repaint timer so the
            // meter keeps falling while nothing is arriving - a stream that stops
            // should decay to silence, not freeze at its last level.
            void advanceTo(qint64 nowMs)
            {
                if (this->lastAdvanceMs < 0)
                {
                    this->lastAdvanceMs = nowMs;
                    return;
                }

                qint64 elapsed = nowMs - this->lastAdvanceMs;
                this->lastAdvanceMs = nowMs;

                // A clock that went backwards - a manual change, an NTP step -
                // must not drive the meter upwards or stall it forever.
                if (elapsed <= 0)
                    return;

                const double seconds = static_cast<double>(elapsed) / 1000.0;

                // The bar falls towards the last level reported, not past it - a
                // steady -40 should read -40. Unless the reports have stopped, in
                // which case there is no level to fall towards and silence is the
                // honest answer.
                const bool stale = this->lastReadingMs < 0
                    || nowMs - this->lastReadingMs > staleAfterMs();
                const double floorNow = stale ? dbMin() : this->targetDb;

                this->levelDb = fallen(this->levelDb, barFallDbPerSecond() * seconds);
                if (this->levelDb < floorNow)
                    this->levelDb = floorNow;

                if (this->peakSetAtMs >= 0 && nowMs - this->peakSetAtMs > peakHoldMs())
                    this->peakDb = fallen(this->peakDb, peakFallDbPerSecond() * seconds);

                // A peak that has fallen to meet the bar is no longer telling you
                // anything the bar is not.
                if (this->peakDb < this->levelDb)
                    this->peakDb = this->levelDb;
            }

            double level() const { return this->levelDb; }
            double peak() const { return this->peakDb; }

            // Whether the peak marker is worth drawing. At the floor it would sit
            // on the bottom of the meter permanently, which reads as a fault.
            bool hasPeak() const
            {
                return this->hasReading && this->peakDb > dbMin();
            }

            bool clipped() const { return this->isClipped; }

            // Cleared by the operator, never by the meter. The whole value of a
            // latch is that it does not decide for you when you have seen it.
            void clearClip() { this->isClipped = false; }

        private:
            static double fallen(double db, double amount)
            {
                double result = db - amount;

                return result < dbMin() ? dbMin() : result;
            }

            double levelDb = -61.0;
            double peakDb = -61.0;
            double targetDb = -61.0;
            qint64 lastReadingMs = -1;
            qint64 peakSetAtMs = -1;
            qint64 lastAdvanceMs = -1;
            bool isClipped = false;
            bool hasReading = false;
    };
}
