#pragma once

#include "Shared.h"

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

#include <QtNetwork/QTcpSocket>

// Asks a CasparCG server which NDI sources it can see (AMCP NDI LIST) on a
// connection of its own, so the reply never mixes with the client's main AMCP
// traffic. The server is the one that will receive the source, so its list is
// the one that counts, not this machine's. Emits finished once, then deletes
// itself.
class WIDGETS_EXPORT NdiSourceQuery : public QObject
{
    Q_OBJECT

    public:
        static NdiSourceQuery* ask(const QString& address, int port, QObject* parent);

        // problem is empty when the server answered with a list (which may be empty).
        Q_SIGNAL void finished(const QStringList& sources, const QString& problem);

    private:
        explicit NdiSourceQuery(QObject* parent);

        QTcpSocket socket;
        QTimer timeout;
        QByteArray reply;
        bool done = false;

        void finish(const QStringList& sources, const QString& problem);
};
