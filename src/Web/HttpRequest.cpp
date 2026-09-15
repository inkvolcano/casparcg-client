#include "HttpRequest.h"
#include "HttpResponseLog.h"

#include <QtCore/QDebug>
#include <QtCore/QPair>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkRequest>

HttpRequest::HttpRequest(QObject* parent)
    : QObject(parent)
{
}

QNetworkAccessManager* HttpRequest::manager()
{
    if (this->networkManager == nullptr)
        this->networkManager = new QNetworkAccessManager(this);

    return this->networkManager;
}

// Each reply carries its own URL to the log, so two requests in flight at once
// are each logged as themselves.
void HttpRequest::logReply(const QString& method, const QString& url, QNetworkReply* reply)
{
    QString data = QString::fromUtf8(reply->readAll());
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    qDebug("HttpRequest::send%sFinished %d %s", method == "GET" ? "Get" : "Post", statusCode, qPrintable(data));

    HttpResponseLog::getInstance().logResponse(method, url, statusCode, data);

    reply->deleteLater();
}

void HttpRequest::sendGet(const QString& url, const QUrlQuery& query)
{
    QUrl request(url);
    request.setQuery(query);

    const QString sentUrl = request.toString();

    qDebug("HttpRequest::sendGet %s", qPrintable(sentUrl));

    QNetworkReply* reply = manager()->get(QNetworkRequest(request));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, sentUrl]() {
        logReply("GET", sentUrl, reply);
    });
}

void HttpRequest::sendPost(const QString& url, const QUrlQuery& query)
{
    qDebug("HttpRequest::sendPost %s, %s", qPrintable(url), qPrintable(query.toString(QUrl::FullyEncoded)));

    QNetworkReply* reply = manager()->post(QNetworkRequest(QUrl(url)), query.toString(QUrl::FullyEncoded).toUtf8());
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        logReply("POST", url, reply);
    });
}
