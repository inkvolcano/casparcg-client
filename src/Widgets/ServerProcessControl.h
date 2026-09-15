#pragma once

// Finding, starting and stopping a CasparCG server and its scanner on this
// machine, for the Server Status panel's server menu.
//
// "On this machine" means whoever started them: a shortcut, a service, an
// auto-start or the client itself. A process is matched by the full path of its
// executable, so a server running from another folder - a second install, a test
// copy - is never touched. Everything is started detached: a server started from
// the client keeps running when the client closes or updates itself, which the
// old Start button (a QProcess owned by the panel) did not.
//
// Windows only for finding and stopping, for now; elsewhere nothing is found, so
// the menu only offers what it can do. tools/test-serverprocess runs it against a
// real process.

#include "Shared.h"

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace ServerProcessControl
{
    struct Running
    {
        quint32 pid = 0;
        QString path;
    };

    // The same file, however it was written: separators, case, a trailing dot.
    WIDGETS_EXPORT bool samePath(const QString& left, const QString& right);

    // The server's executable from what Settings holds: the executable itself, or
    // the folder it is in (casparcg.exe there). Empty when nothing is set.
    WIDGETS_EXPORT QString serverExecutableFor(const QString& configured);

    // scanner.exe beside the server's executable.
    WIDGETS_EXPORT QString scannerPathFor(const QString& serverExecutable);

    // Every running process whose executable has this file name ("casparcg.exe"),
    // with its full path.
    WIDGETS_EXPORT QList<Running> runningNamed(const QString& fileName);

    // The running processes started from exactly this executable.
    WIDGETS_EXPORT QList<quint32> runningFrom(const QString& executable);

    WIDGETS_EXPORT bool isRunning(quint32 pid);

    // Ends a process. True when the request was accepted.
    WIDGETS_EXPORT bool terminate(quint32 pid);

    // Starts an executable detached, in its own folder.
    WIDGETS_EXPORT bool startDetached(const QString& executable, const QStringList& arguments = QStringList());

    // Whether finding and stopping processes works on this system.
    WIDGETS_EXPORT bool supported();
}
