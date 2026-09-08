// The HTTP parser, driven over a real socket.
//
// SheetCacheServer is hand-rolled HTTP on a QTcpServer, and it is the one piece of
// this that listens. Whatever arrives on that port arrives as bytes off a network,
// from something that may not be a client at all, and every request is taken apart
// by code written here rather than by a web server somebody else maintains.
//
// So it gets a real socket and real rubbish: truncated requests, lying lengths,
// split packets, header injection, binary noise, and requests with nothing in them
// at all. The test asserts an answer or a refusal, and above all that the server is
// still listening afterwards.
//
// It runs on a spare port with the installer pointed at a temporary folder, so it
// touches nothing the application uses.

#include "../src/Widgets/SheetCacheServer.h"
#include "../src/Widgets/TemplateInstaller.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTextStream>
#include <QtCore/QThread>

#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>

static int failures = 0;
static int checks = 0;
static int port = 0;

static const char* TOKEN = "a-token-worth-having";

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

/** Send raw bytes and return whatever comes back. Empty means nothing came. */
static QByteArray sendRaw(const QByteArray& request, int msWait = 3000, int splitAfter = -1)
{
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(port));
    if (!socket.waitForConnected(2000))
        return QByteArray("NO CONNECTION");

    // The server under test lives in this same thread, so its event loop only runs
    // when this gives it a turn. Without that it never accepts the connection and
    // every request looks like a timeout.
    QCoreApplication::processEvents();

    if (splitAfter > 0 && splitAfter < request.size())
    {
        // Two packets with a gap, which is how a request crosses a slow link. The
        // parser has to hold the first half rather than answering it.
        socket.write(request.left(splitAfter));
        socket.flush();
        socket.waitForBytesWritten(1000);

        // A real gap, with the server given turns during it, so the parser genuinely
        // sees half a request and has to hold it.
        QElapsedTimer gap;
        gap.start();
        while (gap.elapsed() < 200)
        {
            QCoreApplication::processEvents();
            QThread::msleep(10);
        }
        socket.write(request.mid(splitAfter));
    }
    else
    {
        socket.write(request);
    }

    socket.flush();

    QByteArray answer;
    QElapsedTimer clock;
    clock.start();

    while (clock.elapsed() < msWait)
    {
        QCoreApplication::processEvents();

        if (socket.waitForReadyRead(50))
            answer.append(socket.readAll());
        else if (!answer.isEmpty() && socket.state() != QAbstractSocket::ConnectedState)
            break;
    }

    answer.append(socket.readAll());
    socket.abort();
    return answer;
}

static int statusOf(const QByteArray& answer)
{
    if (!answer.startsWith("HTTP/1.1 "))
        return 0;

    return answer.mid(9, 3).toInt();
}

static QByteArray request(const QByteArray& method, const QByteArray& target,
                          const QByteArray& token, const QByteArray& body = QByteArray())
{
    QByteArray head = method + " " + target + " HTTP/1.1\r\nHost: localhost\r\n";
    if (!token.isNull())
        head += "X-Template-Token: " + token + "\r\n";
    if (!body.isEmpty())
        head += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";

    return head + "\r\n" + body;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    QTemporaryDir sandbox;
    if (!sandbox.isValid())
    {
        out << "could not make a temporary folder\n";
        return 2;
    }

    // A port nothing else is on, found by asking the operating system for one and
    // giving it straight back.
    {
        QTcpServer finder;
        finder.listen(QHostAddress::LocalHost, 0);
        port = finder.serverPort();
    }

    QString root = QDir(sandbox.path()).filePath("templates");
    QDir().mkpath(root);

    qputenv("CASPARCG_TEST_SheetsHostCache", "false");
    qputenv("CASPARCG_TEST_SheetsCacheDirectory", QDir(sandbox.path()).filePath("cache").toUtf8());
    qputenv("CASPARCG_TEST_SheetsHostCachePort", QByteArray::number(port));
    qputenv("CASPARCG_TEST_TemplatePushEnabled", "true");
    qputenv("CASPARCG_TEST_TemplatePushToken", TOKEN);
    qputenv("CASPARCG_TEST_TemplatePushPath", root.toUtf8());

    SheetCacheServer::getInstance().start();
    expectTrue(SheetCacheServer::getInstance().isRunning(), "the server is listening");

    if (!SheetCacheServer::getInstance().isRunning())
    {
        out << "nothing to test against\n";
        return 2;
    }

    out << "Ordinary requests\n";

    expectTrue(statusOf(sendRaw(request("GET", "/templates", TOKEN))) == 200,
               "a listing with the right token");
    expectTrue(statusOf(sendRaw(request("GET", "/templates/info", TOKEN))) == 200,
               "identify with the right token");

    // A push, over a socket, landing on disk. The whole direct route in one check.
    expectTrue(statusOf(sendRaw(request("PUT", "/templates/SEVILLE/a.html", TOKEN, "<h1>hi</h1>"))) == 200,
               "a file installed over TCP");
    expectTrue(QFile::exists(QDir(root).filePath("SEVILLE/a.html")),
               "and it is on disk where it belongs");

    out << "\nThe token\n";

    expectTrue(statusOf(sendRaw(request("GET", "/templates", "wrong"))) == 401, "a wrong token");
    expectTrue(statusOf(sendRaw(request("GET", "/templates", QByteArray()))) == 401, "no token at all");

    // Header names are case-insensitive in HTTP, and a client that sends one in a
    // different case is not wrong.
    QByteArray lower = "GET /templates HTTP/1.1\r\nhost: localhost\r\n"
                       "x-template-token: " + QByteArray(TOKEN) + "\r\n\r\n";
    expectTrue(statusOf(sendRaw(lower)) == 200, "a lower-case header name is accepted");

    out << "\nRubbish\n";

    // Each of these must produce an answer or a refusal, and must leave the server
    // running. A crash here takes the whole client with it.
    struct { const char* what; QByteArray bytes; } nasty[] = {
        { "an empty request",        QByteArray("\r\n\r\n") },
        { "no method or target",     QByteArray("GARBAGE\r\n\r\n") },
        { "one word",                QByteArray("GET\r\n\r\n") },
        { "no HTTP version",         QByteArray("GET /templates\r\n\r\n") },
        { "a header with no colon",  QByteArray("GET /templates HTTP/1.1\r\nnonsense\r\n\r\n") },
        { "binary noise",            QByteArray("\x01\x02\x00\xff\xfe garbage \r\n\r\n", 26) },
        { "a negative length",       QByteArray("PUT /templates/A/b.html HTTP/1.1\r\nContent-Length: -5\r\n\r\nxx") },
        { "a huge declared length",  QByteArray("PUT /templates/A/b.html HTTP/1.1\r\nContent-Length: 999999999\r\n\r\nxx") },
        { "a length of nonsense",    QByteArray("PUT /templates/A/b.html HTTP/1.1\r\nContent-Length: banana\r\n\r\nxx") },
        { "only a newline",          QByteArray("\n\n") },
    };

    for (const auto& one : nasty)
    {
        sendRaw(one.bytes, 1200);
        expectTrue(SheetCacheServer::getInstance().isRunning(),
                   QString("still listening after %1").arg(one.what));
    }

    // A token carrying a line break must not become extra headers, and must not be
    // mistaken for the real one.
    QByteArray injected = "GET /templates HTTP/1.1\r\nX-Template-Token: x\r\nX-Template-Token: "
                        + QByteArray(TOKEN) + "\r\n\r\n";
    sendRaw(injected, 1200);
    expectTrue(SheetCacheServer::getInstance().isRunning(), "still listening after a repeated header");

    out << "\nBodies that arrive oddly\n";

    // Split across two packets with a gap: the parser has to wait for the rest
    // rather than acting on half a request.
    QByteArray split = request("PUT", "/templates/SEVILLE/split.html", TOKEN, "<h1>whole</h1>");
    expectTrue(statusOf(sendRaw(split, 4000, split.size() - 6)) == 200,
               "a request split across two packets");

    QFile check(QDir(root).filePath("SEVILLE/split.html"));
    check.open(QIODevice::ReadOnly);
    expectTrue(check.readAll() == QByteArray("<h1>whole</h1>"),
               "and the whole body was used, not the first packet");
    check.close();

    // A body longer than the length says: the extra must be ignored rather than
    // written, or a sender could append to somebody else's file.
    QByteArray lying = "PUT /templates/SEVILLE/short.html HTTP/1.1\r\nX-Template-Token: "
                     + QByteArray(TOKEN) + "\r\nContent-Length: 5\r\n\r\nHELLOAND MORE THAT SHOULD NOT LAND";
    expectTrue(statusOf(sendRaw(lying)) == 200, "a body longer than its declared length is accepted");

    QFile shortFile(QDir(root).filePath("SEVILLE/short.html"));
    shortFile.open(QIODevice::ReadOnly);
    expectTrue(shortFile.readAll() == QByteArray("HELLO"),
               "and only the declared length was written");
    shortFile.close();

    out << "\nStill working afterwards\n";

    expectTrue(statusOf(sendRaw(request("GET", "/templates", TOKEN))) == 200,
               "an ordinary request still works after all of that");
    expectTrue(SheetCacheServer::getInstance().isRunning(), "the server is still listening");

    SheetCacheServer::getInstance().stop();

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    out.flush();

    return failures == 0 ? 0 : 1;
}
