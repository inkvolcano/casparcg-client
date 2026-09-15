#pragma once

// A rundown as an undo step keeps it: compressed.
//
// Every structural edit - move, delete, drag, group, paste - stores the whole
// rundown twice, before and after, and a tab keeps fifty such steps. Held as
// text that is two copies of the rundown in UTF-16 per step: for the largest
// real rundown here (520 KB on disk, 1,017 KB in memory) about 100 MB of undo
// history in one tab. Rundown XML is extremely repetitive, so zlib at its
// fastest level takes that copy to 14 KB in 2.35 ms and gives it back in
// 1.04 ms (measured, Qt 6.5.3) - small next to serialising the tree, which
// every edit already does.
//
// Exact: tools/test-undosnapshot checks the round trip on text that is not ASCII
// and on the empty rundown.

#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace UndoSnapshot
{
    inline QByteArray pack(const QString& xml)
    {
        return qCompress(xml.toUtf8(), 1);
    }

    inline QString unpack(const QByteArray& packed)
    {
        return QString::fromUtf8(qUncompress(packed));
    }
}
