// Server refusals shown in the Server Status panel: kept per server, cleared by
// that server's own listings, newest shown, and what to ask each server again.

#include "../src/Common/StandingFailures.h"

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
    out << "Standing failures\n";

    expectTrue(StandingFailures::commandOf("501 CLS FAILED") == "CLS", "501 CLS FAILED is a CLS refusal");
    expectTrue(StandingFailures::commandOf("501 THUMBNAIL LIST FAILED") == "THUMBNAIL LIST", "and THUMBNAIL LIST is its own");
    expectTrue(StandingFailures::commandOf("404 PLAY FAILED") == "*", "a refusal of anything else is filed as *");

    StandingFailures f;
    f.record("Server A", "CLS", "501 CLS FAILED");
    f.record("Server B", "CLS", "501 CLS FAILED on B");
    expectTrue(f.count() == 2, "the same command refused by two servers is two refusals");

    expectTrue(f.listingSucceeded("Server A", "CLS"), "Server A listing again clears something");
    expectTrue(f.count() == 1 && f.banner(false) == "501 CLS FAILED on B", "and only Server A's - Server B is still failing");

    expectTrue(!f.listingSucceeded("Server A", "TLS"), "a listing from a server with nothing standing clears nothing");
    expectTrue(f.count() == 1, "not another server's refusal either");

    f.record("Server B", "*", "404 PLAY FAILED");
    expectTrue(f.banner(false) == "404 PLAY FAILED", "the banner shows the newest refusal, not the alphabetically last");
    f.record("Server B", "CLS", "501 CLS FAILED again");
    expectTrue(f.count() == 2 && f.banner(false) == "501 CLS FAILED again", "refused again, a command moves to newest and is not doubled");

    const auto rechecks = f.toRecheck();
    expectTrue(rechecks.count() == 1 && rechecks.at(0).first == "Server B" && rechecks.at(0).second == "CLS",
               "a * refusal is re-checked with the media list, and CLS once, not twice");

    f.record("Server A", "TLS", "501 TLS FAILED");
    const auto both = f.toRecheck();
    expectTrue(both.count() == 2 && both.contains(qMakePair(QString("Server A"), QString("TLS"))), "each server is asked for its own refused list");

    expectTrue(f.banner(true) == "Server A: 501 TLS FAILED", "with more than one server the banner names it");
    expectTrue(f.tooltip(true).split("\n").count() == 3, "and the tooltip lists every refusal");

    expectTrue(f.listingSucceeded("Server B", "CLS") && f.count() == 1, "Server B's media list clears its CLS and its * refusal together");

    f.keepOnly(QStringList() << "Server B");
    expectTrue(f.isEmpty() && f.toRecheck().isEmpty(), "a server that is removed takes its refusals with it, so nothing is re-checked");
    expectTrue(f.banner(true).isEmpty() && f.tooltip(true).isEmpty(), "and the banner is empty");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
