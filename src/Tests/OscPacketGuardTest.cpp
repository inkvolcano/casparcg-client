#include <QtTest>

#include "OscControlListener.h"
#include "OscMonitorListener.h"
#include "OscSubscription.h"
#include "OscSubscriptionRegistry.h"

#include <ip/IpEndpointName.h>

// One malformed UDP datagram used to end the client.
//
// oscpack builds ReceivedPacket and ReceivedMessage inside ProcessPacket, and
// those constructors throw on anything malformed - a length not divisible by four
// is enough, and so is an empty one. Nothing on the way up caught it: not the
// receive loop in UdpSocket.cpp, not OscThread::run(), and an exception leaving
// QThread::run() is std::terminate.
//
// Both listeners bind 0.0.0.0 and are on by default, on 3250 and 6250. So a single
// stray byte from anything on the venue network ended the process - a UDP port
// scan would do it, and so would a truncated frame from the CasparCG server
// itself, since 6250 is where its own OSC stream arrives.
//
// These call ProcessPacket through the oscpack base, which is exactly what the
// receive loop does. No socket is opened and nothing is sent: the whole point is
// that a bad parse cannot travel past the listener.
class OscPacketGuardTest : public QObject
{
    Q_OBJECT

private:
    // A well formed OSC message: address, padded to four; a type tag string,
    // padded to four; then the argument, big endian.
    static QByteArray oscMessage(const char* address, qint32 value)
    {
        QByteArray packet(address);
        packet.append('\0');
        while (packet.size() % 4 != 0)
            packet.append('\0');

        QByteArray tags(",i");
        tags.append('\0');
        while (tags.size() % 4 != 0)
            tags.append('\0');

        packet.append(tags);

        for (int shift = 24; shift >= 0; shift -= 8)
            packet.append(static_cast<char>((value >> shift) & 0xff));

        return packet;
    }

    // Through the base, because that is the call the receive loop makes and the
    // override is what has to catch. If it does not, this test does not fail - the
    // whole runner is terminated, which is the bug.
    static void feed(osc::OscPacketListener& listener, const QByteArray& bytes)
    {
        IpEndpointName from("127.0.0.1", 3250);
        listener.ProcessPacket(bytes.constData(), bytes.size(), from);
    }

private slots:
    void aSingleByteDoesNotEndTheControlListener()
    {
        OscControlListener listener;

        // "message size must be multiple of four" - the cheapest throw there is.
        feed(listener, QByteArray("x"));

        QVERIFY(true); // reaching this line is the assertion
    }

    void anEmptyDatagramDoesNotEndTheControlListener()
    {
        OscControlListener listener;

        feed(listener, QByteArray());

        QVERIFY(true);
    }

    void junkOfALegalLengthDoesNotEndTheControlListener()
    {
        OscControlListener listener;

        // A multiple of four, so it gets past the size check and dies further in:
        // no leading slash, no type tags, nothing terminated.
        feed(listener, QByteArray("\xff\xfe\xfd\xfc", 4));
        feed(listener, QByteArray("/nope", 5));
        feed(listener, QByteArray("#bundle", 7));

        QVERIFY(true);
    }

    void theMonitorListenerIsGuardedToo()
    {
        // 6250 takes the server's own stream, so this one is not only exposed to
        // strangers - a truncated frame from the server reaches it.
        OscMonitorListener listener;

        feed(listener, QByteArray("x"));
        feed(listener, QByteArray());
        feed(listener, QByteArray("\xff\xfe\xfd\xfc", 4));

        QVERIFY(true);
    }

    void aValidMessageStillArrives()
    {
        // The guard must drop malformed packets, not swallow real ones. Without
        // this, catching everything and doing nothing would pass every test above.
        OscControlListener listener;

        OscSubscription subscription("/control/abc/play", this);
        OscSubscriptionRegistry::getInstance().subscribe("/control/abc/play", &subscription);

        QSignalSpy spy(&subscription, &OscSubscription::subscriptionReceived);

        feed(listener, oscMessage("/control/abc/play", 1));

        QCOMPARE(spy.count(), 1);

        OscSubscriptionRegistry::getInstance().unsubscribe(&subscription);
    }

    void aValidMessageAfterABadOneStillArrives()
    {
        // A dropped packet must not leave the listener wedged for the next one.
        OscControlListener listener;

        OscSubscription subscription("/control/abc/stop", this);
        OscSubscriptionRegistry::getInstance().subscribe("/control/abc/stop", &subscription);

        QSignalSpy spy(&subscription, &OscSubscription::subscriptionReceived);

        feed(listener, QByteArray("x"));
        feed(listener, oscMessage("/control/abc/stop", 1));

        QCOMPARE(spy.count(), 1);

        OscSubscriptionRegistry::getInstance().unsubscribe(&subscription);
    }
};

int runOscPacketGuardTest(int argc, char* argv[])
{
    OscPacketGuardTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "OscPacketGuardTest.moc"
