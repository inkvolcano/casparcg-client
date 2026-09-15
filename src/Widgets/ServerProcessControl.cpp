#include "ServerProcessControl.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace ServerProcessControl
{
    static QString normalised(const QString& path)
    {
        QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
        while (clean.endsWith('.'))
            clean.chop(1);
        return clean;
    }

    bool samePath(const QString& left, const QString& right)
    {
        if (left.trimmed().isEmpty() || right.trimmed().isEmpty())
            return false;

        return QString::compare(normalised(left), normalised(right), Qt::CaseInsensitive) == 0;
    }

    QString serverExecutableFor(const QString& configured)
    {
        const QString path = configured.trimmed();
        if (path.isEmpty())
            return QString();

        const QFileInfo info(path);
        if (info.isDir())
            return QDir(info.absoluteFilePath()).filePath("casparcg.exe");

        return path;
    }

    QString scannerPathFor(const QString& serverExecutable)
    {
        if (serverExecutable.trimmed().isEmpty())
            return QString();

        return QDir(QFileInfo(serverExecutable).absolutePath()).filePath("scanner.exe");
    }

    bool supported()
    {
#if defined(Q_OS_WIN)
        return true;
#else
        return false;
#endif
    }

#if defined(Q_OS_WIN)
    static QString imagePathOf(DWORD pid)
    {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (process == nullptr)
            return QString();

        wchar_t buffer[MAX_PATH * 4];
        DWORD size = sizeof(buffer) / sizeof(buffer[0]);
        QString path;
        if (QueryFullProcessImageNameW(process, 0, buffer, &size))
            path = QString::fromWCharArray(buffer, int(size));

        CloseHandle(process);
        return path;
    }
#endif

    QList<Running> runningNamed(const QString& fileName)
    {
        QList<Running> found;

#if defined(Q_OS_WIN)
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return found;

        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (QString::compare(QString::fromWCharArray(entry.szExeFile), fileName, Qt::CaseInsensitive) != 0)
                    continue;

                Running running;
                running.pid = entry.th32ProcessID;
                running.path = imagePathOf(entry.th32ProcessID);
                found.append(running);
            }
            while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
#else
        Q_UNUSED(fileName);
#endif

        return found;
    }

    QList<quint32> runningFrom(const QString& executable)
    {
        QList<quint32> pids;
        if (executable.trimmed().isEmpty())
            return pids;

        for (const Running& running : runningNamed(QFileInfo(executable).fileName()))
        {
            if (samePath(running.path, executable))
                pids.append(running.pid);
        }

        return pids;
    }

    bool isRunning(quint32 pid)
    {
#if defined(Q_OS_WIN)
        HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (process == nullptr)
            return false;

        const bool alive = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
        CloseHandle(process);
        return alive;
#else
        Q_UNUSED(pid);
        return false;
#endif
    }

    bool terminate(quint32 pid)
    {
#if defined(Q_OS_WIN)
        HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (process == nullptr)
            return false;

        const bool accepted = TerminateProcess(process, 1) != 0;
        CloseHandle(process);
        return accepted;
#else
        Q_UNUSED(pid);
        return false;
#endif
    }

    bool startDetached(const QString& executable, const QStringList& arguments)
    {
        const QFileInfo info(executable);
        if (!info.exists())
            return false;

        return QProcess::startDetached(info.absoluteFilePath(), arguments, info.absolutePath());
    }
}
