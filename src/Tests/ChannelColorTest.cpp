#include <QtTest>
#include "Global.h"

class ChannelColorTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        // Reset to defaults before each test.
        ChannelColor::setAngle(ChannelColor::DEFAULT_ANGLE);
        ChannelColor::setOffset(ChannelColor::DEFAULT_OFFSET);
        ChannelColor::setSaturation(ChannelColor::DEFAULT_SATURATION);
        ChannelColor::setLightness(ChannelColor::DEFAULT_LIGHTNESS);
    }

    void hueInRange()
    {
        for (int ch = 0; ch < 20; ch++)
        {
            double h = ChannelColor::hue(ch);
            QVERIFY2(h >= 0.0 && h < 360.0,
                      qPrintable(QString("hue(%1) = %2 out of range").arg(ch).arg(h)));
        }
    }

    void hueDistinctForFirstChannels()
    {
        double h0 = ChannelColor::hue(0);
        double h1 = ChannelColor::hue(1);
        double h2 = ChannelColor::hue(2);

        QVERIFY(qAbs(h0 - h1) > 1.0);
        QVERIFY(qAbs(h1 - h2) > 1.0);
        QVERIFY(qAbs(h0 - h2) > 1.0);
    }

    void customAngleChangesOutput()
    {
        double defaultHue1 = ChannelColor::hue(1);
        ChannelColor::setAngle(90.0);
        double customHue1 = ChannelColor::hue(1);
        QVERIFY(qAbs(defaultHue1 - customHue1) > 1.0);
    }

    void customOffsetChangesOutput()
    {
        double defaultHue0 = ChannelColor::hue(0);
        ChannelColor::setOffset(0.0);
        double customHue0 = ChannelColor::hue(0);
        QVERIFY(qAbs(defaultHue0 - customHue0) > 1.0);
    }

    void activeSaturationCapped()
    {
        ChannelColor::setSaturation(0.95);
        QVERIFY(ChannelColor::activeSaturation() <= 1.0);
    }

    void activeLightnessCapped()
    {
        ChannelColor::setLightness(0.45);
        QVERIFY(ChannelColor::activeLightness() <= 0.50);
    }

    void gettersMatchSetters()
    {
        ChannelColor::setAngle(100.0);
        ChannelColor::setOffset(50.0);
        ChannelColor::setSaturation(0.5);
        ChannelColor::setLightness(0.2);

        QCOMPARE(ChannelColor::angle(), 100.0);
        QCOMPARE(ChannelColor::offset(), 50.0);
        QCOMPARE(ChannelColor::saturation(), 0.5);
        QCOMPARE(ChannelColor::lightness(), 0.2);
    }
};

int runChannelColorTest(int argc, char* argv[])
{
    ChannelColorTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "ChannelColorTest.moc"
