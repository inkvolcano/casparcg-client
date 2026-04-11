#include "LockRundownEvent.h"

#include "Global.h"

LockRundownEvent::LockRundownEvent(bool locked)
    : locked(locked)
{
}

bool LockRundownEvent::getLocked() const
{
    return this->locked;
}
