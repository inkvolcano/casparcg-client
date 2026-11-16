#pragma once

// What stops a rundown from going to air as meant, for Rundown > Check Before
// Going Live.
//
// The window gathers the rundown's items and the state of every server, and
// these rules turn them into findings:
//
//   - an item for a server this client does not have;
//   - a server that is not connected, once per server, with how many items use it;
//   - an item aimed at a locked channel: a lock stops playout there, so that item
//     will not play until the channel is unlocked.
//
// Missing media and templates come from MissingMediaScanner, the same check as
// the row warnings, and sheet sources from a read of each one; the window adds
// those. Disabled items are left out - they do not fire anyway.
// Tested by tools/test-readiness.

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <functional>

namespace ReadinessRules
{
    struct Item
    {
        int id = -1;          // the window's own index, to select the row
        QString label;
        QString device;
        int channel = 0;
        bool disabled = false;
    };

    struct Server
    {
        bool known = false;
        bool connected = false;
        QSet<int> lockedChannels;
    };

    enum class Kind { Server, Locked, Media, Sheet };

    struct Finding
    {
        Kind kind = Kind::Server;
        int item = -1;        // the row it is about, or the first of several
        QString text;
    };

    inline QList<Finding> check(const QList<Item>& items, const std::function<Server(const QString&)>& serverNamed)
    {
        QList<Finding> findings;
        QMap<QString, QList<int>> byServer;
        QMap<QString, Server> servers;

        for (const Item& item : items)
        {
            if (item.disabled || item.device.isEmpty())
                continue;
            if (!servers.contains(item.device))
                servers.insert(item.device, serverNamed(item.device));
            byServer[item.device].append(item.id);
        }

        for (auto it = byServer.constBegin(); it != byServer.constEnd(); ++it)
        {
            const Server server = servers.value(it.key());
            const int count = it.value().count();
            const QString uses = count == 1 ? QString("1 item uses it") : QString("%1 items use it").arg(count);
            if (!server.known)
                findings << Finding{Kind::Server, it.value().first(),
                                    QString("Server '%1' is not set up in this client - %2.").arg(it.key(), uses)};
            else if (!server.connected)
                findings << Finding{Kind::Server, it.value().first(),
                                    QString("Server '%1' is not connected - %2.").arg(it.key(), uses)};
        }

        for (const Item& item : items)
        {
            if (item.disabled || item.device.isEmpty() || item.channel <= 0)
                continue;
            const Server server = servers.value(item.device);
            if (server.known && server.lockedChannels.contains(item.channel))
                findings << Finding{Kind::Locked, item.id,
                                    QString("'%1' goes to channel %2 on %3, which is locked: it will not play until it is unlocked.")
                                        .arg(item.label).arg(item.channel).arg(item.device)};
        }

        return findings;
    }

    // "Ready: nothing found." or "4 things to look at: 2 missing media, 1 server, 1 locked channel."
    inline QString summary(const QList<Finding>& findings)
    {
        if (findings.isEmpty())
            return QString("Ready: nothing found that would stop this rundown.");

        int servers = 0, locked = 0, media = 0, sheets = 0;
        for (const Finding& finding : findings)
        {
            switch (finding.kind)
            {
                case Kind::Server: servers++; break;
                case Kind::Locked: locked++; break;
                case Kind::Media: media++; break;
                case Kind::Sheet: sheets++; break;
            }
        }

        QStringList parts;
        if (media > 0)
            parts << QString("%1 missing media or template%2").arg(media).arg(media == 1 ? "" : "s");
        if (servers > 0)
            parts << QString("%1 server%2").arg(servers).arg(servers == 1 ? "" : "s");
        if (locked > 0)
            parts << QString("%1 item%2 on a locked channel").arg(locked).arg(locked == 1 ? "" : "s");
        if (sheets > 0)
            parts << QString("%1 sheet source%2 not answering").arg(sheets).arg(sheets == 1 ? "" : "s");

        return QString("%1 to look at: %2.")
            .arg(findings.count() == 1 ? QString("1 thing") : QString("%1 things").arg(findings.count()), parts.join(", "));
    }
}
