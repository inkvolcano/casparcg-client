// Rundown > Check Before Going Live: servers that are missing or not
// connected, items aimed at a locked channel, and the summary line.

#include "../src/Common/ReadinessRules.h"

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

using namespace ReadinessRules;

static Item item(int id, const QString& label, const QString& device, int channel, bool disabled = false)
{
    Item i;
    i.id = id;
    i.label = label;
    i.device = device;
    i.channel = channel;
    i.disabled = disabled;
    return i;
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Readiness\n";

    auto servers = [](const QString& name) {
        Server server;
        if (name == "Main")
        {
            server.known = true;
            server.connected = true;
            server.lockedChannels.insert(2);
        }
        else if (name == "Backup")
        {
            server.known = true;
            server.connected = false;
        }
        return server;
    };

    QList<Item> items;
    items << item(0, "Opener", "Main", 1)
          << item(1, "Lower third", "Main", 2)
          << item(2, "Logo", "Backup", 1)
          << item(3, "Clock", "Backup", 1)
          << item(4, "Old sting", "Gone", 1)
          << item(5, "Spare", "Main", 2, true)
          << item(6, "Separator", "", 0);

    const QList<Finding> findings = check(items, servers);
    expectTrue(findings.count() == 3, QString("three findings: %1").arg(findings.count()));

    bool backup = false, gone = false, locked = false;
    for (const Finding& finding : findings)
    {
        if (finding.kind == Kind::Server && finding.text.contains("'Backup' is not connected - 2 items use it") && finding.item == 2)
            backup = true;
        if (finding.kind == Kind::Server && finding.text.contains("'Gone' is not set up") && finding.text.contains("1 item uses it"))
            gone = true;
        if (finding.kind == Kind::Locked && finding.item == 1 && finding.text.contains("channel 2 on Main"))
            locked = true;
    }
    expectTrue(backup, "a server not connected is said once, with how many items use it, pointing at the first");
    expectTrue(gone, "a server this client does not have is said");
    expectTrue(locked, "an item aimed at a locked channel will not play, and says so");
    for (const Finding& finding : findings)
        expectTrue(finding.item != 5 && finding.item != 6, "a disabled item, and one with no server, are left out");

    expectTrue(summary(QList<Finding>()).startsWith("Ready"), "nothing found reads as ready");
    QList<Finding> mixed = findings;
    mixed << Finding{Kind::Media, 0, "x"} << Finding{Kind::Media, 3, "y"} << Finding{Kind::Sheet, 0, "z"};
    expectTrue(summary(mixed) == "6 things to look at: 2 missing media or templates, 2 servers, 1 item on a locked channel, 1 sheet source not answering.",
               "the summary counts each kind: " + summary(mixed));

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
