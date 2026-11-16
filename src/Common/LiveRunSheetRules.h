#pragma once

// What the live run sheet page says about one item right now (build 305).
//
//   a clip (movie, audio) that is playing or paused: the server's own time for it,
//   as its row has it - time left is its length less where it is;
//   anything else that was fired and has a length (a Duration, or a group's
//   planned length): counted down from the moment it was fired, and no longer
//   on air once that time has run out;
//   anything else that was fired: on air, with no end to count to, until it is
//   stopped or cleared.
//
// Tested by tools/test-liverunsheet.

#include <QtCore/QString>
#include <QtCore/QtGlobal>

namespace LiveRunSheetRules
{
    enum class State { Idle, OnAir, Playing, Paused };

    struct Live
    {
        State state = State::Idle;
        double remaining = -1;     // seconds left; -1 = no end known
    };

    // `clipKnown`: the row is a clip and is playing or paused, with `elapsed` and
    // `total` from the server. `firedAt`: when it was fired (ms since epoch), -1
    // for never or stopped since. `length`: its length in seconds, -1 for none.
    inline Live liveState(bool clipKnown, double elapsed, double total, bool paused,
                          qint64 firedAt, double length, qint64 now)
    {
        Live live;
        if (clipKnown)
        {
            live.state = paused ? State::Paused : State::Playing;
            live.remaining = total > 0 ? qMax(0.0, total - elapsed) : -1;
            return live;
        }

        if (firedAt < 0)
            return live;

        if (length > 0)
        {
            const double left = (double(firedAt) + length * 1000.0 - double(now)) / 1000.0;
            if (left <= 0)
                return live;   // its time is up: off air
            live.state = State::OnAir;
            live.remaining = left;
            return live;
        }

        live.state = State::OnAir;
        return live;
    }

    inline QString stateText(State state)
    {
        switch (state)
        {
            case State::Playing: return "playing";
            case State::Paused: return "paused";
            case State::OnAir: return "onair";
            default: return QString();
        }
    }
}
