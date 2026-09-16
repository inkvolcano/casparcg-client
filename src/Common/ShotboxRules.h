#pragma once

// The rules of the Shotbox panel: what may go in, how many, and where a moved
// row lands. The panel itself needs the whole application; these do not, so
// tools/test-shotbox checks them on their own.
//
// The Shotbox holds its own copies of items - dropped from a rundown or the
// Library - and fires them with its own Play, Stop and Next. One Shotbox for the
// whole client, kept in the database, so it is the same whatever rundown is open.

#include <QtCore/QString>
#include <QtCore/QtGlobal>

namespace ShotboxRules
{
    static const int MAX_ROWS = 8;

    // Groups and gateways only mean something inside a rundown: a group fires its
    // children from the tree, and a gateway jumps to a partner in the tree. A copy
    // of one on its own would have nothing to act on, so they are refused.
    inline bool accepts(const QString& type)
    {
        return type != "GROUP"
            && type != "AUTOPLAYGATEWAY"
            && type != "FOCUSGATEWAY"
            && type != "COMMANDGATEWAY"
            && !type.isEmpty();
    }

    // How many more rows fit.
    inline int room(int rows)
    {
        return qMax(0, MAX_ROWS - rows);
    }

    // Where a row dragged from `from` and dropped at `to` ends up, in a list of
    // `count` rows, or -1 when nothing moves. `to` is the row it was dropped on;
    // dropping below the last row means the end.
    inline int moveTarget(int from, int to, int count)
    {
        if (from < 0 || from >= count || count <= 1)
            return -1;

        const int target = qBound(0, to, count - 1);
        return target == from ? -1 : target;
    }

    // What the status bar says after a drop, or nothing when everything landed.
    inline QString dropNotice(int offered, int added, int refusedTypes)
    {
        QString notice;
        const int full = offered - added - refusedTypes;

        if (refusedTypes > 0)
            notice = refusedTypes == 1 ? QString("A group or gateway cannot go in the Shotbox")
                                       : QString("%1 groups or gateways cannot go in the Shotbox").arg(refusedTypes);

        if (full > 0)
        {
            const QString part = QString("the Shotbox holds %1 items, so %2 %3 not added")
                                     .arg(MAX_ROWS).arg(full).arg(full == 1 ? "was" : "were");
            notice = notice.isEmpty() ? part.left(1).toUpper() + part.mid(1) : notice + "; " + part;
        }

        return notice;
    }
}
