// The GitHub pull route, run end to end.
//
// The relay route has a test that runs it; this one had only ever been checked
// against the live API for the shape of its answers. The logic that reads a tree,
// works out which pack a path belongs to, compares Git blob hashes and fetches by
// digest had never executed.
//
// It runs here against tools/mock-github.php, which answers the three calls a
// client makes in the shapes github.com actually uses, including the directory
// entries a real tree carries and the token and user-agent rules it enforces.
//
// Pointing the client at it needs the GitHub Enterprise setting, which exists for
// its own reasons and happens to make this testable.

#include "../src/Widgets/RelayClient.h"
#include "../src/Widgets/RepoPublisher.h"
#include "../src/Widgets/TemplateInstaller.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtCore/QSysInfo>
#include <QtCore/QTextStream>
#include <QtCore/QThread>

#include <QtNetwork/QTcpServer>

static int failures = 0;
static int checks = 0;

static const char* TOKEN = "a-github-token-for-testing";

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

static QString findPhp()
{
    QStringList candidates;
    candidates << "C:/CasparCG/php/php.exe" << "php.exe" << "php";

    foreach (const QString& candidate, candidates)
    {
        if (QFile::exists(candidate))
            return candidate;

        QProcess probe;
        probe.start(candidate, QStringList() << "-v");
        if (probe.waitForFinished(3000) && probe.exitCode() == 0)
            return candidate;
    }

    return QString();
}

static void settle(int ms)
{
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < ms)
    {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
}

static QString poll(int msWait = 20000)
{
    QString summary;
    bool done = false;

    QMetaObject::Connection link = QObject::connect(
        &RelayClient::getInstance(), &RelayClient::finished,
        [&](int, int, const QString& text) { summary = text; done = true; });

    if (!RelayClient::getInstance().checkNow())
    {
        QObject::disconnect(link);
        return QString("refused to start");
    }

    QElapsedTimer clock;
    clock.start();
    while (!done && clock.elapsed() < msWait)
    {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }

    QObject::disconnect(link);
    return done ? summary : QString("timed out");
}

static bool writeFile(const QString& path, const QByteArray& body)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(body);
    file.close();
    return true;
}

static QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();

    QByteArray body = file.readAll();
    file.close();
    return body;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    QString php = findPhp();
    if (php.isEmpty())
    {
        out << "PHP was not found, so the mock API cannot be started. Nothing was tested.\n";
        return 2;
    }

    QTemporaryDir sandbox;
    if (!sandbox.isValid())
    {
        out << "could not make a temporary folder\n";
        return 2;
    }

    QString apiDir = QDir(sandbox.path()).filePath("api");
    QDir().mkpath(apiDir);

    QByteArray mock = readFile("tools/mock-github.php");
    if (mock.isEmpty())
    {
        out << "could not read tools/mock-github.php\n";
        return 2;
    }
    writeFile(QDir(apiDir).filePath("mock-github.php"), mock);

    // What the repository holds. Packs are folders at its root; the rest is there
    // to be ignored, which is the part worth checking.
    QString repo = QDir(apiDir).filePath("mock_repo");
    writeFile(QDir(repo).filePath("SEVILLE/calendar.html"), "<h1>one</h1>");
    writeFile(QDir(repo).filePath("SEVILLE/css/site.css"), "body{}");
    writeFile(QDir(repo).filePath("MARSEILLE/incidents.html"), "<h1>not ours</h1>");
    writeFile(QDir(repo).filePath("README.md"), "a file at the root belongs to no pack");
    writeFile(QDir(repo).filePath(".github/workflows/ci.yml"), "on: push");

    int port = 0;
    int reportPort = 0;
    {
        QTcpServer finder;
        finder.listen(QHostAddress::LocalHost, 0);
        port = finder.serverPort();

        QTcpServer second;
        second.listen(QHostAddress::LocalHost, 0);
        reportPort = second.serverPort();
    }

    QProcess api;
    api.setWorkingDirectory(apiDir);
    api.start(php, QStringList() << "-S" << QString("127.0.0.1:%1").arg(port) << "mock-github.php");
    if (!api.waitForStarted(5000))
    {
        out << "the mock API would not start\n";
        return 2;
    }

    // A second one, purely to receive the check-in.
    //
    // php -S handles a single connection at a time, and the client keeps its
    // connection to the API alive between requests, so a report posted to that same
    // port waits behind a connection that will not close and never arrives. The
    // client is doing nothing wrong; the toy server cannot do two things at once.
    QProcess reporter;
    reporter.setWorkingDirectory(apiDir);
    reporter.start(php, QStringList() << "-S" << QString("127.0.0.1:%1").arg(reportPort)
                                      << "mock-github.php");
    if (!reporter.waitForStarted(5000))
    {
        out << "the mock check-in collector would not start\n";
        return 2;
    }

    settle(900);

    QString templates = QDir(sandbox.path()).filePath("templates");
    QDir().mkpath(templates);

    qputenv("CASPARCG_TEST_TemplatePushPath", templates.toUtf8());
    qputenv("CASPARCG_TEST_RelayEnabled", "true");
    qputenv("CASPARCG_TEST_RelayUrl", "github:mock/templates");
    qputenv("CASPARCG_TEST_RelayToken", TOKEN);
    qputenv("CASPARCG_TEST_RelayPacks", "SEVILLE");
    qputenv("CASPARCG_TEST_RelayGitHubApi", QString("http://127.0.0.1:%1").arg(port).toUtf8());

    // Where this venue reports. A repository has nowhere to report to, so a client
    // on this route is given an address of its own - and for several builds it then
    // sent nothing there at all, because the pack versions the report is built from
    // were only ever filled in on the relay route. The pull worked, the estate view
    // stayed empty, and no test looked.
    qputenv("CASPARCG_TEST_RelayCheckInUrl",
            QString("http://127.0.0.1:%1/report.php").arg(reportPort).toUtf8());
    qputenv("CASPARCG_TEST_RelayCheckInToken", "a-relay-download-token");

    // The application sets this at startup from the generated version header, which
    // a harness has no reason to have. Without it the report carries no build, which
    // is deliberate - reporting nothing beats reporting a wrong answer - so the test
    // does what the application does.
    RelayClient::setClientVersion("2.3.1", "999");

    out << "Reading the address\n";

    expectTrue(RelayClient::isGitHub(), "a github: address is recognised as one");
    expectTrue(RelayClient::gitHubOwnerRepo() == "mock/templates", "the owner and repository are read out");
    expectTrue(RelayClient::gitHubApi() == QString("http://127.0.0.1:%1").arg(port),
               "an Enterprise API address is honoured");

    out << "\nA first pull\n";

    QString first = poll();
    expectTrue(first.contains("installed"), "the first pull installs something: " + first);
    expectTrue(readFile(QDir(templates).filePath("SEVILLE/calendar.html")) == QByteArray("<h1>one</h1>"),
               "a template arrived with the right bytes");

    out << "\nAnd reporting it\n";

    // The mock writes whatever it is posted to this file. Its absence is the bug
    // this exists for: a venue that pulled perfectly and told nobody.
    settle(4000);

    // The collector writes beside mock-github.php, which is apiDir.
    const QByteArray reported = readFile(QDir(apiDir).filePath("mock_checkin.json"));

    expectTrue(!reported.isEmpty(), "a client on the GitHub route reports at all");

    QJsonObject report = QJsonDocument::fromJson(reported).object();

    expectTrue(report.value("source").toString() == "github",
               "and says the templates came from GitHub");
    expectTrue(report.contains("packs") && !report.value("packs").toObject().isEmpty(),
               "and names the packs it holds, which is the field that was empty");
    expectTrue(!report.value("packs").toObject().value("SEVILLE").toString().isEmpty(),
               "with a version for the pack it followed");
    expectTrue(!report.value("build").toString().isEmpty(),
               "and which build it is running");
    expectTrue(readFile(QDir(templates).filePath("SEVILLE/css/site.css")) == QByteArray("body{}"),
               "so did a nested one");

    expectTrue(!QFile::exists(QDir(templates).filePath("MARSEILLE/incidents.html")),
               "a pack this client does not follow was left alone");
    expectTrue(!QFile::exists(QDir(templates).filePath("README.md")),
               "a file at the repository root is not a pack");
    expectTrue(!QDir(templates).exists(".github"),
               "the repository's own .github folder is not installed as one");

    out << "\nFollowing every pack\n";

    // With a filter naming SEVILLE, the filter is what keeps .github and the root
    // files out, whatever the rules about them say. Clearing it is the only way to
    // find out whether those rules do anything at all.
    qputenv("CASPARCG_TEST_RelayPacks", QByteArray());

    QString everything = poll();
    expectTrue(everything.contains("installed"), "the other pack arrives now: " + everything);
    expectTrue(QFile::exists(QDir(templates).filePath("MARSEILLE/incidents.html")),
               "a pack that was filtered out before is installed once it is followed");
    expectTrue(!QDir(templates).exists(".github"),
               "the repository's own .github is still not a pack");
    expectTrue(!QFile::exists(QDir(templates).filePath("workflows/ci.yml")),
               "and nothing from inside it landed under another name");
    expectTrue(!QFile::exists(QDir(templates).filePath("README.md")),
               "a file at the repository root is still not installed");

    qputenv("CASPARCG_TEST_RelayPacks", "SEVILLE");

    out << "\nA second pull with nothing to do\n";

    // The whole efficiency of this route: local files hashed the Git way must match
    // what the tree said, or every client re-downloads everything on every poll.
    QString second = poll();
    expectTrue(second.contains("up to date"), "the second pull fetches nothing: " + second);

    out << "\nAfter a change in the repository\n";

    writeFile(QDir(repo).filePath("SEVILLE/calendar.html"), "<h1>two</h1>");

    QString third = poll();
    expectTrue(third.contains("installed"), "a changed file is fetched again: " + third);
    expectTrue(readFile(QDir(templates).filePath("SEVILLE/calendar.html")) == QByteArray("<h1>two</h1>"),
               "and the new bytes replaced the old");

    out << "\nListing the packs\n";

    // Settings asks the source what it has so the operator can tick packs rather
    // than type folder names. Same fetch as a poll, but it installs nothing, sends
    // no check-in and leaves the last poll's summary alone.
    {
        QStringList listed;
        int listedFiles = 0;
        bool assigns = true;
        QString listError = "never answered";
        bool answered = false;

        QMetaObject::Connection link = QObject::connect(
            &RelayClient::getInstance(), &RelayClient::packsListed,
            [&](const QList<RelayClient::PackListing>& packs, const QStringList&, bool sourceAssigns, const QString& error) {
                listed.clear();
                listedFiles = 0;
                foreach (const RelayClient::PackListing& pack, packs)
                {
                    listed.append(pack.name);
                    listedFiles += pack.files;
                }
                assigns = sourceAssigns;
                listError = error;
                answered = true;
            });

        const QString summaryBefore = RelayClient::getInstance().lastSummary();

        RelayClient::getInstance().listPacks();

        QElapsedTimer clock;
        clock.start();
        while (!answered && clock.elapsed() < 20000)
        {
            QCoreApplication::processEvents();
            QThread::msleep(10);
        }
        QObject::disconnect(link);

        expectTrue(listError.isEmpty(), "the source listed its packs: " + listError);

        QStringList sorted = listed;
        sorted.sort();
        expectTrue(sorted == (QStringList() << "MARSEILLE" << "SEVILLE"),
                   "both packs are listed and nothing else: " + listed.join(", "));
        expectTrue(!listed.contains(".github") && !listed.contains("README.md"),
                   "the repository's own machinery and root files are not packs");
        expectTrue(listedFiles >= 3, QString("files are counted per pack (got %1)").arg(listedFiles));
        expectTrue(!assigns, "a repository with no assignments.json names nobody");
        expectTrue(!RelayClient::getInstance().isBusy(), "a listing leaves the client free for a poll");
        expectTrue(RelayClient::getInstance().lastSummary() == summaryBefore,
                   "and does not overwrite the last poll's summary");

        // The rule both the listing and the pull use, on its own.
        expectTrue(RelayClient::packOfTreePath("SEVILLE/css/site.css") == "SEVILLE", "a nested file belongs to its first folder");
        expectTrue(RelayClient::packOfTreePath("README.md").isEmpty(), "a root file belongs to no pack");
        expectTrue(RelayClient::packOfTreePath(".github/workflows/ci.yml").isEmpty(), ".github is not a pack");
        expectTrue(RelayClient::packOfTreePath("../x/y.html").isEmpty(), "nor is a folder that fails the path rule");
        expectTrue(RelayClient::packOfTreePath("/x.html").isEmpty(), "nor a leading slash");
    }

    out << "\nA master pushes its edits\n";

    // The way back: a client allowed to may send its own edits to the source,
    // from a button and never on its own, with a token the pull token is not.
    {
        const QByteArray WRITE_TOKEN = "a-github-write-token-for-testing";

        auto prepareAndWait = [&](QStringList& files, QString& problem) {
            bool answered = false;
            QMetaObject::Connection link = QObject::connect(
                &RepoPublisher::getInstance(), &RepoPublisher::planned,
                [&](const QStringList& f, const QString& p) { files = f; problem = p; answered = true; });
            RepoPublisher::getInstance().prepare();
            QElapsedTimer clock;
            clock.start();
            while (!answered && clock.elapsed() < 20000) { QCoreApplication::processEvents(); QThread::msleep(10); }
            QObject::disconnect(link);
            if (!answered) problem = "never answered";
        };

        auto pushAndWait = [&](const QString& message, bool& ok, QString& summary) {
            bool answered = false;
            QMetaObject::Connection link = QObject::connect(
                &RepoPublisher::getInstance(), &RepoPublisher::pushed,
                [&](bool o, const QString& s) { ok = o; summary = s; answered = true; });
            RepoPublisher::getInstance().push(message);
            QElapsedTimer clock;
            clock.start();
            while (!answered && clock.elapsed() < 20000) { QCoreApplication::processEvents(); QThread::msleep(10); }
            QObject::disconnect(link);
            if (!answered) { ok = false; summary = "never answered"; }
        };

        QStringList files;
        QString problem;

        // The three refusals that come before any network call.
        qputenv("CASPARCG_TEST_RelayMaster", "false");
        qputenv("CASPARCG_TEST_RelayPushToken", WRITE_TOKEN);
        prepareAndWait(files, problem);
        expectTrue(problem.contains("not a master"), "a client that is not a master cannot push: " + problem);

        qputenv("CASPARCG_TEST_RelayMaster", "true");
        qputenv("CASPARCG_TEST_RelayPushToken", QByteArray());
        prepareAndWait(files, problem);
        expectTrue(problem.contains("write token"), "no write token, no push: " + problem);

        qputenv("CASPARCG_TEST_RelayPushToken", WRITE_TOKEN);

        // Edits at the venue: one changed, one new in a subfolder, one protected
        // file that must not travel, and one in a pack this client does not follow.
        writeFile(QDir(templates).filePath("SEVILLE/calendar.html"), "<h1>edited at the venue</h1>");
        writeFile(QDir(templates).filePath("SEVILLE/venue/new.html"), "<p>made here</p>");
        writeFile(QDir(templates).filePath("SEVILLE/project.js"), "var apiKey = 'this venue only';");
        writeFile(QDir(templates).filePath("MARSEILLE/incidents.html"), "<h1>changed, but not followed</h1>");

        prepareAndWait(files, problem);
        expectTrue(problem.isEmpty(), "the repository was read: " + problem);
        QStringList sortedFiles = files;
        sortedFiles.sort();
        expectTrue(sortedFiles == (QStringList() << "SEVILLE/calendar.html" << "SEVILLE/venue/new.html"),
                   "exactly the changed and the new file are listed: " + files.join(", "));
        expectTrue(!files.contains("SEVILLE/project.js"), "project.js never travels");
        expectTrue(!files.contains("MARSEILLE/incidents.html"), "a pack this client does not follow is not pushed");

        bool ok = false;
        QString summary;
        pushAndWait("Edited at the venue", ok, summary);
        expectTrue(ok, "the push was accepted: " + summary);
        expectTrue(summary.contains("2 file(s) pushed"), "and says how many went: " + summary);
        expectTrue(!RepoPublisher::getInstance().isBusy(), "and the publisher is free again");

        expectTrue(readFile(QDir(repo).filePath("SEVILLE/calendar.html")) == QByteArray("<h1>edited at the venue</h1>"),
                   "the edit is in the repository");
        expectTrue(readFile(QDir(repo).filePath("SEVILLE/venue/new.html")) == QByteArray("<p>made here</p>"),
                   "and so is the new file, folder and all");
        expectTrue(!QFile::exists(QDir(repo).filePath("SEVILLE/project.js")),
                   "project.js did not reach the repository");
        expectTrue(readFile(QDir(repo).filePath("MARSEILLE/incidents.html")) == QByteArray("<h1>not ours</h1>"),
                   "the pack this client does not follow is untouched");

        QJsonObject commit = QJsonDocument::fromJson(readFile(QDir(apiDir).filePath("mock_state/last_commit.json"))).object();
        expectTrue(commit.value("message").toString() == "Edited at the venue", "the commit carries the operator's message");
        expectTrue(commit.value("author").toObject().value("name").toString().contains(QSysInfo::machineHostName()),
                   "and is authored as this machine");

        QString afterPush = poll();
        expectTrue(afterPush.contains("already up to date"),
                   "a poll after the push finds nothing to do: " + afterPush);

        // Behind: the dev machine pushed something this venue has not pulled.
        writeFile(QDir(repo).filePath("SEVILLE/fresh.html"), "<h1>from the dev machine</h1>");
        prepareAndWait(files, problem);
        expectTrue(problem.contains("Pull first"), "a repository holding unpulled files is refused: " + problem);
        expectTrue(files.isEmpty(), "and nothing is listed to push");

        poll();
        prepareAndWait(files, problem);
        expectTrue(problem.isEmpty() && files.isEmpty(), "after pulling, nothing differs and nothing is refused: " + problem);

        // The pull token cannot write, and the repository must not change.
        qputenv("CASPARCG_TEST_RelayPushToken", TOKEN);
        writeFile(QDir(templates).filePath("SEVILLE/calendar.html"), "<h1>third</h1>");
        prepareAndWait(files, problem);
        expectTrue(problem.isEmpty() && files.count() == 1, "reading works with a read token: " + problem);
        pushAndWait("Should be refused", ok, summary);
        expectTrue(!ok, "writing with the pull token is refused: " + summary);
        expectTrue(readFile(QDir(repo).filePath("SEVILLE/calendar.html")) == QByteArray("<h1>edited at the venue</h1>"),
                   "and the repository is unchanged");

        // Cleanly back in step, so what follows starts from a matching pair.
        qputenv("CASPARCG_TEST_RelayPushToken", WRITE_TOKEN);
        prepareAndWait(files, problem);
        pushAndWait("Third", ok, summary);
        expectTrue(ok, "the same push with the write token goes: " + summary);
        qputenv("CASPARCG_TEST_RelayMaster", "false");

        // The rule on its own.
        QMap<QString, QString> local;
        local.insert("a.html", "1");
        local.insert("project.js", "x");
        local.insert("same.html", "s");
        local.insert("sub/b.html", "2");
        QMap<QString, QString> remote;
        remote.insert("a.html", "0");
        remote.insert("same.html", "s");
        remote.insert("gone.html", "g");
        QStringList missing;
        QList<RepoPublisher::Change> planned = RepoPublisher::plan("P", local, remote, &missing);
        QStringList plannedPaths;
        foreach (const RepoPublisher::Change& change, planned)
            plannedPaths.append(change.repoPath() + (change.isNew ? " (new)" : ""));
        plannedPaths.sort();
        expectTrue(plannedPaths == (QStringList() << "P/a.html" << "P/sub/b.html (new)"),
                   "changed and new are planned, unchanged and protected are not: " + plannedPaths.join(", "));
        expectTrue(missing == QStringList("P/gone.html"), "a file only the repository has is reported, not deleted");
    }

    out << "\nA pull larger than one batch\n";

    // The first pull of a real estate is 769 files across two packs, and anything
    // over the cap used to be refused outright - so a new venue following both
    // packs was told the source offered more than one poll would take, every
    // fifteen minutes, and installed nothing. Ever. The cap is a batch now.
    //
    // Set small here so the boundary can be crossed with three files rather than
    // five hundred and one.
    qputenv("CASPARCG_TEST_RelayFilesPerPoll", "2");

    writeFile(QDir(repo).filePath("SEVILLE/batch/one.html"), "1");
    writeFile(QDir(repo).filePath("SEVILLE/batch/two.html"), "2");
    writeFile(QDir(repo).filePath("SEVILLE/batch/three.html"), "3");

    QString batched = poll();
    expectTrue(!batched.contains("more than"), "a pull over the batch is not refused: " + batched);
    expectTrue(batched.contains("still to come"), "it says what is left for the next poll: " + batched);

    int installedNow = 0;
    if (QFile::exists(QDir(templates).filePath("SEVILLE/batch/one.html"))) installedNow++;
    if (QFile::exists(QDir(templates).filePath("SEVILLE/batch/two.html"))) installedNow++;
    if (QFile::exists(QDir(templates).filePath("SEVILLE/batch/three.html"))) installedNow++;

    expectTrue(installedNow == 2, QString("one batch is installed, not all of it (got %1)").arg(installedNow));

    // Half a pack is no more "current" than a pack that failed, so the estate must
    // not be told this venue holds SEVILLE yet.
    settle(2000);
    QJsonObject partial =
        QJsonDocument::fromJson(readFile(QDir(apiDir).filePath("mock_checkin.json"))).object();
    expectTrue(!partial.value("packs").toObject().contains("SEVILLE"),
               "a pack with files still queued is left out of the check-in");

    QString rest = poll();
    expectTrue(rest.contains("installed"), "the next poll takes the rest: " + rest);

    int installedAfter = 0;
    if (QFile::exists(QDir(templates).filePath("SEVILLE/batch/one.html"))) installedAfter++;
    if (QFile::exists(QDir(templates).filePath("SEVILLE/batch/two.html"))) installedAfter++;
    if (QFile::exists(QDir(templates).filePath("SEVILLE/batch/three.html"))) installedAfter++;

    expectTrue(installedAfter == 3, QString("two polls finish what one could not (got %1)").arg(installedAfter));

    settle(2000);
    QJsonObject whole =
        QJsonDocument::fromJson(readFile(QDir(apiDir).filePath("mock_checkin.json"))).object();
    expectTrue(!whole.value("packs").toObject().value("SEVILLE").toString().isEmpty(),
               "and the completed pack is reported again");

    qputenv("CASPARCG_TEST_RelayFilesPerPoll", QByteArray());

    out << "\nA tree too large to list\n";

    // A truncated listing looks exactly like a repository missing files. Acting on
    // it would mean reporting "up to date" while being wrong, so it must refuse.
    writeFile(QDir(repo).filePath("TRUNCATE"), "");
    QString truncated = poll();
    expectTrue(truncated.contains("too large"), "a truncated tree is refused, not guessed at: " + truncated);
    expectTrue(!truncated.contains("up to date"), "and is never reported as up to date");
    QFile::remove(QDir(repo).filePath("TRUNCATE"));

    out << "\nWith the wrong token\n";

    qputenv("CASPARCG_TEST_RelayToken", "not-the-token");
    QString refused = poll();
    expectTrue(refused.contains("token") || refused.contains("repository"),
               "a refused token is reported as one: " + refused);
    qputenv("CASPARCG_TEST_RelayToken", TOKEN);

    out << "\nWhen GitHub is unreachable\n";

    // Whatever the file holds now - earlier sections may have changed it - is
    // what it must still hold after a poll that could not reach the source.
    const QByteArray heldBefore = readFile(QDir(templates).filePath("SEVILLE/calendar.html"));

    api.kill();
    reporter.kill();
    api.waitForFinished(3000);

    QString gone = poll(25000);
    expectTrue(!gone.isEmpty() && gone != "timed out",
               "an unreachable API ends the poll rather than hanging: " + gone);
    expectTrue(!gone.contains("up to date"), "and does not claim to be up to date");
    expectTrue(readFile(QDir(templates).filePath("SEVILLE/calendar.html")) == heldBefore,
               "a failed poll leaves what was already installed alone");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    out.flush();

    return failures == 0 ? 0 : 1;
}
