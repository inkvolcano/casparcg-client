#pragma once

// The client's own log, read back for Help > Show Log (build 301).
//
// Each entry is written as
//
//   [2026-11-15 09:38:20.246] [8990] [W] QObject::connect: No such slot ...
//
// with the level D(ebug), I(nfo), W(arning), C(ritical) or F(atal). A line that
// does not start with "[" belongs to the entry before it - a long message, a
// stack - so it is kept with it, and filtered with it. Tested by
// tools/test-loglines.

#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace LogLines
{
    struct Entry
    {
        QString time;       // "2026-11-15 09:38:20.246", empty for text before the first entry
        QChar level;        // D, I, W, C, F; '?' when the line has none
        QString text;       // the message, continuation lines included
    };

    enum class Filter { Everything, WarningsAndErrors, Errors };

    inline bool passes(const Entry& entry, Filter filter, const QString& search)
    {
        const QChar level = entry.level;
        if (filter == Filter::WarningsAndErrors && level != 'W' && level != 'C' && level != 'F')
            return false;
        if (filter == Filter::Errors && level != 'C' && level != 'F')
            return false;
        return search.trimmed().isEmpty() || entry.text.contains(search.trimmed(), Qt::CaseInsensitive);
    }

    // Reads log text into entries, in pieces as a growing file is followed.
    // A piece can end in the middle of a line: that part is held until the rest
    // of the line arrives, so a message is never cut in two. The entry being
    // read is held too, until the next one starts - or flush() is called.
    class Reader
    {
        public:
            QList<Entry> feed(const QString& piece)
            {
                static const QRegularExpression head(
                    "^\\[(\\d{4}-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d+)\\]\\s+\\[[^\\]]*\\]\\s+\\[([DIWCF])\\]\\s?(.*)$");

                QList<Entry> done;
                this->rest += piece;

                int from = 0;
                while (true)
                {
                    const int newline = this->rest.indexOf('\n', from);
                    if (newline < 0)
                        break;

                    QString line = this->rest.mid(from, newline - from);
                    from = newline + 1;
                    if (line.endsWith('\r'))
                        line.chop(1);

                    const QRegularExpressionMatch m = head.match(line);
                    if (m.hasMatch())
                    {
                        if (this->hasOpen)
                            done << this->open;
                        this->open = Entry();
                        this->open.time = m.captured(1);
                        this->open.level = m.captured(2).at(0);
                        this->open.text = m.captured(3);
                        this->hasOpen = true;
                    }
                    else if (this->hasOpen)
                    {
                        this->open.text += "\n" + line;
                    }
                    else
                    {
                        this->open = Entry();
                        this->open.level = '?';
                        this->open.text = line;
                        this->hasOpen = true;
                    }
                }

                this->rest = this->rest.mid(from);
                return done;
            }

            // The entry still being read, if any, without finishing it.
            bool peek(Entry* out) const
            {
                if (!this->hasOpen || out == nullptr)
                    return false;
                *out = this->open;
                return true;
            }

            // The entry still being read, and an unfinished last line, as done.
            QList<Entry> flush()
            {
                QList<Entry> done;
                if (!this->rest.isEmpty())
                    done << feed("\n");
                if (this->hasOpen)
                    done << this->open;
                this->hasOpen = false;
                this->open = Entry();
                return done;
            }

        private:
            QString rest;
            Entry open;
            bool hasOpen = false;
    };

    inline QList<Entry> parse(const QString& text)
    {
        Reader reader;
        QList<Entry> entries = reader.feed(text);
        entries << reader.flush();
        return entries;
    }

    inline QString format(const Entry& entry)
    {
        if (entry.time.isEmpty())
            return entry.text;
        return QString("%1  %2  %3").arg(entry.time.mid(11), QString(entry.level), entry.text);
    }
}
