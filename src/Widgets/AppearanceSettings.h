#pragma once

#include "Shared.h"

// The stored colours put where the panels read them (build 316).
//
// ColorCache and ChannelColor live in Common/Global.h as header-only statics,
// and a header-only static is one per module on Windows: the copy Main.cpp set
// at startup was the program's, not the one the panels in this library read.
// That is why custom channel and button colours appeared only once Settings had
// been opened (Settings sets this library's copy). load() sets this library's
// copy; it runs when the main window is made, and again when Settings is
// cancelled or a section is put back to its defaults.
class WIDGETS_EXPORT AppearanceSettings
{
    public:
        static void load();

    private:
        AppearanceSettings() {}
};
