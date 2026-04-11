#pragma once

#include "../../Shared.h"

class CORE_EXPORT BankAssignmentChangedEvent
{
    public:
        explicit BankAssignmentChangedEvent(int bankId, bool assigned);

        int getBankId() const;
        bool getAssigned() const;

    private:
        int bankId;
        bool assigned;
};
