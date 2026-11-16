// The presenter view on a channel: the address the server is given, and the
// commands that put the page on and take it off.

#include "../src/Common/PresenterOutputRules.h"

#include <QtCore/QTextStream>

static int checks = 0;
static int failures = 0;

static void expect(const QString& actual, const QString& expected, const QString& what)
{
    checks++;
    if (actual == expected)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n        got      " << actual << "\n        expected " << expected << "\n";
}

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

using namespace PresenterOutputRules;

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Presenter view on a channel\n";

    expect(hostForUrl("192.168.1.20"), "192.168.1.20", "an IPv4 address as it is");
    expect(hostForUrl("::ffff:192.168.1.20"), "192.168.1.20", "IPv4 seen through an IPv6 socket loses its prefix");
    expect(hostForUrl("fe80::1%12"), "[fe80::1]", "IPv6: scope dropped, bracketed");
    expect(hostForUrl("2001:db8::5"), "[2001:db8::5]", "IPv6 bracketed");
    expect(hostForUrl("127.0.0.1"), "127.0.0.1", "a server on this machine reaches it on loopback");
    expect(hostForUrl(""), "", "not connected: no address");
    expect(hostForUrl("0.0.0.0"), "", "the any-address is not one the server can reach");
    expect(hostForUrl("::"), "", "nor is the IPv6 any-address");

    expect(pageUrl("192.168.1.20", 3000), "http://192.168.1.20:3000/runsheet?view=presenter&output=1",
           "the page: presenter view, output form, on the run sheet's port");
    expect(pageUrl("::ffff:10.0.0.4", 8080), "http://10.0.0.4:8080/runsheet?view=presenter&output=1", "another port");
    expect(pageUrl("fe80::1", 3000), "http://[fe80::1]:3000/runsheet?view=presenter&output=1", "IPv6 in a URL");
    expect(pageUrl("", 3000), "", "no address: no page");
    expect(pageUrl("192.168.1.20", 0), "", "no port: no page");
    expect(pageUrl("192.168.1.20", 70000), "", "a port that cannot be: no page");

    expect(playCommand(3, 999, "http://192.168.1.20:3000/runsheet?view=presenter&output=1"),
           "PLAY 3-999 [HTML] \"http://192.168.1.20:3000/runsheet?view=presenter&output=1\"", "on: the HTML producer, the URL quoted");
    expect(clearCommand(3, 999), "CLEAR 3-999", "off: that layer cleared, nothing else");

    expectTrue(validPlace(2, 999, 4), "channel 2 of 4, layer 999");
    expectTrue(!validPlace(5, 10, 4), "a channel the server does not have");
    expectTrue(!validPlace(0, 10, 4), "channel 0 is not a channel");
    expectTrue(!validPlace(1, 10000, 4), "a layer past 9999");
    expectTrue(validPlace(6, 10, 0), "channel count not known yet: any channel from 1");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
