#pragma once

// The two pure decisions auto-save makes about names, kept here so they can be
// compiled and tested without linking the widgets that use them — the same
// reason PanelFit.h and GitBlobSha.h are header-only.
//
// Both of these handle a string that came from outside: the path a rundown was
// opened from, and the first line of a file on disk. Neither may produce
// something that walks out of the recovery folder.

#include <QtCore/QByteArray>
#include <QtCore/QChar>
#include <QtCore/QFileInfo>
#include <QtCore/QString>

namespace AutoSaveNaming
{
    // The marker the writer puts in front of a recovery copy, carrying the path
    // of the rundown it was made for. It goes inside the file rather than in its
    // name so a filename never has to encode a path.
    inline QByteArray marker()
    {
        return QByteArray("<!-- casparcg-client autosave of: ");
    }

    // The filename stem a recovery copy gets for a rundown at this path.
    inline QString stemFor(const QString& activeRundown)
    {
        // A rundown that was never saved has no name to borrow, so it gets one
        // rather than colliding with the next unnamed one.
        QString stem;
        if (activeRundown.isEmpty())
            stem = QString("Untitled");
        else
            stem = QFileInfo(activeRundown).completeBaseName();

        // Whitelist rather than strip. The input is an arbitrary path, and the
        // result reaches a filesystem call, so it must be unable to carry a
        // separator, a drive letter, or the dots that climb out of a folder.
        QString safe;
        for (int i = 0; i < stem.size(); i++)
        {
            const QChar character = stem.at(i);

            if (character.isLetterOrNumber() || character == QChar('-')
                || character == QChar('_') || character == QChar(' '))
            {
                safe.append(character);
            }
            else
            {
                safe.append(QChar('_'));
            }
        }

        safe = safe.trimmed();
        if (safe.isEmpty())
            safe = QString("Untitled");

        return safe;
    }

    // Reads the original path back out of a recovery file's first line. Returns a
    // null string when the line is not a marker at all, and an empty-but-not-null
    // string when the rundown genuinely had no file of its own — the restore
    // prompt tells those two apart.
    inline QString originalPathFromLine(const QByteArray& firstLine)
    {
        const QByteArray prefix = marker();

        if (!firstLine.startsWith(prefix))
            return QString();

        int end = firstLine.lastIndexOf(" -->");
        if (end < prefix.size())
            return QString();

        QByteArray encoded = firstLine.mid(prefix.size(), end - prefix.size());

        // Percent-encoded on the way in, so a path containing "-->" or a newline
        // cannot end the comment early or spill onto the next line.
        return QString::fromUtf8(QByteArray::fromPercentEncoding(encoded));
    }
}
