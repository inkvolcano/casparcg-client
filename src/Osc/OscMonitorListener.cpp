#include "OscMonitorListener.h"
#include "OscSubscriptionRegistry.h"

#include <QtCore/QString>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QPair>
#include <QtCore/QDebug>

OscMonitorListener::OscMonitorListener(QObject* parent)
    : QObject(parent)
{
}

OscMonitorListener::~OscMonitorListener()
{
    if (this->thread != nullptr)
    {
        this->thread->stop();
        this->thread->wait();

        delete this->socket;
        delete this->multiplexer;
    }
}

void OscMonitorListener::start(int port, int batchInterval)
{
    try
    {
        this->port = port;
        this->batchInterval = batchInterval;

        this->socket = new UdpSocket();
        this->socket->SetAllowReuse(true);
        this->socket->Bind(IpEndpointName("0.0.0.0", this->port));

        qDebug("Listening for incoming OSC monitor messages over UDP on port %d", this->port);

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

void OscMonitorListener::ProcessPacket(const char* data, int size, const IpEndpointName& endpoint)
{
    // See OscControlListener: a malformed datagram throws in the parse, and off
    // the OSC thread that is std::terminate. Dropped rather than fatal.
    try
    {
        osc::OscPacketListener::ProcessPacket(data, size, endpoint);
    }
    catch (const osc::Exception& e)
    {
        qDebug("Dropped a malformed OSC monitor packet (%d bytes): %s", size, e.what());
    }
}

void OscMonitorListener::ProcessMessage(const osc::ReceivedMessage& message, const IpEndpointName& endpoint)
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

    //qDebug("DEBUG: OSC monitor message received: %s", qPrintable(eventPath));

    QMutexLocker locker(&eventsMutex);
    if (!eventMessage.startsWith("/control"))
        this->events[eventPath] = arguments;
}

void OscMonitorListener::sendEventBatch()
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
