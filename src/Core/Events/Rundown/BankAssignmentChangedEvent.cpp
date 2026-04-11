#include "BankAssignmentChangedEvent.h"

BankAssignmentChangedEvent::BankAssignmentChangedEvent(int bankId, bool assigned)
    : bankId(bankId), assigned(assigned)
{
}

int BankAssignmentChangedEvent::getBankId() const
{
    return this->bankId;
}

bool BankAssignmentChangedEvent::getAssigned() const
{
    return this->assigned;
}
