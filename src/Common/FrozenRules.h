#pragma once

// When a playing clip counts as frozen (build 302): the server's OSC time for
// it has not moved for three seconds while it is playing - not paused, and not
// at its end, where a clip that has finished holds its last frame.
//
// A clip only becomes a candidate once at least one OSC time update has arrived
// for it, so a client with OSC monitoring off never flags anything: it is the
// time stopping that counts, not its never having started. If the server stops
// sending OSC altogether, the time stops too - that is a freeze worth seeing.
// Tested by tools/test-frozen.

#include <QtCore/QtGlobal>

namespace FrozenRules
{
    const qint64 FROZEN_AFTER_MS = 3000;
    const double END_MARGIN_SECONDS = 0.5;

    // afterMs is Settings > Frozen clip alarm (build 314); FROZEN_AFTER_MS when unset.
    inline bool isFrozen(qint64 now, qint64 lastChange, double time, double total, bool playing, bool paused,
                         qint64 afterMs = FROZEN_AFTER_MS)
    {
        if (!playing || paused || lastChange <= 0)
            return false;
        if (total > 0 && total - time <= END_MARGIN_SECONDS)
            return false;
        return now - lastChange >= afterMs;
    }
}
