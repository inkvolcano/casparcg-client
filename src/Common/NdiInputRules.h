#pragma once

// The NDI Input item: what it sends the server, and how the server's list of
// NDI sources is read. Header-only so tools/test-ndiinput.cpp can check it
// without the Core library.
//
// Measured against CasparCG Server 2.5 (build 321):
//   PLAY 2-10 [NDI] "MACHINE (Source)"           plays the source
//   ... LOW_BANDWIDTH                            the source's low-bandwidth stream
//   NDI LIST                                     200 NDI LIST OK, then one line per
//                                                source: 1 "MACHINE (Source)" 10.0.0.5:5961
//                                                and an empty line to end it.
// A source that does not exist is accepted (202) and stays black until it
// appears, so the item cannot tell a typo from a source that is not up yet.

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace NdiInputRules
{
    // AMCP strings are quoted; a quote or backslash inside the name is escaped.
    inline QString quoted(const QString& source)
    {
        QString escaped = source;
        escaped.replace("\\", "\\\\");
        escaped.replace("\"", "\\\"");
        return "\"" + escaped + "\"";
    }

    // verb is PLAY or LOADBG. The transition words are the ones every other item
    // sends (MIX 20 Linear RIGHT); CUT with any duration is a plain cut.
    inline QString command(const QString& verb, int channel, int videolayer, const QString& source,
                           const QString& transition, int duration, const QString& tween,
                           const QString& direction, bool lowBandwidth)
    {
        QString text = QString("%1 %2-%3 [NDI] %4").arg(verb).arg(channel).arg(videolayer).arg(quoted(source.trimmed()));
        if (!transition.isEmpty())
            text += QString(" %1 %2 %3 %4").arg(transition).arg(duration).arg(tween).arg(direction);
        if (lowBandwidth)
            text += " LOW_BANDWIDTH";
        return text.trimmed();
    }

    // The names out of an NDI LIST reply, in the server's order, without
    // duplicates. Lines that are not "<number> "<name>" <address>" are skipped,
    // so the status line, the empty end line and anything unexpected fall out.
    inline QStringList parseList(const QString& reply)
    {
        QStringList names;
        const QStringList lines = reply.split('\n');
        for (QString line : lines)
        {
            line = line.trimmed();
            const int open = line.indexOf('"');
            const int close = line.lastIndexOf('"');
            if (open <= 0 || close <= open + 1)
                continue;

            bool numbered = false;
            line.left(open).trimmed().toInt(&numbered);
            if (!numbered)
                continue;

            const QString name = line.mid(open + 1, close - open - 1).trimmed();
            if (!name.isEmpty() && !names.contains(name))
                names << name;
        }
        return names;
    }

    // Whether a reply has come in whole: the status line and then the empty line
    // the server ends a list with. A failed list is one line.
    inline bool replyComplete(const QString& reply)
    {
        if (reply.startsWith("501") || reply.startsWith("400") || reply.startsWith("404"))
            return reply.contains("\n");
        return reply.contains("\r\n\r\n") || reply.contains("\n\n");
    }
}
