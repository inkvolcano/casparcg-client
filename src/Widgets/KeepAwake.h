#pragma once

#include "Shared.h"

// Keep the machine awake while the client runs (build 307): Windows does not
// sleep, start the screen saver or switch the display off - a playout machine
// that dozes off mid-show is a black screen nobody chose. On unless Settings >
// General says otherwise (KeepAwake). It ends when the client closes; nothing
// in Windows' own power settings is changed. Windows only; elsewhere it does
// nothing.
class WIDGETS_EXPORT KeepAwake
{
    public:
        static bool isEnabled();
        static void setEnabled(bool on);

        // Asks Windows for it, or stops asking, as the setting says.
        static void apply();

    private:
        KeepAwake() {}
};
