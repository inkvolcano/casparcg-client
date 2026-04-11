#include <QtTest>
#include "CloneGroupRegistry.h"
#include "Commands/PlayoutCommand.h"

class CloneGroupRegistryTest : public QObject
{
    Q_OBJECT

private slots:
    void registerAndSync()
    {
        CloneGroupRegistry registry;
        PlayoutCommand cmd1;
        PlayoutCommand cmd2;

        registry.registerCommand("group1", &cmd1);
        registry.registerCommand("group1", &cmd2);

        cmd1.setChannel(5);
        registry.syncFromSource("group1", &cmd1);

        // cmd2 should now have channel 5 from sync.
        QCOMPARE(cmd2.getChannel(), 5);

        registry.unregisterCommand(&cmd1);
        registry.unregisterCommand(&cmd2);
    }

    void syncDoesNotAffectSource()
    {
        CloneGroupRegistry registry;
        PlayoutCommand cmd1;
        PlayoutCommand cmd2;

        registry.registerCommand("group1", &cmd1);
        registry.registerCommand("group1", &cmd2);

        cmd1.setChannel(3);
        cmd1.setVideolayer(7);
        registry.syncFromSource("group1", &cmd1);

        // Source should be unchanged.
        QCOMPARE(cmd1.getChannel(), 3);
        QCOMPARE(cmd1.getVideolayer(), 7);
        // Sibling should be synced.
        QCOMPARE(cmd2.getChannel(), 3);
        QCOMPARE(cmd2.getVideolayer(), 7);

        registry.unregisterCommand(&cmd1);
        registry.unregisterCommand(&cmd2);
    }

    void emptyGroupIdDoesNothing()
    {
        CloneGroupRegistry registry;
        PlayoutCommand cmd;
        registry.registerCommand("", &cmd);
        // Should not crash, and isSyncing should be false.
        QVERIFY(!registry.isSyncing());
    }

    void isSyncingGuard()
    {
        CloneGroupRegistry registry;
        QVERIFY(!registry.isSyncing());
    }

    void unregisterLastTwoClearsCloneGroupId()
    {
        CloneGroupRegistry registry;
        PlayoutCommand cmd1;
        PlayoutCommand cmd2;

        cmd1.setCloneGroupId("grp");
        cmd2.setCloneGroupId("grp");
        registry.registerCommand("grp", &cmd1);
        registry.registerCommand("grp", &cmd2);

        // Unregister one — group shrinks to 1 member, should auto-dissolve.
        registry.unregisterCommand(&cmd1);

        // The remaining member's cloneGroupId should be cleared.
        QCOMPARE(cmd2.getCloneGroupId(), QString(""));
    }
};

int runCloneGroupRegistryTest(int argc, char* argv[])
{
    CloneGroupRegistryTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "CloneGroupRegistryTest.moc"
