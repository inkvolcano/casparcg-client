#pragma once

// The server refusals the Server Status panel is showing, per server, in the order
// they happened, and what to ask each server again so they clear on their own.
//
// Build 224 cleared a refusal when the same listing next succeeded. Two things
// were still wrong with it:
//   - It only re-checked when something else refreshed the library - startup, a
//     reconnect, Ctrl+R, or the auto-refresh, which is off by default - so a
//     message stayed up long after the server was fine again.
//   - Refusals were not kept per server, so one server's good listing cleared
//     another server's message while that server was still failing. And they
//     were held in a map ordered by command name, so "the newest" the banner
//     meant to show was really the alphabetically last.
//
// tools/test-standingfailures checks the rules.

#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QStringList>

class StandingFailures
{
    public:
        // The commands a server lists with. A refusal of anything else is filed as
        // "*" for that server.
        static QStringList listings()
        {
            return QStringList() << "CLS" << "TLS" << "DATA LIST" << "THUMBNAIL LIST";
        }

        // A refusal, from its response line: "501 CLS FAILED" -> "CLS".
        static QString commandOf(const QString& line)
        {
            QStringList words = line.trimmed().toUpper().split(' ', Qt::SkipEmptyParts);
            if (!words.isEmpty() && words.first().toInt() > 0)
                words.removeFirst();
            if (!words.isEmpty() && (words.last() == "FAILED" || words.last() == "ERROR"))
                words.removeLast();

            const QString command = words.join(' ');
            return listings().contains(command) ? command : QString("*");
        }

        // A server refused a command. The same server and command replaces the
        // earlier one and becomes the newest.
        void record(const QString& server, const QString& command, const QString& text)
        {
            forget(server, command);

            Entry entry;
            entry.server = server;
            entry.command = command;
            entry.text = text;
            this->entries.append(entry);
        }

        // A server answered a listing: its refusal of that command is over, and so
        // is any other refusal from that server, since it is answering again. Other
        // servers' refusals stay. Returns whether anything cleared.
        bool listingSucceeded(const QString& server, const QString& command)
        {
            const int before = this->entries.count();
            forget(server, command);
            forget(server, QStringLiteral("*"));
            return this->entries.count() != before;
        }

        // A server that is no longer configured takes its refusals with it.
        void keepOnly(const QStringList& servers)
        {
            for (int i = this->entries.count() - 1; i >= 0; i--)
            {
                if (!servers.contains(this->entries.at(i).server))
                    this->entries.removeAt(i);
            }
        }

        bool isEmpty() const { return this->entries.isEmpty(); }
        int count() const { return this->entries.count(); }

        // What to ask again, once per server and command: the refused listing
        // itself, or the media list for a refusal of anything else - a server that
        // answers it is answering, which is what clears such a refusal.
        QList<QPair<QString, QString>> toRecheck() const
        {
            QList<QPair<QString, QString>> checks;
            for (const Entry& entry : this->entries)
            {
                const QPair<QString, QString> check(entry.server, entry.command == "*" ? QString("CLS") : entry.command);
                if (!checks.contains(check))
                    checks.append(check);
            }
            return checks;
        }

        // The banner shows the newest refusal; the tooltip lists them all. The
        // server is named when there is more than one to tell apart.
        QString banner(bool nameServers) const
        {
            return this->entries.isEmpty() ? QString() : line(this->entries.last(), nameServers);
        }

        QString tooltip(bool nameServers) const
        {
            QStringList lines;
            for (const Entry& entry : this->entries)
                lines.append(line(entry, nameServers));
            return lines.join("\n");
        }

    private:
        struct Entry
        {
            QString server;
            QString command;
            QString text;
        };

        QList<Entry> entries;

        void forget(const QString& server, const QString& command)
        {
            for (int i = this->entries.count() - 1; i >= 0; i--)
            {
                if (this->entries.at(i).server == server && this->entries.at(i).command == command)
                    this->entries.removeAt(i);
            }
        }

        static QString line(const Entry& entry, bool nameServers)
        {
            return nameServers ? QString("%1: %2").arg(entry.server, entry.text) : entry.text;
        }
};
