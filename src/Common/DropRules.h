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
    // Whether a drop from another rundown takes the items out of where they
    // came from. A locked rundown keeps its items: it can be dragged from, and
    // what lands elsewhere is a copy. Refusing the drag outright was the wrong
    // reading of the lock - it protects the locked rundown, not the other one.
    inline bool removesFromSource(bool sourceLocked)
    {
        return !sourceLocked;
    }

    // What the operator is told when the drop was a copy rather than a move.
    inline QString copiedNotice(bool sourceLocked)
    {
        if (!sourceLocked)
            return QString();

        return QString("Copied from the locked rundown; the original stays where it is.");
    }

    inline QString refusalReason(bool targetLocked, bool hasLibraryItem, bool hasRundownItem)
    {
        if (!isRundownPayload(hasLibraryItem, hasRundownItem))
            return QString();

        if (targetLocked)
            return QString("That rundown is locked, so nothing was moved into it.");

        return QString();
    }

    // Where a dropped item lands, read from the cursor.
    //
    // Qt hands a tree's dropMimeData a container and a row number, and only a
    // cursor in the middle of a row names that row: the top and bottom margin
    // of a row name its container instead. The rundown pastes after its current
    // item, so it made the container current - and for a top-level row that is
    // the root, which has no row, and pasting after nothing is the end of the
    // list. That is where a drop a few pixels from a row's edge went, in the
    // original client and every build since.
    //
    // The rule now reads the cursor itself: the upper half of a row lands before
    // it, the lower half after it. The lower half of an open group with children
    // lands first inside it, which is where the eye puts a line drawn under the
    // group's header. Past the last row is the end of the list. The indicator
    // line is drawn by the same rule, so it shows where the drop will go.
    enum class Place { Before, After, Into, End };

    inline Place placeFor(bool overRow, int cursorY, int rowTop, int rowHeight, bool openGroupWithChildren)
    {
        if (!overRow)
            return Place::End;

        if (cursorY < rowTop + rowHeight / 2)
            return Place::Before;

        if (openGroupWithChildren)
            return Place::Into;

        return Place::After;
    }

    // The line's y: the row's top edge when landing before it, its bottom edge
    // otherwise.
    inline int indicatorY(Place place, int rowTop, int rowBottom)
    {
        return place == Place::Before ? rowTop : rowBottom;
    }

    // What the paste adds to the anchor row's number. Before lands on the
    // anchor's own row, pushing it down; After lands on the next. Into anchors
    // on the group's first child and lands on its row.
    inline int pasteOffset(Place place)
    {
        return (place == Place::Before || place == Place::Into) ? 0 : 1;
    }

    // What the operator is told when a drop placed nothing: every dragged item
    // was refused by the paste, which is what happens to a group let go inside
    // a group. The move branches used to delete the originals regardless, and
    // with the upper half of a group's child now landing inside the group that
    // is one drop away. Nothing landed, so nothing is taken.
    inline QString nothingLandedNotice()
    {
        return QString("Nothing was moved: a group cannot be placed inside a group.");
    }
}
