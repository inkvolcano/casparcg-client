#pragma once

// Where "From Preview" takes an item's reference picture from (build 298).
//
// Two ways to get a frame of a server's preview channel:
//
//   NDI     the frame of the channel's NDI output, read with this client's own
//           NDI receiver - works for a server anywhere on the network, and needs
//           the channel to have an NDI output in the server's casparcg.config;
//   server  the server writes a still of the channel into its media folder
//           (AMCP ADD <channel> IMAGE), which this client reads back - needs
//           the server's media folder to be reachable from here (Settings >
//           Servers > media path).
//
// The setting (ReferencePictureSource) is Automatic, NDI only or Server only.
// Automatic, the user's rule: NDI when the server's config declares an NDI
// output on that channel, else the server still when the media folder is set,
// else nothing - and say why. Tested by tools/test-referencecapture.

#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace ReferenceCaptureRules
{
    enum class Setting { Automatic, Ndi, Server };

    inline Setting settingFrom(const QString& text)
    {
        if (text == "ndi")
            return Setting::Ndi;
        if (text == "server")
            return Setting::Server;
        return Setting::Automatic;
    }

    inline QString settingText(Setting setting)
    {
        switch (setting)
        {
            case Setting::Ndi: return "ndi";
            case Setting::Server: return "server";
            default: return "auto";
        }
    }

    // What is known about one channel when the button is pressed.
    struct Situation
    {
        int channel = 0;
        QString server;
        bool configRead = false;        // the server's casparcg.config could be read
        bool ndiDeclared = false;       // it has an <ndi> output on this channel
        QString ndiName;                // that output's <name>, empty when not set
        bool ndiAvailable = false;      // NDI runtime here, and not a safe start
        QString ndiSource;              // the discovered NDI source it is, empty if not found
        bool mediaFolderSet = false;    // the server's media path, reachable from here
    };

    enum class Route { None, Ndi, Server };

    struct Plan
    {
        Route route = Route::None;
        QString why;                    // when None: what is missing
    };

    inline QString ndiProblem(const Situation& s)
    {
        if (!s.configRead)
            return QString("the casparcg.config of %1 cannot be read from here, so it is not known whether channel %2 has an NDI output")
                .arg(s.server).arg(s.channel);
        if (!s.ndiDeclared)
            return QString("channel %1 of %2 has no NDI output in its casparcg.config").arg(s.channel).arg(s.server);
        if (!s.ndiAvailable)
            return QString("NDI is not available here (no NDI runtime, or a safe start)");
        if (s.ndiSource.isEmpty())
            return s.ndiName.isEmpty()
                ? QString("channel %1's NDI output has no name, so its source cannot be told apart from others").arg(s.channel)
                : QString("no NDI source called %1 is on the network").arg(s.ndiName);
        return QString();
    }

    inline QString serverProblem(const Situation& s)
    {
        return s.mediaFolderSet ? QString()
            : QString("no media folder is set for %1 (Settings > Servers), so a still it writes cannot be read back").arg(s.server);
    }

    inline Plan plan(Setting setting, const Situation& s)
    {
        const QString ndi = ndiProblem(s);
        const QString server = serverProblem(s);

        switch (setting)
        {
            case Setting::Ndi:
                return ndi.isEmpty() ? Plan{Route::Ndi, QString()} : Plan{Route::None, ndi};
            case Setting::Server:
                return server.isEmpty() ? Plan{Route::Server, QString()} : Plan{Route::None, server};
            default:
                if (ndi.isEmpty())
                    return Plan{Route::Ndi, QString()};
                if (server.isEmpty())
                    return Plan{Route::Server, QString()};
                return Plan{Route::None, QString("%1, and %2.").arg(ndi, server)};
        }
    }

    // The discovered source that is a channel's NDI output. NDI names a source
    // "MACHINE (name)"; the output's <name> is the part in brackets. An output
    // without a name is called "CasparCG" or "CasparCG <n>" by the server, which
    // is only used when exactly one such source is out there.
    inline QString findSource(const QStringList& sources, const QString& ndiName)
    {
        static const QRegularExpression bracket("\\(([^()]*)\\)\\s*$");
        QStringList unnamed;
        for (const QString& source : sources)
        {
            const QRegularExpressionMatch m = bracket.match(source);
            const QString inside = m.hasMatch() ? m.captured(1).trimmed() : source.trimmed();
            if (!ndiName.isEmpty() && QString::compare(inside, ndiName.trimmed(), Qt::CaseInsensitive) == 0)
                return source;
            if (ndiName.isEmpty() && QRegularExpression("^CasparCG( \\d+)?$").match(inside).hasMatch())
                unnamed << source;
        }
        return ndiName.isEmpty() && unnamed.count() == 1 ? unnamed.first() : QString();
    }

    // The still the server writes: a name nobody else would pick, and the file
    // it becomes. CasparCG's image consumer adds .png.
    inline QString stillName(qint64 stamp)
    {
        return QString("casparcg-client-reference-%1").arg(stamp);
    }

    inline QString stillCommand(int channel, const QString& name)
    {
        return QString("ADD %1 IMAGE %2").arg(channel).arg(name);
    }
}
