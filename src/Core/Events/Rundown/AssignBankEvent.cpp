#include "AssignBankEvent.h"

AssignBankEvent::AssignBankEvent(int bankId)
    : bankId(bankId)
{
}

int AssignBankEvent::getBankId() const
{
    return this->bankId;
}
