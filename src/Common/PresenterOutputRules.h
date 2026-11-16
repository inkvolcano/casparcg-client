#pragma once

// The presenter view on a channel (build 308): the live run sheet's presenter
// view played by the server's own HTML producer, so a confidence monitor fed
// from a server output shows what is on air, its countdown and what is next.
//
// The server fetches the page from this machine, so the address in the URL is
// the one the server reaches the client on - the local end of the client's own
// AMCP connection to that server. Pure, so tools/test-presenteroutput can check
// every string the client builds.

#include <QtCore/QString>
#include <QtCore/QUrl>

namespace PresenterOutputRules
{
    // The address as it goes in a URL: an IPv4 address seen through an IPv6
    // socket loses its ::ffff: prefix, a scope id is dropped, IPv6 is bracketed.
    // Empty when there is no address the server could reach.
    inline QString hostForUrl(QString address)
    {
        address = address.trimmed();
        const int scope = address.indexOf('%');
        if (scope >= 0)
            address = address.left(scope);
        if (address.startsWith("::ffff:", Qt::CaseInsensitive) && address.count('.') == 3)
            address = address.mid(7);
        if (address.isEmpty() || address == "0.0.0.0" || address == "::")
            return QString();
        if (address.contains(':'))
            return "[" + address + "]";
        return address;
    }

    // The page the server plays: the presenter view, in its output form (no
    // links, nothing to tap).
    // With a run sheet token set (build 314) the link carries it.
    inline QString pageUrl(const QString& clientAddress, int port, const QString& token = QString())
    {
        const QString host = hostForUrl(clientAddress);
        if (host.isEmpty() || port <= 0 || port > 65535)
            return QString();
        QString url = QString("http://%1:%2/runsheet?view=presenter&output=1").arg(host).arg(port);
        if (!token.trimmed().isEmpty())
            url += "&token=" + QString::fromLatin1(QUrl::toPercentEncoding(token.trimmed()));
        return url;
    }

    inline bool validPlace(int channel, int layer, int channelCount)
    {
        return channel >= 1 && (channelCount <= 0 || channel <= channelCount) && layer >= 0 && layer <= 9999;
    }

    inline QString playCommand(int channel, int layer, const QString& url)
    {
        return QString("PLAY %1-%2 [HTML] \"%3\"").arg(channel).arg(layer).arg(url);
    }

    inline QString clearCommand(int channel, int layer)
    {
        return QString("CLEAR %1-%2").arg(channel).arg(layer);
    }
}
