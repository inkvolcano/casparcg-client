#pragma once

#include "../../Shared.h"

class CORE_EXPORT LockRundownEvent
{
    public:
        explicit LockRundownEvent(bool locked);

        bool getLocked() const;

    private:
        bool locked;
};
