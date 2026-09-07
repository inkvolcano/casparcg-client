// How the push tool reads an address, and every URL it builds from one.
//
// One text field decides whether a push goes to a client on the next rack, a relay
// on the internet, or a Git repository, and the three are told apart by how the
// address is written. Misread it and a push goes somewhere it was not meant to, with
// a token in a header the far end was not expecting.
//
// It is pure text handling with no network in it, which is exactly the kind of code
// that gets changed casually and breaks quietly. This runs it.

#include "../src/Push/PushWindow.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#include <QtNetwork/QNetworkRequest>

static int failures = 0;
static int checks = 0;

static void same(const QString& actual, const QString& wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n"
                        << "        wanted " << wanted << "\n"
                        << "        got    " << actual << "\n";
}

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

static PushTarget read(const QString& address)
{
    PushTarget target;
    target.token = "the-token";
    target.readAddress(address);
    return target;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    out << "A client\n";
    {
        PushTarget t = read("10.0.0.5:3000");
        expectTrue(!t.relay && !t.github, "host:port is a client");
        same(t.host, "10.0.0.5", "the host");
        same(QString::number(t.port), "3000", "the port");
        same(t.base(), "http://10.0.0.5:3000", "the base address");
        same(t.infoUrl(), "http://10.0.0.5:3000/templates/info", "identify");
        same(t.manifestUrl("SEVILLE"), "http://10.0.0.5:3000/templates/SEVILLE", "compare");
        same(t.uploadUrl("SEVILLE", "css/site.css"),
             "http://10.0.0.5:3000/templates/SEVILLE/css/site.css", "upload");
        expectTrue(!t.canRemove(), "a client cannot be removed from");

        PushTarget bare = read("10.0.0.5");
        same(QString::number(bare.port), "3000", "a missing port means 3000");
    }

    out << "\nA relay\n";
    {
        PushTarget t = read("https://example.com/relay/relay.php");
        expectTrue(t.relay && !t.github, "an address with a scheme is a relay");

        // The client branch splits on the last colon. A URL has one in "https:",
        // so reading this as a client would give a host of "https" and no port.
        same(t.host, "https://example.com/relay/relay.php", "the whole URL is kept");
        same(t.infoUrl(), "https://example.com/relay/relay.php?action=ping", "ping");
        same(t.manifestUrl("SEVILLE"),
             "https://example.com/relay/relay.php?action=manifest&pack=SEVILLE", "manifest");
        same(t.uploadUrl("SEVILLE", "css/site.css"),
             "https://example.com/relay/relay.php?action=upload&pack=SEVILLE&path=css/site.css",
             "upload keeps the slashes in a path");
        same(t.removeUrl("SEVILLE", "old.html"),
             "https://example.com/relay/relay.php?action=remove&pack=SEVILLE&path=old.html", "remove");
        expectTrue(t.canRemove(), "a relay can be removed from");

        // A port in the URL must not be read as a client's port.
        PushTarget ported = read("http://192.168.1.9:8080/relay.php");
        expectTrue(ported.relay, "a URL with a port is still a relay");
        same(ported.host, "http://192.168.1.9:8080/relay.php", "and keeps its port in the URL");

        // An address that already carries a query gets its action added, not a
        // second question mark that would break it.
        PushTarget query = read("https://example.com/r.php?site=north");
        same(query.infoUrl(), "https://example.com/r.php?site=north&action=ping",
             "an existing query is extended rather than replaced");
    }

    out << "\nA repository\n";
    {
        PushTarget t = read("github:acme/templates");
        expectTrue(t.github && !t.relay, "a github: address is a repository");
        same(t.ownerRepo, "acme/templates", "the owner and repository");
        same(t.branch, QString(), "no branch means the default one");
        same(t.base(), "https://api.github.com/repos/acme/templates", "the base address");
        same(t.manifestUrl("SEVILLE"),
             "https://api.github.com/repos/acme/templates/git/trees/HEAD?recursive=1",
             "the tree, which is the whole repository rather than one pack");
        same(t.uploadUrl("SEVILLE", "css/site.css"),
             "https://api.github.com/repos/acme/templates/contents/SEVILLE/css/site.css", "contents");
        expectTrue(t.canRemove(), "a repository can be removed from");

        PushTarget branch = read("github:acme/templates@rehearsal");
        same(branch.ownerRepo, "acme/templates", "a branch does not disturb the repository");
        same(branch.branch, "rehearsal", "the branch is read out");
        same(branch.manifestUrl("X"),
             "https://api.github.com/repos/acme/templates/git/trees/rehearsal?recursive=1",
             "and the tree is asked for on it");

        PushTarget slash = read("github:acme/templates/");
        same(slash.ownerRepo, "acme/templates", "a trailing slash is dropped");
    }

    out << "\nA repository on an Enterprise Server\n";
    {
        // The newest of this, and the easiest to get wrong: the address holds a URL
        // and the repository, and only the last two segments are the repository.
        PushTarget t = read("github:https://github.acme.com/api/v3/acme/templates");
        expectTrue(t.github, "still a repository");
        same(t.api, "https://github.acme.com/api/v3", "the server address");
        same(t.ownerRepo, "acme/templates", "and the repository after it");
        same(t.base(), "https://github.acme.com/api/v3/repos/acme/templates", "the base address");
        same(t.uploadUrl("SEVILLE", "a.html"),
             "https://github.acme.com/api/v3/repos/acme/templates/contents/SEVILLE/a.html", "contents");

        PushTarget branch = read("github:https://github.acme.com/api/v3/acme/templates@dev");
        same(branch.api, "https://github.acme.com/api/v3", "the server address, with a branch after it");
        same(branch.ownerRepo, "acme/templates", "the repository");
        same(branch.branch, "dev", "and the branch");

        // github.com written out in full is not an Enterprise server, but it must
        // still be read the same way rather than becoming part of the name.
        PushTarget plain = read("github:acme/templates");
        same(plain.api, QString(), "an ordinary address names no server");
    }

    out << "\nEncoding\n";
    {
        PushTarget relay = read("https://example.com/relay.php");
        same(relay.uploadUrl("SEVILLE", "images/team away.png"),
             "https://example.com/relay.php?action=upload&pack=SEVILLE&path=images/team%20away.png",
             "a space is encoded and the separators are not");

        PushTarget client = read("10.0.0.5:3000");
        same(client.uploadUrl("SEVILLE", "images/team away.png"),
             "http://10.0.0.5:3000/templates/SEVILLE/images/team%20away.png",
             "the same on a client");

        PushTarget hub = read("github:acme/templates");
        same(hub.uploadUrl("SEVILLE", "a b/c d.html"),
             "https://api.github.com/repos/acme/templates/contents/SEVILLE/a%20b/c%20d.html",
             "and on a repository");
    }

    out << "\nWhich token goes where\n";
    {
        // Three kinds, three ways of authenticating, and deliberately three
        // different tokens. Sending one where another belongs would hand a client's
        // push token to a repository, or a relay's upload token to a client.
        PushTarget client = read("10.0.0.5:3000");
        QNetworkRequest r1;
        client.authorise(r1);
        same(QString::fromUtf8(r1.rawHeader("X-Template-Token")), "the-token", "a client gets its own header");
        expectTrue(r1.rawHeader("Authorization").isEmpty(), "and no bearer token");

        PushTarget relay = read("https://example.com/relay.php");
        QNetworkRequest r2;
        relay.authorise(r2);
        same(QString::fromUtf8(r2.rawHeader("X-Relay-Token")), "the-token", "a relay gets its own header");
        expectTrue(r2.rawHeader("X-Template-Token").isEmpty(), "and not a client's");

        PushTarget hub = read("github:acme/templates");
        QNetworkRequest r3;
        hub.authorise(r3);
        same(QString::fromUtf8(r3.rawHeader("Authorization")), "Bearer the-token", "a repository gets a bearer token");
        expectTrue(!r3.rawHeader("User-Agent").isEmpty(), "and a user agent, which GitHub insists on");
        expectTrue(r3.rawHeader("X-Relay-Token").isEmpty(), "and not a relay's header");
    }

    out << "\nWhat a row is called\n";
    {
        PushTarget named = read("10.0.0.5:3000");
        named.name = "Studio A";
        same(named.label(), "Studio A", "a name is used when there is one");

        same(read("10.0.0.5:3000").label(), "http://10.0.0.5:3000", "otherwise a client shows its address");
        same(read("github:acme/templates").label(), "github.com/acme/templates", "a repository shows its name");
        expectTrue(read("https://example.com/relay.php").label().startsWith("relay "),
                   "and a relay says it is one");
    }

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    out.flush();

    return failures == 0 ? 0 : 1;
}
