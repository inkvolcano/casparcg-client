#include "NdiSourceQuery.h"

#include "NdiInputRules.h"

NdiSourceQuery::NdiSourceQuery(QObject* parent)
    : QObject(parent)
{
}

NdiSourceQuery* NdiSourceQuery::ask(const QString& address, int port, QObject* parent)
{
    NdiSourceQuery* query = new NdiSourceQuery(parent);

    QObject::connect(&query->socket, &QTcpSocket::connected, query, [query]() {
        query->socket.write("NDI LIST\r\n");
    });
    QObject::connect(&query->socket, &QTcpSocket::readyRead, query, [query]() {
        query->reply += query->socket.readAll();
        const QString text = QString::fromUtf8(query->reply);
        if (!NdiInputRules::replyComplete(text))
            return;

        if (text.startsWith("200"))
            query->finish(NdiInputRules::parseList(text), QString());
        else
            query->finish(QStringList(), "The server could not list NDI sources. Its NDI runtime may be missing, or the server is older than 2.4.");
    });
    QObject::connect(&query->socket, &QTcpSocket::errorOccurred, query, [query](QAbstractSocket::SocketError) {
        query->finish(QStringList(), "Could not reach the server: " + query->socket.errorString());
    });
    QObject::connect(&query->timeout, &QTimer::timeout, query, [query]() {
        query->finish(QStringList(), "The server did not answer within 5 seconds.");
    });

    query->timeout.setSingleShot(true);
    query->timeout.start(5000);
    query->socket.connectToHost(address, port);
    return query;
}

void NdiSourceQuery::finish(const QStringList& sources, const QString& problem)
{
    if (this->done)
        return;

    this->done = true;
    this->timeout.stop();
    this->socket.abort();
    emit finished(sources, problem);
    deleteLater();
}
