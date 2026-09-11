#include "OscThread.h"

#include <QtCore/QDebug>

#include <exception>

OscThread::OscThread(SocketReceiveMultiplexer* multiplexer, QObject* parent)
    : QThread(parent),
      multiplexer(multiplexer)
{
}

void OscThread::run()
{
    // The listeners catch a malformed packet at ProcessPacket, which is where it
    // is thrown today. This is the backstop: an exception that escapes Run() would
    // otherwise leave QThread::run() and call std::terminate, so any future throw
    // on this thread costs OSC input rather than the whole client.
    try
    {
        this->multiplexer->Run();
    }
    catch (const std::exception& e)
    {
        qDebug("OSC thread stopped on an unhandled exception: %s", e.what());
    }
    catch (...)
    {
        qDebug("OSC thread stopped on an unknown exception");
    }
}

void OscThread::stop()
{
    this->multiplexer->AsynchronousBreak();
}
