#include <QtTest>
#include "TriggerBankRegistry.h"

class TriggerBankRegistryTest : public QObject
{
    Q_OBJECT

private slots:
    void assignAndRetrieve()
    {
        TriggerBankRegistry registry;
        QTreeWidgetItem item;
        registry.assign(1, &item);
        QCOMPARE(registry.getItem(1), &item);
        QCOMPARE(registry.getBankForItem(&item), 1);
    }

    void reassignSameItemToDifferentBank()
    {
        TriggerBankRegistry registry;
        QTreeWidgetItem item;
        registry.assign(1, &item);
        registry.assign(3, &item);
        // Old bank should be cleared.
        QCOMPARE(registry.getItem(1), nullptr);
        QCOMPARE(registry.getItem(3), &item);
        QCOMPARE(registry.getBankForItem(&item), 3);
    }

    void assignDifferentItemToOccupiedBank()
    {
        TriggerBankRegistry registry;
        QTreeWidgetItem item1, item2;
        registry.assign(2, &item1);
        registry.assign(2, &item2);
        QCOMPARE(registry.getItem(2), &item2);
        QCOMPARE(registry.getBankForItem(&item1), 0);
        QCOMPARE(registry.getBankForItem(&item2), 2);
    }

    void boundaryBankIdZeroRejected()
    {
        TriggerBankRegistry registry;
        QTreeWidgetItem item;
        registry.assign(0, &item);
        QCOMPARE(registry.getItem(0), nullptr);
        QCOMPARE(registry.getBankForItem(&item), 0);
    }

    void boundaryBankIdTenRejected()
    {
        TriggerBankRegistry registry;
        QTreeWidgetItem item;
        registry.assign(10, &item);
        QCOMPARE(registry.getItem(10), nullptr);
    }

    void unassign()
    {
        TriggerBankRegistry registry;
        QTreeWidgetItem item;
        registry.assign(5, &item);
        registry.unassign(5);
        QCOMPARE(registry.getItem(5), nullptr);
        QCOMPARE(registry.getBankForItem(&item), 0);
    }

    void unassignByItem()
    {
        TriggerBankRegistry registry;
        QTreeWidgetItem item;
        registry.assign(4, &item);
        registry.unassignByItem(&item);
        QCOMPARE(registry.getItem(4), nullptr);
    }

    void clearAll()
    {
        TriggerBankRegistry registry;
        QTreeWidgetItem item1, item2;
        registry.assign(1, &item1);
        registry.assign(2, &item2);
        registry.clear();
        QCOMPARE(registry.getItem(1), nullptr);
        QCOMPARE(registry.getItem(2), nullptr);
    }

    void getBankForUnknownItemReturnsZero()
    {
        TriggerBankRegistry registry;
        QTreeWidgetItem item;
        QCOMPARE(registry.getBankForItem(&item), 0);
    }

    void fireBankTriggeredEmitsSignal()
    {
        TriggerBankRegistry registry;
        QSignalSpy spy(&registry, &TriggerBankRegistry::bankTriggered);
        registry.fireBankTriggered(3);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 3);
    }
};

int runTriggerBankRegistryTest(int argc, char* argv[])
{
    TriggerBankRegistryTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "TriggerBankRegistryTest.moc"
