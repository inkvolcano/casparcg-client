#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QStringList>

// Whether a template file is read and scanned on selection, and a memory of
// what the scan found so a file is read once, not on every click.
//
// The Inspector's Invoke section learns a template's functions by reading its
// HTML and running three regular expressions over it, and it did that on every
// selection of the item. A self-contained page with its media embedded - the
// Dreamforce rolling graphics are 18 to 28 MB each - took twenty seconds to
// read, decode to UTF-16 and scan, on the interface thread, per click. Every
// other template is under 100 KB, which is why only those hung.
//
// Three rules, all here so they can be tested on their own:
//   - the file is read with readText() below. The twenty seconds were not the
//     file: QTextStream::readAll() on a QFile grows and re-decodes as it goes,
//     and takes 10 to 24 seconds on 18 to 28 MB (measured on Qt 6.5.3). The
//     same file as one readAll() and one fromUtf8() takes 40 ms;
//   - a file over the limit is not scanned on selection; the operator presses
//     Discover Functions to scan it on purpose, and is told why it was skipped;
//   - a file that has been scanned is not read again while its size and
//     modification time are unchanged.
namespace TemplateScan
{
    // The whole file as text, the fast way. Line endings are kept as they are:
    // every reader of these files matches on \s, which takes a \r. A UTF-8
    // byte-order mark is dropped, as QTextStream would have dropped it.
    inline bool readText(const QString& path, QString* content)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return false;

        *content = QString::fromUtf8(file.readAll());
        if (content->startsWith(QChar(0xFEFF)))
            content->remove(0, 1);

        return true;
    }

    // The file's bytes with every base64 payload taken out: from "base64," up to
    // the first byte that cannot be part of base64 ([A-Za-z0-9+/=]). What is left
    // is the template's own markup and script.
    //
    // The rolling graphics are 18 to 28 MB of which all but about 2 KB is embedded
    // images. Reading, decoding and running the selection scans over the whole of
    // one took most of a second, measured on the real files, and the client's own
    // log showed selections of 587 to 967 ms after dropping them into a rundown.
    // Without the payloads the same scans run in about a millisecond and find
    // exactly the same matches: a declaration needs spaces, parentheses, braces or
    // quotes, and a base64 payload holds none, so no match can start, end or cross
    // inside one. tools/test-templatescan holds that.
    //
    // Only for scans that look for names and declarations. Anything that reads a
    // value out of the template - window.debugData defaults, which can themselves
    // be data URIs - must read the whole file with readText.
    inline QByteArray withoutBase64Payloads(const QByteArray& bytes)
    {
        static const QByteArray marker("base64,");

        QByteArray out;
        int from = 0;
        while (true)
        {
            int at = bytes.indexOf(marker, from);
            if (at < 0)
            {
                if (from == 0)
                    return bytes;   // nothing to take out: no copy

                out.append(bytes.constData() + from, bytes.size() - from);
                return out;
            }

            if (out.isEmpty())
                out.reserve(4096);

            at += marker.size();
            out.append(bytes.constData() + from, at - from);

            const char* data = bytes.constData();
            const int size = bytes.size();
            int end = at;
            while (end < size)
            {
                const char c = data[end];
                const bool base64 = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                    || c == '+' || c == '/' || c == '=';
                if (!base64)
                    break;
                ++end;
            }

            from = end;
        }
    }

    // readText, with the base64 payloads left out before decoding - for the scans
    // that only look for names and declarations (see withoutBase64Payloads).
    //
    // The result is remembered per file while its size and modification time are
    // unchanged: a first selection runs two scans (functions, sheet declaration)
    // that each read the file, and the same template selected again in another
    // rundown reads it again. Without its images a template is a few KB, so this
    // holds little; it is capped at 64 files all the same.
    inline bool readScannable(const QString& path, QString* content)
    {
        struct Held
        {
            qint64 size = -1;
            QDateTime modified;
            QString text;
        };
        static QHash<QString, Held> held;

        const QFileInfo info(path);
        if (!info.exists())
            return false;

        const auto found = held.constFind(path);
        if (found != held.constEnd() && found->size == info.size() && found->modified == info.lastModified())
        {
            *content = found->text;
            return true;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return false;

        *content = QString::fromUtf8(withoutBase64Payloads(file.readAll()));
        if (content->startsWith(QChar(0xFEFF)))
            content->remove(0, 1);

        if (held.size() >= 64 && !held.contains(path))
            held.clear();

        Held entry;
        entry.size = info.size();
        entry.modified = info.lastModified();
        entry.text = *content;
        held.insert(path, entry);

        return true;
    }

    // Above this a template is not scanned when it is merely selected. With the
    // read at 40 ms and the scan itself under half a second on a 28 MB page,
    // the rolling graphics fit under it and are scanned once, then remembered;
    // the limit is there for a file no template should ever be.
    const qint64 SCAN_ON_SELECTION_LIMIT = 64 * 1024 * 1024;

    inline bool scanOnSelection(qint64 fileSize)
    {
        return fileSize <= SCAN_ON_SELECTION_LIMIT;
    }

    // What to tell the operator when a selection skipped the scan.
    inline QString skippedNotice(qint64 fileSize)
    {
        return QString("This template is %1 MB, so its functions were not scanned on selection. "
                       "Press Discover Functions to scan it.")
            .arg(QString::number(fileSize / (1024.0 * 1024.0), 'f', 1));
    }

    // One scanned file. Fresh while the file on disk has the same size and time.
    struct Entry
    {
        qint64 size = -1;
        QDateTime modified;
        QStringList functions;

        bool matches(const QFileInfo& info) const
        {
            return this->size == info.size() && this->modified == info.lastModified();
        }
    };

    class Cache
    {
        public:
            // The functions found before for this file, if it has not changed since.
            bool lookup(const QString& path, const QFileInfo& info, QStringList* functions) const
            {
                if (!this->entries.contains(path))
                    return false;

                const Entry& entry = this->entries.value(path);
                if (!entry.matches(info))
                    return false;

                if (functions != nullptr)
                    *functions = entry.functions;

                return true;
            }

            void remember(const QString& path, const QFileInfo& info, const QStringList& functions)
            {
                Entry entry;
                entry.size = info.size();
                entry.modified = info.lastModified();
                entry.functions = functions;
                this->entries.insert(path, entry);
            }

            int count() const { return this->entries.count(); }

        private:
            QHash<QString, Entry> entries;
    };
}
