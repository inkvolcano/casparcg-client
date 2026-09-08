#pragma once

// Whether a rundown will accept something being dropped into it.
//
// Dragging items between two rundowns side by side already works — it arrived
// with split view. What did not arrive with it is the other half of the lock.
//
// A locked rundown refuses keyboard edits, and refuses to be dragged *from*.
// Nothing refused a drop *into* one, so a repository rundown — the case the lock
// exists for, where the content belongs to somebody else — could be edited by
// dragging into it. The lock held every door but that one.
//
// The rule is here rather than inline in the drop handler so it can be stated
// once, tested, and used by both the handler that performs the drop and the
// events that decide what the cursor looks like on the way in. Those two
// disagreeing is its own bug: a cursor that says "yes" over a target that then
// silently does nothing.

#include <QtCore/QString>

namespace DropRules
{
    // What the drag is carrying. Anything else is not ours and is declined
    // without comment, because another application's drag is not a rundown's
    // business.
    inline bool isRundownPayload(bool hasLibraryItem, bool hasRundownItem)
    {
        return hasLibraryItem || hasRundownItem;
    }

    inline bool accepts(bool targetLocked, bool hasLibraryItem, bool hasRundownItem)
    {
        if (!isRundownPayload(hasLibraryItem, hasRundownItem))
            return false;

        // The whole point of the lock.
        if (targetLocked)
            return false;

        return true;
    }

    // Why a drop was refused, for the status bar. Empty when it was accepted, or
    // when it was not a rundown drag at all — a drag from another application
    // ending on the client is not something to explain to the operator.
    inline QString refusalReason(bool targetLocked, bool hasLibraryItem, bool hasRundownItem)
    {
        if (!isRundownPayload(hasLibraryItem, hasRundownItem))
            return QString();

        if (targetLocked)
            return QString("That rundown is locked, so nothing was moved into it.");

        return QString();
    }
}
