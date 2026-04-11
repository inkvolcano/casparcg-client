#include "HttpResponseLog.h"

HttpResponseLog::HttpResponseLog(QObject* parent)
    : QObject(parent)
{
}

HttpResponseLog& HttpResponseLog::getInstance()
{
    static HttpResponseLog instance;
    return instance;
}

void HttpResponseLog::logResponse(const QString& method, const QString& url, int statusCode, const QString& body)
{
    emit responseReceived(method, url, statusCode, body);
}
