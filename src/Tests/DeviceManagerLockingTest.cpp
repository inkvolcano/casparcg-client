#include <QtTest>
#include "DeviceManager.h"

class DeviceManagerLockingTest : public QObject
{
    Q_OBJECT

private slots:
    void toggleChannelLock()
    {
        DeviceManager& dm = DeviceManager::getInstance();

        // Ensure clean state.
        if (dm.isChannelLocked("TestDevice", 1))
            dm.toggleChannelLock("TestDevice", 1);

        QCOMPARE(dm.isChannelLocked("TestDevice", 1), false);
        dm.toggleChannelLock("TestDevice", 1);
        QCOMPARE(dm.isChannelLocked("TestDevice", 1), true);
        dm.toggleChannelLock("TestDevice", 1);
        QCOMPARE(dm.isChannelLocked("TestDevice", 1), false);
    }

    void globalChannelLock()
    {
        DeviceManager& dm = DeviceManager::getInstance();

        // Ensure clean state.
        if (dm.getGlobalLockedChannels().contains(2))
            dm.toggleGlobalChannelLock(2);

        QCOMPARE(dm.isChannelLocked("AnyDevice", 2), false);
        dm.toggleGlobalChannelLock(2);
        // Global lock should make any device's channel locked.
        QCOMPARE(dm.isChannelLocked("AnyDevice", 2), true);
        dm.toggleGlobalChannelLock(2);
        QCOMPARE(dm.isChannelLocked("AnyDevice", 2), false);
    }

    void getLockedChannels()
    {
        DeviceManager& dm = DeviceManager::getInstance();

        // Clean state.
        for (int ch : dm.getLockedChannels("LockTest").values())
            dm.toggleChannelLock("LockTest", ch);

        dm.toggleChannelLock("LockTest", 3);
        dm.toggleChannelLock("LockTest", 5);

        QSet<int> locked = dm.getLockedChannels("LockTest");
        QVERIFY(locked.contains(3));
        QVERIFY(locked.contains(5));
        QCOMPARE(locked.size(), 2);

        // Cleanup.
        dm.toggleChannelLock("LockTest", 3);
        dm.toggleChannelLock("LockTest", 5);
    }

    void channelLockChangedSignal()
    {
        DeviceManager& dm = DeviceManager::getInstance();

        // Ensure clean.
        if (dm.isChannelLocked("SigTest", 1))
            dm.toggleChannelLock("SigTest", 1);

        QSignalSpy spy(&dm, &DeviceManager::channelLockChanged);
        dm.toggleChannelLock("SigTest", 1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("SigTest"));
        QCOMPARE(spy.at(0).at(1).toInt(), 1);
        QCOMPARE(spy.at(0).at(2).toBool(), true);

        // Cleanup.
        dm.toggleChannelLock("SigTest", 1);
    }
};

int runDeviceManagerLockingTest(int argc, char* argv[])
{
    DeviceManagerLockingTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "DeviceManagerLockingTest.moc"
