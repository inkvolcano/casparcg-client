#pragma once

// The as-run log: what was actually sent to air, and when.
//
// It is taken from the commands themselves, at the moment each one is written to
// a server - not from the buttons. A button press is not when something aired:
// an item with a delay sends later, autoplay and group children are fired without
// any button, and remote triggers, the Shotbox and the Sheets panel each have
// their own route. Every one of them ends in a command to a server, so that is
// where the log is written, with the server's own name on each line.
//
// This header decides which commands are on-air actions and how a line is
// written; src/Widgets/AsRunLogWriter does the writing. Daily files, CSV, kept 30
// days (the user's choice, 2026-09-23). tools/test-asrun checks the rules.

#include <QtCore/QDate>
#include <QtCore/QDateTime>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace AsRun
{
    static const int KEEP_DAYS = 30;

    struct Entry
    {
        int channel = 0;
        int layer = -1;         // -1: the whole channel
        QString action;
        QString what;           // clip, template, source or invoke name; may be empty
    };

    // An AMCP line split into words, with double-quoted parts kept whole and
    // unquoted (\" and \\ inside quotes are unescaped).
    inline QStringList words(const QString& line)
    {
        QStringList out;
        QString current;
        bool quoted = false;
        bool any = false;

        for (int i = 0; i < line.length(); i++)
        {
            const QChar c = line.at(i);
            if (quoted && c == '\\' && i + 1 < line.length())
            {
                current += line.at(++i);
                continue;
            }
            if (c == '"')
            {
                quoted = !quoted;
                any = true;
                continue;
            }
            if (!quoted && c.isSpace())
            {
                if (any || !current.isEmpty())
                    out.append(current);
                current.clear();
                any = false;
                continue;
            }
            current += c;
            any = true;
        }

        if (any || !current.isEmpty())
            out.append(current);

        return out;
    }

    // "1-10" -> channel 1, layer 10; "1" -> channel 1, the whole channel.
    inline bool target(const QString& word, int& channel, int& layer)
    {
        const QStringList parts = word.split('-');
        bool ok = false;
        channel = parts.at(0).toInt(&ok);
        if (!ok || channel <= 0)
            return false;

        layer = -1;
        if (parts.count() > 1)
        {
            layer = parts.at(1).toInt(&ok);
            if (!ok)
                return false;
        }

        return parts.count() <= 2;
    }

    // Whether this command put something on air or took it off, and what. Queries,
    // listings, mixer changes, and loads that show nothing yet are not as-run.
    inline bool fromCommand(const QString& line, Entry& entry)
    {
        const QStringList w = words(line.trimmed());
        if (w.count() < 2)
            return false;

        const QString verb = w.at(0).toUpper();
        if (!target(w.at(1), entry.channel, entry.layer))
            return false;

        auto source = [&w](int index) -> QString {
            if (w.count() <= index)
                return QString();
            const QString first = w.at(index).toUpper();
            if (first == "[HTML]" && w.count() > index + 1)
                return w.at(index + 1);
            if (first == "DECKLINK")
                return w.mid(index).join(' ').section(' ', 0, 2);
            return w.at(index);
        };

        if (verb == "PLAY")
        {
            entry.action = "Play";
            entry.what = source(2);
            return true;
        }

        if (verb == "LOAD")
        {
            entry.action = "Load (first frame on air)";
            entry.what = source(2);
            return true;
        }

        if (verb == "LOADBG")
        {
            // Only AUTO puts it on air: it plays when the clip before it ends,
            // which the server decides. A plain LOADBG is a preload, not air.
            for (int i = 2; i < w.count(); i++)
            {
                if (w.at(i).toUpper() == "AUTO")
                {
                    entry.action = "Queued (plays when the clip before it ends)";
                    entry.what = source(2);
                    return true;
                }
            }
            return false;
        }

        if (verb == "STOP" || verb == "PAUSE" || verb == "RESUME")
        {
            entry.action = verb.left(1) + verb.mid(1).toLower();
            return entry.layer >= 0;
        }

        if (verb == "CLEAR")
        {
            entry.action = entry.layer >= 0 ? QString("Clear") : QString("Clear channel");
            return true;
        }

        if (verb == "CG" && w.count() >= 3)
        {
            const QString sub = w.at(2).toUpper();
            if (sub == "ADD" && w.count() >= 6)
            {
                // CG c-l ADD flashlayer template playOnLoad [data]
                if (w.at(5) != "1")
                    return false;
                entry.action = "Play template";
                entry.what = w.at(4);
                return true;
            }
            if (sub == "PLAY")   { entry.action = "Play template";   return true; }
            if (sub == "STOP")   { entry.action = "Stop template";   return true; }
            if (sub == "NEXT")   { entry.action = "Next";            return true; }
            if (sub == "UPDATE") { entry.action = "Update template"; return true; }
            if (sub == "REMOVE") { entry.action = "Remove template"; return true; }
            if (sub == "CLEAR")  { entry.action = "Clear templates"; return true; }
            if (sub == "INVOKE")
            {
                entry.action = "Invoke";
                entry.what = w.count() > 4 ? w.at(4) : QString();
                return true;
            }
        }

        return false;
    }

    // One CSV field: quoted when it holds a comma, a quote or a line break.
    inline QString csvField(const QString& value)
    {
        if (!value.contains(',') && !value.contains('"') && !value.contains('\n') && !value.contains('\r'))
            return value;

        QString escaped = value;
        escaped.replace("\"", "\"\"");
        return "\"" + escaped + "\"";
    }

    inline QString header()
    {
        return "Date,Time,Server,Channel,Layer,Action,What,Command";
    }

    inline QString csvLine(const QDateTime& when, const QString& server, const Entry& entry, const QString& command)
    {
        QStringList fields;
        fields << when.toString("yyyy-MM-dd")
               << when.toString("HH:mm:ss.zzz")
               << server
               << QString::number(entry.channel)
               << (entry.layer >= 0 ? QString::number(entry.layer) : QString())
               << entry.action
               << entry.what
               << command.trimmed();

        for (QString& field : fields)
            field = csvField(field);

        return fields.join(',');
    }

    inline QString fileName(const QDate& date)
    {
        return QString("AsRun_%1.csv").arg(date.toString("yyyy-MM-dd"));
    }

    inline QDate dateOf(const QString& name)
    {
        static const QRegularExpression pattern("^AsRun_(\\d{4}-\\d{2}-\\d{2})\\.csv$");
        const QRegularExpressionMatch match = pattern.match(name);
        return match.hasMatch() ? QDate::fromString(match.captured(1), "yyyy-MM-dd") : QDate();
    }

    // Days older than keepDays before today. Only exact AsRun_date.csv names with a
    // real date; a file dated in the future is kept.
    inline QStringList expired(const QStringList& names, const QDate& today, int keepDays = KEEP_DAYS)
    {
        QStringList old;
        if (!today.isValid() || keepDays <= 0)
            return old;

        const QDate oldestKept = today.addDays(-keepDays);
        for (const QString& name : names)
        {
            const QDate date = dateOf(name);
            if (date.isValid() && date < oldestKept)
                old.append(name);
        }
        return old;
    }
}
