#include <QtTest>
#include "Models/KeyValueModel.h"

class KeyValueModelTest : public QObject
{
    Q_OBJECT

private slots:
    void constructionDefaults()
    {
        KeyValueModel model("key1", "value1");
        QCOMPARE(model.getKey(), QString("key1"));
        QCOMPARE(model.getValue(), QString("value1"));
        QCOMPARE(model.getMode(), 0);
        QCOMPARE(model.getCycleValues(), QString(""));
    }

    void constructionAllParams()
    {
        KeyValueModel model("key2", "value2", 2, "a,b,c");
        QCOMPARE(model.getKey(), QString("key2"));
        QCOMPARE(model.getValue(), QString("value2"));
        QCOMPARE(model.getMode(), 2);
        QCOMPARE(model.getCycleValues(), QString("a,b,c"));
    }

    void setters()
    {
        KeyValueModel model("k", "v");
        model.setKey("newKey");
        model.setValue("newValue");
        model.setMode(3);
        model.setCycleValues("x,y");

        QCOMPARE(model.getKey(), QString("newKey"));
        QCOMPARE(model.getValue(), QString("newValue"));
        QCOMPARE(model.getMode(), 3);
        QCOMPARE(model.getCycleValues(), QString("x,y"));
    }
};

int runKeyValueModelTest(int argc, char* argv[])
{
    KeyValueModelTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "KeyValueModelTest.moc"
