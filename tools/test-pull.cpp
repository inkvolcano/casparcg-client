// The pull route, run end to end against a real relay.
//
// This is the thing the whole exercise was for: a client on one network fetching
// templates from a host on another, without anything reaching in to it. Every part
// of that had been tested except the part that actually does it. RelayClient reads
// a manifest, decides what differs, fetches it, checks the bytes and installs them,
// and none of that had ever run.
//
// So it runs, against a real PHP relay started for the test on a spare port, with
// the installer pointed at a temporary folder. Nothing here touches a real venue,
// a real repository or the application's own settings.
//
// PHP is needed. Without it the test says so and stops rather than pretending.

#include "../src/Widgets/RelayClient.h"
#include "../src/Widgets/TemplateInstaller.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTextStream>
#include <QtCore/QThread>

#include <QtNetwork/QTcpServer>

static int failures = 0;
static int checks = 0;

static const char* UPLOAD = "an-upload-token-long-enough";
static const char* DOWNLOAD = "a-download-token-long-enough";

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

/** Where PHP is, or empty. */
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

static int freePort()
{
    QTcpServer finder;
    finder.listen(QHostAddress::LocalHost, 0);
    return finder.serverPort();
}

/** Run one poll and wait for it to finish, giving the event loop its turns. */
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

/** Let any check-in that follows a poll actually reach the relay. */
static void settle(int ms = 700)
{
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < ms)
    {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
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
        out << "PHP was not found, so the relay cannot be started. Nothing was tested.\n";
        return 2;
    }

    QTemporaryDir sandbox;
    if (!sandbox.isValid())
    {
        out << "could not make a temporary folder\n";
        return 2;
    }

    // The relay's own copy, with tokens it will actually accept.
    QString relayDir = QDir(sandbox.path()).filePath("relay");
    QDir().mkpath(relayDir);

    QByteArray relaySource = readFile(QDir(QCoreApplication::applicationDirPath())
                                         .filePath("relay.php"));
    if (relaySource.isEmpty())
        relaySource = readFile("tools/php/relay/relay.php");
    if (relaySource.isEmpty())
    {
        out << "could not read tools/php/relay/relay.php\n";
        return 2;
    }

    relaySource.replace("'change-me-upload'", QByteArray("'") + UPLOAD + "'");
    relaySource.replace("'change-me-download'", QByteArray("'") + DOWNLOAD + "'");
    writeFile(QDir(relayDir).filePath("relay.php"), relaySource);

    // Two packs on the relay, so the filter has something to leave behind.
    QString store = QDir(relayDir).filePath("relay_data");
    writeFile(QDir(store).filePath("SEVILLE/calendar.html"), "<h1>one</h1>");
    writeFile(QDir(store).filePath("SEVILLE/css/site.css"), "body{}");
    writeFile(QDir(store).filePath("MARSEILLE/incidents.html"), "<h1>not ours</h1>");

    int port = freePort();

    QProcess relay;
    relay.setWorkingDirectory(relayDir);
    relay.start(php, QStringList() << "-S" << QString("127.0.0.1:%1").arg(port) << "relay.php");
    if (!relay.waitForStarted(5000))
    {
        out << "the relay would not start\n";
        return 2;
    }

    settle(900);   // give it a moment to be listening

    QString templates = QDir(sandbox.path()).filePath("templates");
    QDir().mkpath(templates);

    qputenv("CASPARCG_TEST_TemplatePushPath", templates.toUtf8());
    qputenv("CASPARCG_TEST_RelayEnabled", "true");
    qputenv("CASPARCG_TEST_RelayUrl", QString("http://127.0.0.1:%1/relay.php").arg(port).toUtf8());
    qputenv("CASPARCG_TEST_RelayToken", DOWNLOAD);
    qputenv("CASPARCG_TEST_RelayPacks", "SEVILLE");     // this venue follows one pack

    out << "A first pull\n";

    QString first = poll();
    expectTrue(first.contains("installed"), "the first poll installs something: " + first);
    expectTrue(readFile(QDir(templates).filePath("SEVILLE/calendar.html")) == QByteArray("<h1>one</h1>"),
               "a template arrived with the right bytes");
    expectTrue(readFile(QDir(templates).filePath("SEVILLE/css/site.css")) == QByteArray("body{}"),
               "so did a nested one");

    // The filter is what lets one relay carry every venue.
    expectTrue(!QFile::exists(QDir(templates).filePath("MARSEILLE/incidents.html")),
               "a pack this client does not follow was left alone");

    out << "\nA second pull with nothing to do\n";

    QString second = poll();
    expectTrue(second.contains("up to date"), "the second poll fetches nothing: " + second);

    out << "\nAfter a change at the relay\n";

    writeFile(QDir(store).filePath("SEVILLE/calendar.html"), "<h1>two</h1>");
    settle(1100);   // the relay stamps versions by the second

    QString third = poll();
    expectTrue(third.contains("installed"), "a changed file is fetched again: " + third);
    expectTrue(readFile(QDir(templates).filePath("SEVILLE/calendar.html")) == QByteArray("<h1>two</h1>"),
               "and the new bytes replaced the old");

    out << "\nWhat the relay was told\n";

    settle();

    // The check-in is what lets a dev machine see which venues are current.
    QProcess ask;
    ask.start(php, QStringList() << "-r"
        << QString("$c = stream_context_create(['http' => ['header' => 'X-Relay-Token: %1']]); "
                   "echo file_get_contents('http://127.0.0.1:%2/relay.php?action=clients', false, $c);")
           .arg(UPLOAD).arg(port));
    ask.waitForFinished(8000);
    QByteArray estate = ask.readAllStandardOutput();

    // Parsed rather than searched: the answer also lists every pack the relay
    // carries, so looking for a name anywhere in it proves nothing about what this
    // client claimed.
    QJsonArray clients = QJsonDocument::fromJson(estate).object().value("clients").toArray();
    expectTrue(clients.count() == 1, "exactly one client has checked in");

    QJsonObject mine = clients.isEmpty() ? QJsonObject() : clients.at(0).toObject();
    QJsonObject held = mine.value("packs").toObject();

    expectTrue(!mine.value("host").toString().isEmpty(), "it gave its name");
    expectTrue(held.contains("SEVILLE"), "and named the pack it holds");
    expectTrue(!held.contains("MARSEILLE"), "and did not claim a pack it does not follow");
    expectTrue(mine.value("current").toBool(), "and the relay reads it as current");
    expectTrue(mine.value("failed").toInt() == 0, "with nothing reported as failed");

    out << "\nListing the packs\n";

    // The relay's manifest carries a version and the files per pack; the listing
    // reads both, so Settings can show more than a name.
    {
        QList<RelayClient::PackListing> listed;
        QString listError = "never answered";
        bool answered = false;

        QMetaObject::Connection link = QObject::connect(
            &RelayClient::getInstance(), &RelayClient::packsListed,
            [&](const QList<RelayClient::PackListing>& packs, const QStringList&, bool, const QString& error) {
                listed = packs;
                listError = error;
                answered = true;
            });

        RelayClient::getInstance().listPacks();

        QElapsedTimer clock;
        clock.start();
        while (!answered && clock.elapsed() < 20000)
        {
            QCoreApplication::processEvents();
            QThread::msleep(10);
        }
        QObject::disconnect(link);

        expectTrue(listError.isEmpty(), "the relay listed its packs: " + listError);

        QStringList names;
        int sevilleFiles = 0;
        QString sevilleVersion;
        foreach (const RelayClient::PackListing& pack, listed)
        {
            names.append(pack.name);
            if (pack.name == "SEVILLE")
            {
                sevilleFiles = pack.files;
                sevilleVersion = pack.version;
            }
        }
        names.sort();

        expectTrue(names == (QStringList() << "MARSEILLE" << "SEVILLE"),
                   "both packs are listed, including the one this venue does not follow: " + names.join(", "));
        expectTrue(sevilleFiles == 2, QString("SEVILLE's files are counted (got %1)").arg(sevilleFiles));
        expectTrue(!sevilleVersion.isEmpty(), "and its version is carried");
        expectTrue(!RelayClient::getInstance().isBusy(), "a listing leaves the client free for a poll");
    }

    out << "\nWith the wrong token\n";

    qputenv("CASPARCG_TEST_RelayToken", "not-the-token");
    QString refused = poll();
    expectTrue(refused.contains("token"), "a wrong token is reported as one: " + refused);
    qputenv("CASPARCG_TEST_RelayToken", DOWNLOAD);

    out << "\nWhen the relay is not there\n";

    relay.kill();
    relay.waitForFinished(3000);

    QString gone = poll(25000);
    expectTrue(!gone.isEmpty() && gone != "timed out",
               "an unreachable relay ends the poll rather than hanging: " + gone);
    expectTrue(!gone.contains("up to date"), "and does not claim to be up to date");

    // Whatever happened, nothing that was already installed was removed.
    expectTrue(readFile(QDir(templates).filePath("SEVILLE/calendar.html")) == QByteArray("<h1>two</h1>"),
               "a failed poll leaves what was already installed alone");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    out.flush();

    return failures == 0 ? 0 : 1;
}
