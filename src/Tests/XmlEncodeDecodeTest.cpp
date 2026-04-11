#include <QtTest>
#include "Xml.h"

class XmlEncodeDecodeTest : public QObject
{
    Q_OBJECT

private slots:
    void encodeAmpersand()
    {
        QCOMPARE(Xml::encode("a&b"), QString("a&amp;b"));
    }

    void encodeLessThan()
    {
        QCOMPARE(Xml::encode("a<b"), QString("a&lt;b"));
    }

    void encodeGreaterThan()
    {
        QCOMPARE(Xml::encode("a>b"), QString("a&gt;b"));
    }

    void encodeQuote()
    {
        QCOMPARE(Xml::encode("a\"b"), QString("a&quot;b"));
    }

    void encodeApostrophe()
    {
        QCOMPARE(Xml::encode("a'b"), QString("a&apos;b"));
    }

    void encodeNewline()
    {
        QCOMPARE(Xml::encode("a\nb"), QString("a&#10;b"));
    }

    void encodeCarriageReturn()
    {
        QCOMPARE(Xml::encode("a\rb"), QString("a&#13;b"));
    }

    void encodeTab()
    {
        QCOMPARE(Xml::encode("a\tb"), QString("a&#9;b"));
    }

    void roundtripSimple()
    {
        QString original = "Hello World";
        QCOMPARE(Xml::decode(Xml::encode(original)), original);
    }

    void roundtripSpecialChars()
    {
        QString original = "a&b<c>d\"e'f\ng\rh\ti";
        QCOMPARE(Xml::decode(Xml::encode(original)), original);
    }

    void emptyString()
    {
        QCOMPARE(Xml::encode(""), QString(""));
        QCOMPARE(Xml::decode(""), QString(""));
    }

    void noSpecialChars()
    {
        QString plain = "Hello World 123";
        QCOMPARE(Xml::encode(plain), plain);
        QCOMPARE(Xml::decode(plain), plain);
    }
};

int runXmlEncodeDecodeTest(int argc, char* argv[])
{
    XmlEncodeDecodeTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "XmlEncodeDecodeTest.moc"
