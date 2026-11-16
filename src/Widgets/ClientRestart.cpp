#include "ClientRestart.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>

#include <QtWidgets/QWidget>

bool ClientRestart::restart(QWidget* mainWindow)
{
#if defined(Q_OS_WIN)
    if (mainWindow == nullptr || !mainWindow->close())
        return false;

    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const qint64 pid = QCoreApplication::applicationPid();
    const QString scriptPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("casparcg-client-restart.cmd");

    // Wait for this process (by its id, not its name: another client may be
    // running) for up to a minute, then start the client from where it ran.
    QString text;
    text += "@echo off\r\n";
    text += "set /a TRIES=0\r\n";
    text += ":wait\r\n";
    text += "set /a TRIES+=1\r\n";
    text += "if %TRIES% GTR 60 exit /b 1\r\n";
    text += QString("tasklist /fi \"PID eq %1\" 2>nul | find \"%1\" >nul\r\n").arg(pid);
    text += "if errorlevel 1 goto :start\r\n";
    text += "ping -n 2 127.0.0.1 >nul\r\n";
    text += "goto :wait\r\n";
    text += ":start\r\n";
    text += "start \"\" \"" + exe + "\"\r\n";

    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    script.write(text.toLocal8Bit());
    script.close();

    if (!QProcess::startDetached("cmd.exe", QStringList() << "/c" << QDir::toNativeSeparators(scriptPath)))
        return false;

    QCoreApplication::quit();
    return true;
#else
    Q_UNUSED(mainWindow);
    return false;
#endif
}
