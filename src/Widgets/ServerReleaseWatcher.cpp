#include "ServerReleaseWatcher.h"

#include "DatabaseManager.h"
#include "ServerReleases.h"

#include <QtCore/QUrl>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace
{
    const int FIRST_CHECK_MS = 60 * 1000;
    const int CHECK_EVERY_MS = 24 * 60 * 60 * 1000;
}

ServerReleaseWatcher::ServerReleaseWatcher(QObject* parent)
    : QObject(parent)
{
    this->network = new QNetworkAccessManager(this);

    // A minute in, so it never competes with the client starting up.
    this->timer.setSingleShot(true);
    QObject::connect(&this->timer, &QTimer::timeout, this, [this]() {
        check();
        this->timer.start(CHECK_EVERY_MS);
    });
    this->timer.start(FIRST_CHECK_MS);
}

bool ServerReleaseWatcher::enabled()
{
    return DatabaseManager::getInstance().getConfigurationByName("ServerReleaseCheck").getValue() != "false";
}

void ServerReleaseWatcher::setEnabled(bool on)
{
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "ServerReleaseCheck", on ? "true" : "false"));
}

void ServerReleaseWatcher::check()
{
    if (!enabled())
    {
        if (!this->latest.isEmpty())
        {
            this->latest.clear();
            emit changed();
        }
        return;
    }

    QNetworkRequest request((QUrl(QString(ServerReleases::GITHUB_API))));
    request.setRawHeader("User-Agent", "CasparCG-Client");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(30000);

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        // A failed check changes nothing: what was known stays known.
        if (reply->error() != QNetworkReply::NoError)
            return;

        const ServerReleases::Build newest = ServerReleases::newestStable(ServerReleases::parseGitHubReleases(reply->readAll()));
        if (newest.version.isEmpty() || newest.version == this->latest)
            return;

        this->latest = newest.version;
        emit changed();
    });
}
