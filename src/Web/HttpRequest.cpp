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

void HttpRequest::sendGet(const QString& url, const QUrlQuery& query)
{
    QUrl request(url);
    request.setQuery(query);

    this->pendingGetUrl = request.toString();

    qDebug("HttpRequest::sendGet %s", qPrintable(this->pendingGetUrl));

    this->networkManager = new QNetworkAccessManager(this);
    QObject::connect(this->networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(sendGetFinished(QNetworkReply*)));
    this->networkManager->get(QNetworkRequest(request));
}

void HttpRequest::sendGetFinished(QNetworkReply* reply)
{
    QString data = QString::fromUtf8(reply->readAll());
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    qDebug("HttpRequest::sendGetFinished %d %s", statusCode, qPrintable(data));

    HttpResponseLog::getInstance().logResponse("GET", this->pendingGetUrl, statusCode, data);

    reply->deleteLater();
    this->networkManager->deleteLater();
}

void HttpRequest::sendPost(const QString& url, const QUrlQuery& query)
{
    this->pendingPostUrl = url;

    qDebug("HttpRequest::sendPost %s, %s", qPrintable(url), qPrintable(query.toString(QUrl::FullyEncoded)));

    this->networkManager = new QNetworkAccessManager(this);
    QObject::connect(this->networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(sendPostFinished(QNetworkReply*)));
    this->networkManager->post(QNetworkRequest(QUrl(url)), query.toString(QUrl::FullyEncoded).toUtf8());
}

void HttpRequest::sendPostFinished(QNetworkReply* reply)
{
    QString data = QString::fromUtf8(reply->readAll());
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    qDebug("HttpRequest::sendPostFinished %d %s", statusCode, qPrintable(data));

    HttpResponseLog::getInstance().logResponse("POST", this->pendingPostUrl, statusCode, data);

    reply->deleteLater();
    this->networkManager->deleteLater();
}
