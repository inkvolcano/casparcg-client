#include "SheetCacheServer.h"

#include "SheetDataResolver.h"
#include "TemplateInstaller.h"

#include "DatabaseManager.h"
#include "Models/ConfigurationModel.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonParseError>
#include <QtCore/QJsonObject>
#include <QtCore/QLocale>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>

SheetCacheServer::SheetCacheServer()
    : QObject()
{
}

SheetCacheServer& SheetCacheServer::getInstance()
{
    static SheetCacheServer instance;
    return instance;
}

bool SheetCacheServer::isEnabled()
{
    return DatabaseManager::getInstance().getConfigurationByName("SheetsHostCache").getValue() == "true";
}

int SheetCacheServer::configuredPort()
{
    QString configured = DatabaseManager::getInstance().getConfigurationByName("SheetsHostCachePort").getValue();
    int port = configured.toInt();
    return (port > 0) ? port : 3000;
}

// Defaults beside the executable so the files can be looked at, and so an existing
// sheets_data folder can simply be pointed at.
QString SheetCacheServer::cacheDirectory()
{
    QString configured = DatabaseManager::getInstance().getConfigurationByName("SheetsCacheDirectory").getValue();
    if (!configured.trimmed().isEmpty())
        return configured.trimmed();

    return QCoreApplication::applicationDirPath() + "/sheets_data";
}

// Cache files are "<spreadsheetId>_<sheet>.json" and nothing else in the folder is
// ours. Matching the shape rather than taking every .json means a shared folder \xe2\x80\x94
// with the PHP service, or with anything the operator keeps beside it \xe2\x80\x94 survives.
static QStringList cachedSheetFiles()
{
    QDir directory(SheetCacheServer::cacheDirectory());
    if (!directory.exists())
        return QStringList();

    QStringList names;
    foreach (const QString& name, directory.entryList(QStringList() << "*_*.json", QDir::Files))
        names.append(directory.filePath(name));

    return names;
}

int SheetCacheServer::cachedSheetCount(qint64* totalBytes)
{
    QStringList files = cachedSheetFiles();

    if (totalBytes != nullptr)
    {
        *totalBytes = 0;
        foreach (const QString& path, files)
            *totalBytes += QFileInfo(path).size();
    }

    return files.count();
}

// Answers how many went, or -1 when something would not go. A cache is rebuilt by
// being read, so this is safe in a way that deleting most things is not \xe2\x80\x94 but it is
// still a delete, so it reports what happened rather than failing quietly.
int SheetCacheServer::clearCache(QString* error)
{
    int removed = 0;
    QStringList stubborn;

    foreach (const QString& path, cachedSheetFiles())
    {
        if (QFile::remove(path))
            removed++;
        else
            stubborn.append(QFileInfo(path).fileName());
    }

    if (!stubborn.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QString("Could not remove %1 of %2 file(s): %3")
                .arg(stubborn.count())
                .arg(removed + stubborn.count())
                .arg(stubborn.mid(0, 3).join(", "));
        }

        return -1;
    }

    return removed;
}

bool SheetCacheServer::isBypassing()
{
    return DatabaseManager::getInstance().getConfigurationByName("SheetsCacheBypass").getValue() == "true";
}

void SheetCacheServer::setBypassing(bool bypass)
{
    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, "SheetsCacheBypass", bypass ? "true" : "false"));
}

bool SheetCacheServer::mayChangeBypass(const QHostAddress& peer, const QString& pushToken)
{
    // This port is open to the whole network, and bypass is persisted: one URL
    // from the guest Wi-Fi used to turn a venue's cache off until somebody
    // noticed. Local callers keep the URL the settings label promises; anything
    // else needs the same token a template push does, and an unset token never
    // matches, as on that route.
    if (peer.isLoopback())
        return true;

    // A dual-stack listener sees localhost as ::ffff:127.0.0.1.
    bool isIPv4 = false;
    const quint32 v4 = peer.toIPv4Address(&isIPv4);
    if (isIPv4 && QHostAddress(v4).isLoopback())
        return true;

    const QString expected = TemplateInstaller::token();
    return !expected.isEmpty() && pushToken == expected;
}

QString SheetCacheServer::cacheFilePath(const QString& spreadsheetId, const QString& sheetNumber)
{
    // Same names the PHP service uses, so the two can share a folder.
    static QRegularExpression unsafe("[^A-Za-z0-9._-]");
    QString id = QString(spreadsheetId).replace(unsafe, "_");
    QString number = QString(sheetNumber).replace(unsafe, "_");

    return QString("%1/%2_%3.json").arg(cacheDirectory()).arg(id).arg(number);
}

bool SheetCacheServer::isRunning() const
{
    return this->server != nullptr && this->server->isListening();
}

int SheetCacheServer::listeningPort() const
{
    return isRunning() ? static_cast<int>(this->server->serverPort()) : 0;
}

void SheetCacheServer::start()
{
    if (isRunning())
        return;

    // The push endpoint rides on this same socket, so the server is wanted if
    // either feature is on.
    if (!isEnabled() && !TemplateInstaller::isEnabled())
        return;

    QDir().mkpath(cacheDirectory());

    if (this->server == nullptr)
    {
        this->server = new QTcpServer(this);
        QObject::connect(this->server, SIGNAL(newConnection()), this, SLOT(acceptConnection()));
    }

    int port = configuredPort();
    if (this->server->listen(QHostAddress::Any, static_cast<quint16>(port)))
        qDebug("Hosting the sheet cache on port %d, files in %s", port, qPrintable(cacheDirectory()));
    else
        qWarning("Unable to host the sheet cache on port %d: %s", port, qPrintable(this->server->errorString()));
}

void SheetCacheServer::stop()
{
    if (this->server == nullptr)
        return;

    this->server->close();

    foreach (QTcpSocket* socket, this->buffers.keys())
        socket->abort();

    this->buffers.clear();
}

void SheetCacheServer::acceptConnection()
{
    while (this->server->hasPendingConnections())
    {
        QTcpSocket* socket = this->server->nextPendingConnection();
        this->buffers.insert(socket, QByteArray());

        QObject::connect(socket, SIGNAL(readyRead()), this, SLOT(readFromSocket()));
        QObject::connect(socket, SIGNAL(disconnected()), this, SLOT(forgetSocket()));
    }
}

void SheetCacheServer::forgetSocket()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket == nullptr)
        return;

    this->buffers.remove(socket);
    socket->deleteLater();
}

// A deliberately small slice of HTTP: read until the headers are complete, take the
// body if one was announced, answer once and close. Nothing here needs keep-alive.
void SheetCacheServer::readFromSocket()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket == nullptr || !this->buffers.contains(socket))
        return;

    QByteArray& buffer = this->buffers[socket];
    buffer.append(socket->readAll());

    qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0)
    {
        if (buffer.size() > 64 * 1024)
            socket->abort();   // not a request we are going to understand

        return;
    }

    QByteArray head = buffer.left(headerEnd);
    QList<QByteArray> lines = head.split('\n');
    if (lines.isEmpty())
    {
        respond(socket, 400, "{\"error\":\"Malformed request\"}");
        return;
    }

    QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
    if (requestLine.count() < 2)
    {
        respond(socket, 400, "{\"error\":\"Malformed request\"}");
        return;
    }

    QString method = QString::fromLatin1(requestLine.at(0)).toUpper();
    QString target = QString::fromLatin1(requestLine.at(1));

    qsizetype contentLength = 0;
    QString pushToken;
    QString contentSha1;
    for (int i = 1; i < lines.count(); i++)
    {
        QByteArray line = lines.at(i).trimmed();
        if (line.toLower().startsWith("content-length:"))
            contentLength = line.mid(line.indexOf(':') + 1).trimmed().toLongLong();
        else if (line.toLower().startsWith("x-template-token:"))
            pushToken = QString::fromUtf8(line.mid(line.indexOf(':') + 1).trimmed());
        else if (line.toLower().startsWith("x-content-sha1:"))
            contentSha1 = QString::fromUtf8(line.mid(line.indexOf(':') + 1).trimmed());
    }

    QByteArray body = buffer.mid(headerEnd + 4);
    if (body.size() < contentLength)
        return;   // the rest is still on its way

    body = body.left(contentLength);

    handle(socket, method, target, body, pushToken, contentSha1);
}

void SheetCacheServer::handle(QTcpSocket* socket, const QString& method, const QString& target,
                              const QByteArray& body, const QString& pushToken,
                              const QString& contentSha1)
{
    QUrl url(target);
    QUrlQuery query(url.query());
    QString path = url.path();

    if (method == "OPTIONS")
    {
        respond(socket, 204, QByteArray());
        return;
    }

    // Named routes first. The cache below is matched on its parameters rather than
    // its path, so that an unedited template still reaches it \xe2\x80\x94 but that is a
    // fallback, and a request that named a route outright must not fall into it.
    // ---- strain ----
    if (path.endsWith("/strain") || path == "/strain")
    {
        // Another application reporting what it is spending. The same body this
        // client publishes is accepted verbatim, so one can report to another.
        if (method == "POST")
        {
            QJsonParseError parseError;
            QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
            {
                respond(socket, 400, "{\"error\":\"Expected a JSON object\"}");
                return;
            }

            QString reason;
            if (!SheetDataResolver::getInstance().acceptExternalReport(document.object(), &reason))
            {
                QJsonObject failure;
                failure.insert("error", reason);
                respond(socket, 400, QJsonDocument(failure).toJson(QJsonDocument::Compact));
                return;
            }

            respond(socket, 200, "{\"ok\":true}");
            return;
        }

        if (query.hasQueryItem("tick"))
        {
            QString spreadsheetId = query.queryItemValue("spreadsheetId");
            if (spreadsheetId.isEmpty())
            {
                respond(socket, 400, "{\"error\":\"Missing spreadsheetId\"}");
                return;
            }

            QString source = query.queryItemValue("source");

            // One read unless told otherwise, so the simple form stays simple while
            // something batching its own reads can say so in one request.
            int count = query.hasQueryItem("count") ? query.queryItemValue("count").toInt() : 1;
            count = qBound(1, count, 1000);

            for (int i = 0; i < count; i++)
                SheetDataResolver::getInstance().noteTemplateRead(spreadsheetId, source.isEmpty() ? "template" : source);

            respond(socket, 200, "{\"ok\":true}");
            return;
        }

        QJsonObject report = SheetDataResolver::getInstance().strainReport();
        respond(socket, 200, QJsonDocument(report).toJson(QJsonDocument::Indented));
        return;
    }

    // ---- bypass ----
    if (path.endsWith("/bypass") || path == "/bypass")
    {
        if (query.hasQueryItem("on"))
        {
            if (!mayChangeBypass(socket->peerAddress(), pushToken))
            {
                respond(socket, 403, "{\"error\":\"Bypass can only be changed from this machine, "
                                     "or with the template push token\"}");
                return;
            }

            QString value = query.queryItemValue("on").toLower();
            setBypassing(value == "1" || value == "true" || value == "yes");
        }

        QJsonObject state;
        state.insert("bypass", isBypassing());
        state.insert("cacheDirectory", cacheDirectory());
        respond(socket, 200, QJsonDocument(state).toJson(QJsonDocument::Compact));
        return;
    }

    // ---- template push ----
    // A template is HTML that CasparCG executes, so this is off unless switched on
    // and every request carries the token from Settings. Both checks come before
    // anything is read off the path.
    if (path == "/templates" || path.startsWith("/templates/"))
    {
        if (!TemplateInstaller::isEnabled())
        {
            respond(socket, 403, "{\"error\":\"Template push is switched off on this client\"}");
            return;
        }

        QString peer = socket->peerAddress().toString();
        if (TemplateInstaller::isThrottled(peer))
        {
            respond(socket, 429, "{\"error\":\"Too many wrong tokens from this address; try later\"}");
            return;
        }

        QString expected = TemplateInstaller::token();
        if (expected.isEmpty() || pushToken != expected)
        {
            // An unset token never matches, so switching the feature on without
            // setting one leaves it shut rather than open.
            TemplateInstaller::noteBadToken(peer);
            respond(socket, 401, "{\"error\":\"Missing or wrong X-Template-Token\"}");
            return;
        }

        TemplateInstaller::noteGoodToken(peer);

        QString remainder = path.mid(QString("/templates").length());
        if (remainder.startsWith('/'))
            remainder = remainder.mid(1);

        if (remainder == "info")
        {
            respond(socket, 200, QJsonDocument(TemplateInstaller::identify()).toJson(QJsonDocument::Indented));
            return;
        }

        if (remainder.isEmpty())
        {
            if (method != "GET")
            {
                respond(socket, 405, "{\"error\":\"Method not allowed\"}");
                return;
            }

            respond(socket, 200, QJsonDocument(TemplateInstaller::listPacks()).toJson(QJsonDocument::Indented));
            return;
        }

        int slash = remainder.indexOf('/');
        QString pack = (slash < 0) ? remainder : remainder.left(slash);
        QString relativePath = (slash < 0) ? QString() : remainder.mid(slash + 1);

        if (relativePath.isEmpty())
        {
            if (method != "GET")
            {
                respond(socket, 405, "{\"error\":\"Method not allowed\"}");
                return;
            }

            bool found = false;
            QJsonObject described = TemplateInstaller::describePack(pack, &found);
            respond(socket, found ? 200 : 404, QJsonDocument(described).toJson(QJsonDocument::Indented));
            return;
        }

        if (method != "PUT")
        {
            respond(socket, 405, "{\"error\":\"Method not allowed\"}");
            return;
        }

        QString reason;
        int status = TemplateInstaller::installFile(pack, relativePath, body, &reason, contentSha1);
        if (status == 200)
        {
            QJsonObject ok;
            ok.insert("ok", true);
            ok.insert("pack", pack);
            ok.insert("path", relativePath);
            ok.insert("bytes", body.size());
            respond(socket, 200, QJsonDocument(ok).toJson(QJsonDocument::Compact));
            return;
        }

        QJsonObject failure;
        failure.insert("error", reason);
        failure.insert("pack", pack);
        failure.insert("path", relativePath);
        respond(socket, status, QJsonDocument(failure).toJson(QJsonDocument::Compact));
        return;
    }
    // ---- the cache itself ----
    // Matched on the parameters rather than the path, so a template still asking for
    // local_server.php reaches this untouched.
    if (query.hasQueryItem("spreadsheetId") && query.hasQueryItem("sheetNumber"))
    {
        QString spreadsheetId = query.queryItemValue("spreadsheetId");
        QString sheetNumber = query.queryItemValue("sheetNumber");
        QString filePath = cacheFilePath(spreadsheetId, sheetNumber);

        if (method == "POST")
        {
            // Writes always land. Bypass is about not serving, not about going cold.
            QDir().mkpath(cacheDirectory());

            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            {
                respond(socket, 500, "{\"error\":\"Cannot write cache file\"}");
                return;
            }

            file.write(body);
            file.close();

            // Somebody paid Google for this, so it counts as a read that happened \xe2\x80\x94
            // unless it was us, whose read was already counted on the way in.
            if (query.queryItemValue("via") != "client")
                SheetDataResolver::getInstance().noteTemplateRead(spreadsheetId, "cache-write");

            respond(socket, 200, "{\"success\":\"Sheet updated\"}");
            return;
        }

        if (method != "GET")
        {
            respond(socket, 405, "{\"error\":\"Method not allowed\"}");
            return;
        }

        if (isBypassing())
        {
            // Deliberately the same shape as a miss, with a word that says which it is.
            respond(socket, 404, "{\"error\":\"Sheet not found\",\"reason\":\"bypass\"}");
            return;
        }

        QFile file(filePath);
        if (!file.exists() || !file.open(QIODevice::ReadOnly))
        {
            respond(socket, 404, "{\"error\":\"Sheet not found\"}");
            return;
        }

        QByteArray cached = file.readAll();
        file.close();

        // When this copy was written. Without it a reader cannot tell a fresh cache
        // from a stale one, and both look identical on the wire.
        respond(socket, 200, cached, QFileInfo(filePath).lastModified());
        return;
    }

    respond(socket, 404, "{\"error\":\"No such route\"}");
}

void SheetCacheServer::respond(QTcpSocket* socket, int status, const QByteArray& body, const QDateTime& cachedAt)
{
    QString reason = "OK";
    switch (status)
    {
        case 204: reason = "No Content"; break;
        case 400: reason = "Bad Request"; break;
        case 404: reason = "Not Found"; break;
        case 401: reason = "Unauthorized"; break;
        case 403: reason = "Forbidden"; break;
        case 405: reason = "Method Not Allowed"; break;
        case 409: reason = "Conflict"; break;
        case 429: reason = "Too Many Requests"; break;
        case 500: reason = "Internal Server Error"; break;
        default: break;
    }

    QByteArray response;
    response.append(QString("HTTP/1.1 %1 %2\r\n").arg(status).arg(reason).toLatin1());
    response.append("Content-Type: application/json\r\n");
    // Graphics run from file:// as often as from http://, so neither origin can be named.
    response.append("Access-Control-Allow-Origin: *\r\n");
    response.append("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
    response.append("Access-Control-Allow-Headers: X-Requested-With, Content-Type, Accept, Origin\r\n");
    response.append("Cache-Control: no-store\r\n");

    if (cachedAt.isValid())
    {
        // ISO alongside the standard header: one is unambiguous to parse, the other is
        // what an ordinary HTTP client already understands.
        response.append(QString("Last-Modified: %1\r\n")
            .arg(QLocale::c().toString(cachedAt.toUTC(), "ddd, dd MMM yyyy HH:mm:ss 'GMT'")).toLatin1());
        response.append(QString("X-Sheet-Cached-At: %1\r\n")
            .arg(cachedAt.toUTC().toString(Qt::ISODate)).toLatin1());
    }
    response.append(QString("Content-Length: %1\r\n").arg(body.size()).toLatin1());
    response.append("Connection: close\r\n\r\n");
    response.append(body);

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}
