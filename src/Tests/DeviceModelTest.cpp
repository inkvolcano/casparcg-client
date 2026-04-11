#include <QtTest>
#include "Models/DeviceModel.h"

class DeviceModelTest : public QObject
{
    Q_OBJECT

private slots:
    void constructionWithDefaults()
    {
        DeviceModel model(1, "Server1", "192.168.1.1", 5250, "user", "pass",
                          "desc", "2.3", "shadow", 4, "PAL,PAL", 0, 0);
        QCOMPARE(model.getId(), 1);
        QCOMPARE(model.getName(), QString("Server1"));
        QCOMPARE(model.getAddress(), QString("192.168.1.1"));
        QCOMPARE(model.getPort(), 5250);
        QCOMPARE(model.getChannels(), 4);
        QCOMPARE(model.getTemplatePath(), QString(""));
        QCOMPARE(model.getMediaPath(), QString(""));
    }

    void constructionWithNewFields()
    {
        DeviceModel model(2, "Server2", "10.0.0.1", 5250, "", "", "", "", "",
                          2, "HD", 1, 0, "/templates", "/media");
        QCOMPARE(model.getTemplatePath(), QString("/templates"));
        QCOMPARE(model.getMediaPath(), QString("/media"));
    }

    void allGetters()
    {
        DeviceModel model(3, "name", "addr", 1234, "usr", "pwd", "desc",
                          "ver", "shd", 8, "fmts", 2, 1, "tpl", "med");
        QCOMPARE(model.getUsername(), QString("usr"));
        QCOMPARE(model.getPassword(), QString("pwd"));
        QCOMPARE(model.getDescription(), QString("desc"));
        QCOMPARE(model.getVersion(), QString("ver"));
        QCOMPARE(model.getShadow(), QString("shd"));
        QCOMPARE(model.getChannelFormats(), QString("fmts"));
        QCOMPARE(model.getPreviewChannel(), 2);
        QCOMPARE(model.getLockedChannel(), 1);
    }
};

int runDeviceModelTest(int argc, char* argv[])
{
    DeviceModelTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "DeviceModelTest.moc"
