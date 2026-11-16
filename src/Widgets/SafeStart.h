#pragma once

#include "Shared.h"

// A safe start: the client up with the parts most likely to fail on one machine
// left off, to get a machine that will not start running again. Chosen by
// holding Shift while the client starts, or offered after a start that did not
// finish (Common/StartupGuard.h).
//
// It leaves off what StartupGuard::safeStartSkips() lists - the auto-save
// recovery offer, NDI, the sheet cache server, the relay, the HTML template
// preview - and changes no setting: the next start is a normal one. Each of
// those places asks active().
class WIDGETS_EXPORT SafeStart
{
    public:
        static bool active();
        static void setActive(bool on);

    private:
        SafeStart() {}
};
