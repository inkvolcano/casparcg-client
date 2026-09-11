#include "OscControlListener.h"
#include "OscSubscriptionRegistry.h"

#include <QtCore/QString>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QPair>
#include <QtCore/QDebug>

OscControlListener::OscControlListener(QObject* parent)
    : QObject(parent)
{
}

OscControlListener::~OscControlListener()
{
    if (this->thread != nullptr)
    {
        this->thread->stop();
        this->thread->wait();

        delete this->socket;
        delete this->multiplexer;
    }
}

void OscControlListener::start(int port, int batchInterval)
{
    try
    {
        this->port = port;
        this->batchInterval = batchInterval;

        this->socket = new UdpSocket();
        this->socket->SetAllowReuse(true);
        this->socket->Bind(IpEndpointName("0.0.0.0", this->port));

        qDebug("Listening for incoming OSC control messages over UDP on port %d", this->port);

        this->multiplexer = new SocketReceiveMultiplexer();
        this->multiplexer->AttachSocketListener(this->socket, this);

        this->thread = new OscThread(this->multiplexer, this);
        this->thread->start();

        QTimer::singleShot(this->batchInterval, this, SLOT(sendEventBatch()));
    }
    catch (std::runtime_error &e)
    {
        qDebug("%s", qPrintable(QString::fromStdString(e.what()).trimmed()));
    }
 }

void OscControlListener::ProcessPacket(const char* data, int size, const IpEndpointName& endpoint)
{
    // The parse is the throwing part: ReceivedPacket/ReceivedMessage reject a
    // malformed datagram, and without this that exception unwinds the OSC thread
    // into std::terminate. A bad packet from anything on the network — a UDP port
    // scan, or a truncated frame from the server itself — is a dropped message now.
    try
    {
        osc::OscPacketListener::ProcessPacket(data, size, endpoint);
    }
    catch (const osc::Exception& e)
    {
        qDebug("Dropped a malformed OSC control packet (%d bytes): %s", size, e.what());
    }
}

void OscControlListener::ProcessMessage(const osc::ReceivedMessage& message, const IpEndpointName& endpoint)
{
    char addressBuffer[256];

    endpoint.AddressAsString(addressBuffer);

    QList<QVariant> arguments;
    for (osc::ReceivedMessage::const_iterator iterator = message.ArgumentsBegin(); iterator != message.ArgumentsEnd(); ++iterator)
    {
        const osc::ReceivedMessageArgument& argument = *iterator;

        if (argument.IsBool())
            arguments.append(argument.AsBool());
        else if (argument.IsInt32())
            arguments.append(QVariant::fromValue<qint32>(argument.AsInt32()));
        else if (argument.IsInt64())
            arguments.append(QVariant::fromValue<qint64>(argument.AsInt64()));
        else if (argument.IsFloat())
            arguments.append(argument.AsFloat());
        else if (argument.IsDouble())
            arguments.append(argument.AsDouble());
        else if (argument.IsString())
            arguments.append(argument.AsString());
    }

    QString eventMessage = QString("%1").arg(message.AddressPattern());
    QString eventPath = QString("%1%2").arg(addressBuffer).arg(message.AddressPattern());

    //qDebug("DEBUG: OSC control message received: %s", qPrintable(eventPath));

    QMutexLocker locker(&eventsMutex);
    if (eventMessage.startsWith("/control"))
    {
        qDebug("Received OSC control message over UDP from %s:%d: %s", qPrintable(addressBuffer), this->port, qPrintable(eventMessage));

        // Do not overwrite control commands already in queue.
        if (!this->events.contains(eventPath))
            this->events[eventPath] = arguments;
    }
}

void OscControlListener::sendEventBatch()
{
    QMap<QString, QList<QVariant>> other;
    {
        QMutexLocker locker(&eventsMutex);
        this->events.swap(other);
    }

    foreach (const QString& eventPath, other.keys())
        OscSubscriptionRegistry::getInstance().dispatch(eventPath, other[eventPath]);

    QTimer::singleShot(this->batchInterval, this, SLOT(sendEventBatch()));
}
