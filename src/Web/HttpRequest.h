#pragma once

#include "Shared.h"

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUrlQuery>

#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkAccessManager>

class WEB_EXPORT HttpRequest : public QObject
{
    Q_OBJECT

    public:
        explicit HttpRequest(QObject* parent = 0);

        void sendGet(const QString& url, const QUrlQuery& query);
        void sendPost(const QString& url, const QUrlQuery& query);

    private:
        // One for the life of the item, made on first use. A manager per request
        // meant no connection reuse, and a second send before the first answered
        // overwrote the pointer: the first answer then deleted the second
        // request's manager, cancelling it, and was logged under its URL.
        QNetworkAccessManager* networkManager = nullptr;

        QNetworkAccessManager* manager();
        void logReply(const QString& method, const QString& url, QNetworkReply* reply);
};
