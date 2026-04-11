#pragma once

#include "Shared.h"

#include <QtCore/QObject>
#include <QtCore/QString>

class WEB_EXPORT HttpResponseLog : public QObject
{
    Q_OBJECT

    public:
        static HttpResponseLog& getInstance();

        void logResponse(const QString& method, const QString& url, int statusCode, const QString& body);

        Q_SIGNAL void responseReceived(const QString& method, const QString& url, int statusCode, const QString& body);

    private:
        explicit HttpResponseLog(QObject* parent = nullptr);
};
