#include <QtTest>
#include "OscSubscriptionRegistry.h"
#include "OscSubscription.h"

class OscSubscriptionRegistryTest : public QObject
{
    Q_OBJECT

private slots:
    void exactMatchDispatch()
    {
        OscSubscriptionRegistry registry;
        OscSubscription sub("/channel/1/time", this);
        registry.subscribe("/channel/1/time", &sub);

        QSignalSpy spy(&sub, &OscSubscription::subscriptionReceived);
        registry.dispatch("/channel/1/time", { QVariant(42) });

        QCOMPARE(spy.count(), 1);
        registry.unsubscribe(&sub);
    }

    void suffixMatchWithIpPrefix()
    {
        OscSubscriptionRegistry registry;
        OscSubscription sub("/channel/1/time", this);
        registry.subscribe("/channel/1/time", &sub);

        QSignalSpy spy(&sub, &OscSubscription::subscriptionReceived);
        registry.dispatch("192.168.1.5/channel/1/time", { QVariant(100) });

        QCOMPARE(spy.count(), 1);
        registry.unsubscribe(&sub);
    }

    void noMatchNoNotification()
    {
        OscSubscriptionRegistry registry;
        OscSubscription sub("/channel/1/time", this);
        registry.subscribe("/channel/1/time", &sub);

        QSignalSpy spy(&sub, &OscSubscription::subscriptionReceived);
        registry.dispatch("/channel/2/time", {});

        QCOMPARE(spy.count(), 0);
        registry.unsubscribe(&sub);
    }

    void unsubscribeStopsNotification()
    {
        OscSubscriptionRegistry registry;
        OscSubscription sub("/test/path", this);
        registry.subscribe("/test/path", &sub);
        registry.unsubscribe(&sub);

        QSignalSpy spy(&sub, &OscSubscription::subscriptionReceived);
        registry.dispatch("/test/path", {});

        QCOMPARE(spy.count(), 0);
    }

    void multipleSubscribersSamePath()
    {
        OscSubscriptionRegistry registry;
        OscSubscription sub1("/path", this);
        OscSubscription sub2("/path", this);
        registry.subscribe("/path", &sub1);
        registry.subscribe("/path", &sub2);

        QSignalSpy spy1(&sub1, &OscSubscription::subscriptionReceived);
        QSignalSpy spy2(&sub2, &OscSubscription::subscriptionReceived);
        registry.dispatch("/path", {});

        QCOMPARE(spy1.count(), 1);
        QCOMPARE(spy2.count(), 1);
        registry.unsubscribe(&sub1);
        registry.unsubscribe(&sub2);
    }

    void dispatchNonExistentPathNoCrash()
    {
        OscSubscriptionRegistry registry;
        registry.dispatch("/nonexistent", { QVariant("data") });
        // Should not crash.
    }

    void pathWithoutSlashNoFalseSuffixMatch()
    {
        OscSubscriptionRegistry registry;
        OscSubscription sub("/test", this);
        registry.subscribe("/test", &sub);

        QSignalSpy spy(&sub, &OscSubscription::subscriptionReceived);
        // eventPath "noSlashPrefix" has no slash — should not trigger suffix match.
        registry.dispatch("noSlashPrefix", {});

        QCOMPARE(spy.count(), 0);
        registry.unsubscribe(&sub);
    }
};

int runOscSubscriptionRegistryTest(int argc, char* argv[])
{
    OscSubscriptionRegistryTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "OscSubscriptionRegistryTest.moc"
