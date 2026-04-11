#pragma once

#include "../../Shared.h"

class CORE_EXPORT AssignBankEvent
{
    public:
        explicit AssignBankEvent(int bankId);

        int getBankId() const;

    private:
        int bankId;
};
