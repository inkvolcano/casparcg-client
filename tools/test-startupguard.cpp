// Did the last start finish, and did the last session close normally: the two
// startup markers, and what the client makes of them at the next start.

#include "../src/Common/StartupGuard.h"

#include <QtCore/QSet>
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

using namespace StartupGuard;

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Startup guard\n";

    const QDateTime when(QDate(2026, 11, 16), QTime(9, 54, 1));
    const Marker parsed = parseMarker(markerText(14952, when, "293"));
    expectTrue(parsed.present && parsed.pid == 14952 && parsed.when == when && parsed.build == "293", "a marker reads back what was written");

    const Marker broken = parseMarker("pid=");
    expectTrue(broken.present && broken.pid == 0, "a half-written marker still counts as there");

    const Marker none;
    QSet<quint32> running = { 7000 };
    auto alive = [&running](quint32 pid) { return running.contains(pid); };

    Marker starting = parseMarker(markerText(14952, when, "293"));
    Marker session = parseMarker(markerText(14952, when, "293"));

    expectTrue(judge(none, none, alive) == Last::Clean, "no markers: the last session closed normally");
    expectTrue(judge(starting, none, alive) == Last::StartFailed, "a starting marker left behind: the last start did not finish");
    expectTrue(judge(starting, session, alive) == Last::StartFailed, "both left: the start failing is what matters");
    expectTrue(judge(none, session, alive) == Last::DidNotClose, "a running marker left behind: it did not close normally");
    expectTrue(judge(broken, none, alive) == Last::StartFailed, "a half-written starting marker is a failed start");

    // Another copy of the client is open now: its markers are its own business.
    Marker other = parseMarker(markerText(7000, when, "293"));
    expectTrue(judge(other, none, alive) == Last::Clean, "a starting marker of a client that is running now is not a crash");
    expectTrue(judge(none, other, alive) == Last::Clean, "nor is its running marker");

    expectTrue(safeStartSkips().count() == 5 && safeStartSkips().join(" ").contains("NDI"), "what a safe start leaves off is said");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
