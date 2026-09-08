#include "AbstractRundownWidget.h"

#include "Commands/GroupCommand.h"

// clone() is written once per item type and only ever knows about that type's own
// command, so anything living on every command has to be repeated in all forty-odd
// of them or be forgotten in most. It was forgotten: a duplicated item lost its
// Simple Mode button settings.
//
// This is the seam where that is put right. Callers duplicate through here, clone()
// keeps doing the type-specific work, and a property shared by every command is
// carried across in one place rather than in every one of them.
AbstractRundownWidget* AbstractRundownWidget::cloneItem()
{
    AbstractRundownWidget* copy = clone();
    if (copy == nullptr)
        return nullptr;

    AbstractCommand* source = getCommand();
    AbstractCommand* target = copy->getCommand();
    if (source == nullptr || target == nullptr)
        return copy;

    // How the item appears on the Simple Mode surface, but deliberately not *where*:
    // a slot holds one item, so the copy keeps its unassigned slot and settles into
    // the first free one rather than fighting the original for its place.
    target->setShowInSimpleMode(source->getShowInSimpleMode());
    target->setSimpleModeNextButton(source->getSimpleModeNextButton());
    target->setSimpleModeGroupInvokes(source->getSimpleModeGroupInvokes());
    target->setSimpleModeWidth(source->getSimpleModeWidth());
    target->setSimpleModeHeight(source->getSimpleModeHeight());
    target->setSimpleModeLabelSize(source->getSimpleModeLabelSize());
    target->setSimpleModeIcon(source->getSimpleModeIcon());

    if (GroupCommand* sourceGroup = dynamic_cast<GroupCommand*>(source))
    {
        if (GroupCommand* targetGroup = dynamic_cast<GroupCommand*>(target))
        {
            targetGroup->setTreatAsDropdown(sourceGroup->getTreatAsDropdown());
            targetGroup->setDropdownIndex(sourceGroup->getDropdownIndex());
        }
    }

    return copy;
}
