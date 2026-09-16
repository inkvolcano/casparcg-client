// Which daily log files the client deletes: only its own, only past 30 days.

#include "../src/Common/LogRetention.h"

#include <QtCore/QTextStream>

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

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Log retention\n";

    const QDate today(2026, 9, 16);

    expectTrue(LogRetention::dateOf("Client_2026-09-16.log") == today, "a client log name gives its date");
    expectTrue(!LogRetention::dateOf("Client_2026-13-40.log").isValid(), "a name with an impossible date is not a log");
    expectTrue(!LogRetention::dateOf("client_2026-09-16.log").isValid(), "nor is a different case");
    expectTrue(!LogRetention::dateOf("Client_2026-09-16.log.bak").isValid(), "nor a renamed copy");
    expectTrue(!LogRetention::dateOf("Server_2026-09-16.log").isValid(), "nor another program's log");

    const QStringList folder = QStringList()
        << "Client_2026-09-16.log"      // today
        << "Client_2026-08-17.log"      // exactly 30 days ago: kept
        << "Client_2026-08-16.log"      // 31 days ago: deleted
        << "Client_2025-01-01.log"      // long ago: deleted
        << "Client_2026-10-01.log"      // in the future: kept
        << "Client_2025-01-01.log.txt"  // not the pattern: kept
        << "notes.txt"
        << "Client_2025-02-30.log";     // not a real date: kept

    const QStringList old = LogRetention::expired(folder, today);
    expectTrue(old.count() == 2, QString("two files are old enough: %1").arg(old.join(", ")));
    expectTrue(old.contains("Client_2026-08-16.log") && old.contains("Client_2025-01-01.log"), "the 31-day-old and the year-old one");
    expectTrue(!old.contains("Client_2026-08-17.log"), "the file from exactly 30 days ago is kept");
    expectTrue(!old.contains("Client_2026-10-01.log"), "a file dated in the future is kept");
    expectTrue(!old.contains("notes.txt") && !old.contains("Client_2025-01-01.log.txt") && !old.contains("Client_2025-02-30.log"),
               "nothing that is not a client log is ever returned");

    expectTrue(LogRetention::expired(folder, QDate()).isEmpty(), "without a valid today nothing is deleted");
    expectTrue(LogRetention::expired(folder, today, 0).isEmpty(), "and a keep of 0 days deletes nothing rather than everything");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
