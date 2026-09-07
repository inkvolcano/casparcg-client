#include "RelayClient.h"

#include "DatabaseManager.h"
#include "TemplateInstaller.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMap>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace
{
    // A single poll should never turn into an afternoon of downloads. This is far
    // above any real pack and low enough that a misconfigured relay is noticed as a
    // refusal rather than as an hour of traffic.
    const int MAXIMUM_FILES_PER_POLL = 500;

    // Long enough for a slow venue link, short enough that a dead relay does not
    // hold the queue open until someone notices.
    const int TIMEOUT_MS = 30000;
}

RelayClient& RelayClient::getInstance()
{
    static RelayClient instance;
    return instance;
}

RelayClient::RelayClient()
    : QObject(nullptr)
{
    this->network = new QNetworkAccessManager(this);
    this->network->setTransferTimeout(TIMEOUT_MS);

    this->timer = new QTimer(this);
    QObject::connect(this->timer, &QTimer::timeout, this, [this]() { checkNow(); });
}

// ---- settings ----

bool RelayClient::isEnabled()
{
    return DatabaseManager::getInstance().getConfigurationByName("RelayEnabled").getValue() == "true";
}

QString RelayClient::url()
{
    return DatabaseManager::getInstance().getConfigurationByName("RelayUrl").getValue().trimmed();
}

QString RelayClient::token()
{
    return DatabaseManager::getInstance().getConfigurationByName("RelayToken").getValue().trimmed();
}

int RelayClient::pollMinutes()
{
    int minutes = DatabaseManager::getInstance().getConfigurationByName("RelayPollMinutes").getValue().toInt();
    return minutes > 0 ? minutes : 15;
}

QStringList RelayClient::packFilter()
{
    QString raw = DatabaseManager::getInstance().getConfigurationByName("RelayPacks").getValue().trimmed();
    if (raw.isEmpty())
        return QStringList();

    QStringList packs;
    foreach (const QString& name, raw.split(',', Qt::SkipEmptyParts))
        packs.append(name.trimmed());

    return packs;
}

// The operator may paste the URL with a query already on it, or without. Both
// should work rather than one of them silently doing nothing.
QString RelayClient::endpoint(const QString& action)
{
    QString base = url();
    if (base.isEmpty())
        return QString();

    return base + (base.contains('?') ? "&action=" : "?action=") + action;
}

// ---- running ----

void RelayClient::start()
{
    this->timer->stop();

    if (!isEnabled() || url().isEmpty() || token().isEmpty())
        return;

    this->timer->start(pollMinutes() * 60 * 1000);

    // A client that has just been switched on is the one most likely to be behind,
    // so ask once now rather than waiting out the first interval.
    QTimer::singleShot(5000, this, [this]() { checkNow(); });
}

void RelayClient::stop()
{
    this->timer->stop();
}

void RelayClient::say(const QString& line)
{
    emit progress(line);
}

bool RelayClient::checkNow()
{
    if (this->busy)
        return false;

    if (url().isEmpty() || token().isEmpty())
    {
        // An empty token would be sent as an empty header and refused, which is a
        // worse way to learn this than being told.
        say("Relay: no address or no token, nothing to poll.");
        return false;
    }

    if (TemplateInstaller::templatesRoot().isEmpty())
    {
        say("Relay: no template folder is configured on this machine.");
        return false;
    }

    this->busy = true;
    this->installed = 0;
    this->failed = 0;
    this->queue.clear();

    requestManifest();
    return true;
}

void RelayClient::ping()
{
    if (url().isEmpty() || token().isEmpty())
    {
        say("Relay: no address or no token.");
        return;
    }

    QNetworkRequest request((QUrl(endpoint("ping"))));
    request.setRawHeader("X-Relay-Token", token().toUtf8());

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200)
        {
            say(QString("Relay: %1").arg(status == 401 ? "wrong or missing token" : reply->errorString()));
            return;
        }

        QJsonObject info = QJsonDocument::fromJson(reply->readAll()).object();
        say(QString("Relay: reached \"%1\", %2 pack(s) available.")
            .arg(info.value("relay").toString(), QString::number(info.value("packs").toInt())));
    });
}

void RelayClient::requestManifest()
{
    say("Relay: asking what is there");

    QNetworkRequest request((QUrl(endpoint("manifest"))));
    request.setRawHeader("X-Relay-Token", token().toUtf8());

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200)
        {
            done(status == 401 ? "wrong or missing token"
                               : QString("could not read the manifest (%1)").arg(reply->errorString()));
            return;
        }

        planFrom(reply->readAll());
    });
}

// What the relay has, minus what this machine already has. Everything that decides
// whether a file is fetched happens here, so the fetch loop stays a fetch loop.
void RelayClient::planFrom(const QByteArray& manifestJson)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(manifestJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        done("the relay did not answer with a manifest");
        return;
    }

    QStringList wantedPacks = packFilter();
    int skippedPacks = 0;

    foreach (const QJsonValue& value, document.object().value("packs").toArray())
    {
        QJsonObject pack = value.toObject();
        QString name = pack.value("name").toString();
        if (name.isEmpty())
            continue;

        // An empty filter follows everything. A filter that names packs lets one relay
        // carry every venue while each client takes only the packs that are its own.
        if (!wantedPacks.isEmpty() && !wantedPacks.contains(name, Qt::CaseInsensitive))
        {
            skippedPacks++;
            continue;
        }

        // What is already here, by digest, so an unchanged file is never fetched.
        QMap<QString, QString> mine;
        foreach (const QJsonValue& entry, TemplateInstaller::describePack(name).value("files").toArray())
        {
            QJsonObject file = entry.toObject();
            mine.insert(QString(file.value("path").toString()).replace('\\', '/'),
                        file.value("sha1").toString());
        }

        foreach (const QJsonValue& entry, pack.value("files").toArray())
        {
            QJsonObject file = entry.toObject();
            QString path = QString(file.value("path").toString()).replace('\\', '/');
            QString sha1 = file.value("sha1").toString();

            if (path.isEmpty() || sha1.isEmpty())
                continue;

            // Refused by the installer anyway; skipped here so it is not a download
            // that gets thrown away and reported as a failure.
            if (TemplateInstaller::isProtected(path))
                continue;

            // The same rule that guards the installer, applied before the fetch so a
            // relay serving a path this client will not write costs nothing.
            if (!TemplateInstaller::isSafeRelativePath(path))
            {
                say(QString("  %1 / %2 refused: not a usable path").arg(name, path));
                continue;
            }

            if (mine.value(path) == sha1)
                continue;   // identical, and the digest is the evidence rather than a guess

            Wanted wanted;
            wanted.pack = name;
            wanted.relativePath = path;
            wanted.sha1 = sha1;
            wanted.bytes = static_cast<qint64>(file.value("bytes").toDouble());
            this->queue.append(wanted);
        }
    }

    if (this->queue.count() > MAXIMUM_FILES_PER_POLL)
    {
        done(QString("the relay offered %1 files, which is more than one poll will take. "
                     "Check the address and the pack list.").arg(this->queue.count()));
        return;
    }

    if (this->queue.isEmpty())
    {
        done(skippedPacks > 0 ? QString("already up to date (%1 pack(s) not followed)").arg(skippedPacks)
                              : "already up to date");
        return;
    }

    say(QString("Relay: %1 file(s) to fetch").arg(this->queue.count()));
    fetchNext();
}

// One at a time, on purpose: this runs on a machine that may be on air, over a link
// that may be someone's phone, against a host that may be shared.
void RelayClient::fetchNext()
{
    if (this->queue.isEmpty())
    {
        done(QString());
        return;
    }

    Wanted wanted = this->queue.takeFirst();

    QString target = QString("%1&pack=%2&path=%3")
        .arg(endpoint("fetch"),
             QString::fromUtf8(QUrl::toPercentEncoding(wanted.pack)),
             QString::fromUtf8(QUrl::toPercentEncoding(wanted.relativePath, "/")));

    QNetworkRequest request((QUrl(target)));
    request.setRawHeader("X-Relay-Token", token().toUtf8());

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, wanted]() {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200)
        {
            this->failed++;
            say(QString("  %1 / %2 failed: %3").arg(wanted.pack, wanted.relativePath,
                                                    status == 401 ? "wrong or missing token"
                                                                  : reply->errorString()));
            fetchNext();
            return;
        }

        QByteArray body = reply->readAll();

        // The manifest said what these bytes would be. If they are not that, something
        // between here and the relay changed them, and the last thing to do with bytes
        // like that is write them where CasparCG will run them.
        QString actual = QString::fromLatin1(
            QCryptographicHash::hash(body, QCryptographicHash::Sha1).toHex());
        if (actual != wanted.sha1)
        {
            this->failed++;
            say(QString("  %1 / %2 refused: the bytes do not match the digest the relay promised")
                .arg(wanted.pack, wanted.relativePath));
            fetchNext();
            return;
        }

        QString error;
        int result = TemplateInstaller::installFile(wanted.pack, wanted.relativePath, body, &error);
        if (result == 200)
        {
            this->installed++;
            say(QString("  %1 / %2 installed").arg(wanted.pack, wanted.relativePath));
        }
        else
        {
            this->failed++;
            say(QString("  %1 / %2 failed: %3").arg(wanted.pack, wanted.relativePath, error));
        }

        fetchNext();
    });
}

void RelayClient::done(const QString& note)
{
    this->busy = false;
    this->ranAt = QDateTime::currentDateTime();

    if (!note.isEmpty())
        this->summary = note;
    else if (this->failed > 0)
        this->summary = QString("%1 installed, %2 failed").arg(this->installed).arg(this->failed);
    else
        this->summary = QString("%1 file(s) installed").arg(this->installed);

    say(QString("Relay: %1").arg(this->summary));
    emit finished(this->installed, this->failed, this->summary);
}
