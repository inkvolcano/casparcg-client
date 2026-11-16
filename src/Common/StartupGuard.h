#pragma once

// Did the last start finish, and did the last session close normally?
//
// Two marker files in the client's data folder (%USERPROFILE%\.CasparCG\Client):
//
//   starting.marker  written before the window is built, removed once it has
//                    been on screen for a few seconds;
//   running.marker   written then, removed when the client closes normally.
//
// So at the next start:
//
//   starting.marker still there  the last start never finished - build 289's
//                                crash was one. The client offers a safe start
//                                before building the window again.
//   running.marker still there   the client was up and did not close normally:
//                                a crash, or ended from Task Manager. Said in
//                                the status bar; Help > Collect a Bug Report
//                                then includes what Windows recorded.
//
// Each marker holds the process id that wrote it. A marker whose process is
// still running belongs to another copy of the client that is open now, not to
// a crash, and is left alone. Tested by tools/test-startupguard.

#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace StartupGuard
{
    inline QString startingName() { return QString("starting.marker"); }
    inline QString runningName() { return QString("running.marker"); }

    struct Marker
    {
        bool present = false;
        quint32 pid = 0;
        QDateTime when;
        QString build;
    };

    inline QString markerText(quint32 pid, const QDateTime& when, const QString& build)
    {
        return QString("pid=%1\nwhen=%2\nbuild=%3\n").arg(pid).arg(when.toString(Qt::ISODate), build);
    }

    // A marker that cannot be read still counts as there: a crash may have left
    // it half written.
    inline Marker parseMarker(const QString& text)
    {
        Marker marker;
        marker.present = true;
        for (const QString& line : text.split('\n', Qt::SkipEmptyParts))
        {
            const int equals = line.indexOf('=');
            if (equals < 0)
                continue;
            const QString key = line.left(equals).trimmed();
            const QString value = line.mid(equals + 1).trimmed();
            if (key == "pid")
                marker.pid = value.toUInt();
            else if (key == "when")
                marker.when = QDateTime::fromString(value, Qt::ISODate);
            else if (key == "build")
                marker.build = value;
        }
        return marker;
    }

    enum class Last
    {
        Clean,          // nothing to say
        StartFailed,    // offer a safe start
        DidNotClose     // say so; the bug report has the crash record
    };

    // `alive` says whether a process id is still running (another client open now).
    template <typename Alive>
    inline Last judge(const Marker& starting, const Marker& running, Alive alive)
    {
        const bool startingStale = starting.present && !(starting.pid != 0 && alive(starting.pid));
        const bool runningStale = running.present && !(running.pid != 0 && alive(running.pid));

        if (startingStale)
            return Last::StartFailed;
        if (runningStale)
            return Last::DidNotClose;
        return Last::Clean;
    }

    // What a safe start leaves off, for the question and the status bar.
    inline QStringList safeStartSkips()
    {
        return QStringList()
            << "the offer to recover auto-saved rundowns (they are kept for the next normal start)"
            << "NDI"
            << "the sheet cache server"
            << "relay check-ins and template pulls"
            << "the HTML template preview";
    }
}
