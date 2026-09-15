// Finding, starting and stopping a server process by its executable's path, for
// real: a copy of PING.EXE named casparcg.exe in a temporary folder stands in for
// the server. Windows only.

#include "../src/Widgets/ServerProcessControl.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTextStream>
#include <QtCore/QThread>

static int checks = 0;
static int failures = 0;

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    out << "Server processes\n";

    using namespace ServerProcessControl;

    expectTrue(samePath("C:\\CasparCG\\Server\\casparcg.exe", "c:/casparcg/server/CASPARCG.EXE"), "the same file written two ways is the same");
    expectTrue(!samePath("C:\\CasparCG\\Server\\casparcg.exe", "C:\\CasparCG\\Server2\\casparcg.exe"), "another folder's copy is not");
    expectTrue(!samePath("", "C:\\x.exe"), "an empty path matches nothing");
    expectTrue(samePath(scannerPathFor("C:\\CasparCG\\Server\\casparcg.exe"), "C:/CasparCG/Server/scanner.exe"), "the scanner is beside the server");
    expectTrue(scannerPathFor("").isEmpty(), "no server, no scanner");
    expectTrue(serverExecutableFor("   ").isEmpty(), "nothing set is no server");
    expectTrue(serverExecutableFor("C:/CasparCG/Server/casparcg.exe") == "C:/CasparCG/Server/casparcg.exe", "an executable is kept as it is");

#if defined(Q_OS_WIN)
    QTemporaryDir sandbox;
    const QString fake = QDir(sandbox.path()).filePath("casparcg.exe");
    const QString elsewhere = QDir(sandbox.path()).filePath("other/casparcg.exe");
    QDir().mkpath(QDir(sandbox.path()).filePath("other"));

    const QString ping = QDir(qEnvironmentVariable("SystemRoot", "C:\\Windows")).filePath("System32/PING.EXE");
    expectTrue(QFile::copy(ping, fake) && QFile::copy(ping, elsewhere), "a stand-in server is in place");

    expectTrue(samePath(serverExecutableFor(sandbox.path()), fake), "a folder means the casparcg.exe in it");
    expectTrue(runningFrom(fake).isEmpty(), "before starting, nothing runs from that path");
    expectTrue(startDetached(fake, QStringList() << "-n" << "60" << "127.0.0.1"), "it starts detached");
    expectTrue(startDetached(elsewhere, QStringList() << "-n" << "60" << "127.0.0.1"), "and a copy in another folder starts too");

    QList<quint32> pids;
    for (int i = 0; i < 50 && pids.isEmpty(); i++)
    {
        QThread::msleep(100);
        pids = runningFrom(fake);
    }
    expectTrue(pids.count() == 1, QString("it is found by its path: %1 process").arg(pids.count()));
    expectTrue(!pids.isEmpty() && isRunning(pids.first()), "and is running");
    expectTrue(runningNamed("casparcg.exe").count() >= 2, "both copies are found by name");

    const QList<quint32> other = runningFrom(elsewhere);
    expectTrue(other.count() == 1 && (pids.isEmpty() || other.first() != pids.first()), "the other folder's copy is a different process");

    expectTrue(!pids.isEmpty() && terminate(pids.first()), "it is stopped");
    bool gone = false;
    for (int i = 0; i < 50 && !gone; i++)
    {
        QThread::msleep(100);
        gone = pids.isEmpty() || !isRunning(pids.first());
    }
    expectTrue(gone && runningFrom(fake).isEmpty(), "and is gone");
    expectTrue(runningFrom(elsewhere).count() == 1, "while the copy in the other folder is untouched");

    for (quint32 pid : runningFrom(elsewhere))
        terminate(pid);
    for (int i = 0; i < 50 && !runningFrom(elsewhere).isEmpty(); i++)
        QThread::msleep(100);
    expectTrue(runningFrom(elsewhere).isEmpty(), "the other copy is cleaned up");
#endif

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
