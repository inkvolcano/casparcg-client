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
// Reference images (ReferenceImage.h) are the exception to "repetitive": a
// picture is 30-60 KB of base64 that zlib barely shrinks, and a rundown of
// lower thirds can carry twenty. Kept in every step, that is two copies of all
// of them per edit, fifty edits deep. So each picture is taken out of the step
// and held once, shared by every step that has it, and the step keeps a short
// key in its place. Pictures are not let go during the session - a rundown's
// worth is small next to fifty copies of it.
//
// Exact: tools/test-undosnapshot checks the round trip on text that is not ASCII,
// on the empty rundown, and with pictures in it.

#include <QtCore/QByteArray>
#include <QtCore/QCryptographicHash>
#include <QtCore/QHash>
#include <QtCore/QString>

namespace UndoSnapshot
{
    // Key -> picture, for every picture any step has held.
    inline QHash<QString, QString>& pictures()
    {
        static QHash<QString, QString> held;
        return held;
    }

    static const char* const OPEN = "<referenceimage>";
    static const char* const CLOSE = "</referenceimage>";

    // Each picture replaced by "@" and its SHA-1. A real picture starts with
    // "data:", so a key can never be mistaken for one.
    inline QString withoutPictures(const QString& xml)
    {
        const QString open = QString::fromLatin1(OPEN);
        const QString close = QString::fromLatin1(CLOSE);

        int at = xml.indexOf(open);
        if (at < 0)
            return xml;

        QString out;
        out.reserve(xml.size());
        int from = 0;
        while (at >= 0)
        {
            const int start = at + open.size();
            const int end = xml.indexOf(close, start);
            if (end < 0)
                break;

            const QString picture = xml.mid(start, end - start);
            const QString key = "@" + QString::fromLatin1(
                QCryptographicHash::hash(picture.toUtf8(), QCryptographicHash::Sha1).toHex());
            pictures().insert(key, picture);

            out += QStringView(xml).mid(from, start - from);
            out += key;
            from = end;
            at = xml.indexOf(open, end + close.size());
        }

        out += QStringView(xml).mid(from);
        return out;
    }

    inline QString withPictures(const QString& xml)
    {
        const QString open = QString::fromLatin1(OPEN);
        const QString close = QString::fromLatin1(CLOSE);

        int at = xml.indexOf(open);
        if (at < 0)
            return xml;

        QString out;
        int from = 0;
        while (at >= 0)
        {
            const int start = at + open.size();
            const int end = xml.indexOf(close, start);
            if (end < 0)
                break;

            const QString key = xml.mid(start, end - start);
            out += QStringView(xml).mid(from, start - from);
            out += key.startsWith('@') ? pictures().value(key) : key;
            from = end;
            at = xml.indexOf(open, end + close.size());
        }

        out += QStringView(xml).mid(from);
        return out;
    }

    inline QByteArray pack(const QString& xml)
    {
        return qCompress(withoutPictures(xml).toUtf8(), 1);
    }

    inline QString unpack(const QByteArray& packed)
    {
        return withPictures(QString::fromUtf8(qUncompress(packed)));
    }
}
