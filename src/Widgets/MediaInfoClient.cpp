#include "MediaInfoClient.h"

#include "MediaInfoSummary.h"
#include "DeviceManager.h"
#include "EventManager.h"
#include "Models/DeviceModel.h"

#include <QtCore/QDateTime>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace
{
    const qint64 KEEP_KNOWN_MS = 10 * 60 * 1000;
    const qint64 KEEP_UNANSWERED_MS = 30 * 1000;
    const int TIMEOUT_MS = 3000;
}

MediaInfoClient& MediaInfoClient::getInstance()
{
    static MediaInfoClient* instance = new MediaInfoClient();
    return *instance;
}

MediaInfoClient::MediaInfoClient(QObject* parent)
    : QObject(parent)
{
    this->network = new QNetworkAccessManager(this);

    // A refresh is how an operator says the media has changed: a clip replaced
    // under the same name should not keep describing the old one.
    QObject::connect(&EventManager::getInstance(), SIGNAL(refreshLibrary(const RefreshLibraryEvent&)), this, SLOT(forget()));
}

void MediaInfoClient::forget()
{
    this->cache.clear();
}

void MediaInfoClient::request(const QString& deviceName, const QString& mediaName, bool still,
                              QObject* context, const std::function<void(const QString&, bool)>& done)
{
    const QSharedPointer<DeviceModel> device = DeviceManager::getInstance().getDeviceModelByName(deviceName);
    if (device == nullptr || mediaName.trimmed().isEmpty())
    {
        done(QString(), false);
        return;
    }

    const QString host = device->getAddress();
    const QString key = QString("%1|%2|%3").arg(host, mediaName.toUpper()).arg(still ? 1 : 0);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (this->cache.contains(key))
    {
        const Entry& entry = this->cache[key];
        if (now - entry.at < (entry.known ? KEEP_KNOWN_MS : KEEP_UNANSWERED_MS))
        {
            done(entry.text, entry.known);
            return;
        }
    }

    // The scanner's ids are the names the server lists, in capitals, with folders.
    QUrl url;
    url.setScheme("http");
    url.setHost(host);
    url.setPort(SCANNER_PORT);
    url.setPath("/media/info/" + mediaName.toUpper());

    QNetworkRequest request(url);
    request.setTransferTimeout(TIMEOUT_MS);

    QNetworkReply* reply = this->network->get(request);
    QPointer<QObject> guard(context);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, guard, done, key, host, still]() {
        reply->deleteLater();

        Entry entry;
        entry.at = QDateTime::currentMSecsSinceEpoch();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError && status == 0)
        {
            entry.text = QString("The scanner on %1:%2 did not answer").arg(host).arg(SCANNER_PORT);
        }
        else
        {
            entry.text = MediaInfoSummary::summarize(reply->readAll(), still);
            entry.known = !entry.text.isEmpty();
            if (!entry.known)
                entry.text = "The scanner has no details for this file";
        }

        this->cache.insert(key, entry);

        if (!guard.isNull())
            done(entry.text, entry.known);
    });
}
