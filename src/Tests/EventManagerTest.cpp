#include <QtTest>
#include "EventManager.h"

class EventManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void previewModeToggle()
    {
        EventManager& em = EventManager::getInstance();
        em.setPreviewMode(false);
        QCOMPARE(em.getPreviewMode(), false);

        QSignalSpy spy(&em, &EventManager::previewModeChanged);
        em.setPreviewMode(true);
        QCOMPARE(em.getPreviewMode(), true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void previewModeNoSignalOnSameValue()
    {
        EventManager& em = EventManager::getInstance();
        em.setPreviewMode(false);

        QSignalSpy spy(&em, &EventManager::previewModeChanged);
        em.setPreviewMode(false);
        QCOMPARE(spy.count(), 0);
    }

    void autostepModeToggle()
    {
        EventManager& em = EventManager::getInstance();
        em.setAutostepMode(false);

        QSignalSpy spy(&em, &EventManager::autostepModeChanged);
        em.setAutostepMode(true);
        QCOMPARE(em.getAutostepMode(), true);
        QCOMPARE(spy.count(), 1);
    }

    void bigBoldModeToggle()
    {
        EventManager& em = EventManager::getInstance();
        em.setBigBoldMode(false);

        QSignalSpy spy(&em, &EventManager::bigBoldModeChanged);
        em.setBigBoldMode(true);
        QCOMPARE(em.getBigBoldMode(), true);
        QCOMPARE(spy.count(), 1);
    }

    void undoLimitChangedSignal()
    {
        EventManager& em = EventManager::getInstance();

        QSignalSpy spy(&em, &EventManager::undoLimitChanged);
        em.fireUndoLimitChangedEvent(25);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 25);
    }

    void gatewayLabelProvider()
    {
        EventManager& em = EventManager::getInstance();

        em.setGatewayExitLabelProvider([](const QString& gatewayId) -> QStringList {
            if (gatewayId == "gw1") return { "Exit A", "Exit B" };
            return {};
        });

        QCOMPARE(em.getGatewayExitLabels("gw1"), QStringList({"Exit A", "Exit B"}));
        QCOMPARE(em.getGatewayExitLabels("unknown"), QStringList());
    }

    void nullGatewayProviderReturnsEmpty()
    {
        EventManager& em = EventManager::getInstance();
        em.setGatewayExitLabelProvider(nullptr);
        QCOMPARE(em.getGatewayExitLabels("anything"), QStringList());
    }
};

int runEventManagerTest(int argc, char* argv[])
{
    EventManagerTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "EventManagerTest.moc"
