#include "TriggerBankRegistry.h"

#include "EventManager.h"
#include "Events/Rundown/BankAssignmentChangedEvent.h"

Q_GLOBAL_STATIC(TriggerBankRegistry, triggerBankRegistry)

TriggerBankRegistry::TriggerBankRegistry()
{
}

TriggerBankRegistry& TriggerBankRegistry::getInstance()
{
    return *triggerBankRegistry();
}

void TriggerBankRegistry::assign(int bankId, QTreeWidgetItem* item)
{
    if (bankId < 1 || bankId > TriggerBank::BANK_COUNT || item == nullptr)
        return;

    // If this item is already assigned to a different bank, remove that mapping.
    int oldBank = getBankForItem(item);
    if (oldBank > 0 && oldBank != bankId)
    {
        assignments.remove(oldBank);
        EventManager::getInstance().fireBankAssignmentChangedEvent(BankAssignmentChangedEvent(oldBank, false));
    }

    // If another item currently holds this bank, remove that mapping.
    if (assignments.contains(bankId))
    {
        QTreeWidgetItem* prev = assignments.value(bankId);
        if (prev != nullptr && prev != item)
            assignments.remove(bankId);
    }

    assignments.insert(bankId, item);
    EventManager::getInstance().fireBankAssignmentChangedEvent(BankAssignmentChangedEvent(bankId, true));
}

void TriggerBankRegistry::unassign(int bankId)
{
    if (bankId < 1 || bankId > TriggerBank::BANK_COUNT)
        return;

    if (!assignments.contains(bankId))
        return;

    assignments.remove(bankId);
    EventManager::getInstance().fireBankAssignmentChangedEvent(BankAssignmentChangedEvent(bankId, false));
}

void TriggerBankRegistry::unassignByItem(QTreeWidgetItem* item)
{
    if (item == nullptr)
        return;

    int bankId = getBankForItem(item);
    if (bankId > 0)
        unassign(bankId);
}

QTreeWidgetItem* TriggerBankRegistry::getItem(int bankId) const
{
    return assignments.value(bankId, nullptr);
}

int TriggerBankRegistry::getBankForItem(QTreeWidgetItem* item) const
{
    if (item == nullptr)
        return 0;

    for (auto it = assignments.constBegin(); it != assignments.constEnd(); ++it)
    {
        if (it.value() == item)
            return it.key();
    }

    return 0;
}

void TriggerBankRegistry::clear()
{
    QList<int> bankIds = assignments.keys();
    for (int bankId : bankIds)
        unassign(bankId);
}

void TriggerBankRegistry::fireBankTriggered(int bankId)
{
    emit bankTriggered(bankId);
}
