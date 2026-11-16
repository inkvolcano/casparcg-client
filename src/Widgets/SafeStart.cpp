#include "SafeStart.h"

namespace
{
    bool safeStartOn = false;
}

bool SafeStart::active()
{
    return safeStartOn;
}

void SafeStart::setActive(bool on)
{
    safeStartOn = on;
}
