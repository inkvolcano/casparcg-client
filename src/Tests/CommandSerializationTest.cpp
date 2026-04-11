#include <QtTest>
#include "Commands/PlayoutCommand.h"

#include <QtCore/QBuffer>
#include <QtCore/QXmlStreamWriter>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <sstream>

class CommandSerializationTest : public QObject
{
    Q_OBJECT

private:
    // Helper: serialize a command to XML string.
    QString writeToXml(AbstractCommand& cmd)
    {
        QString xml;
        QXmlStreamWriter writer(&xml);
        writer.writeStartDocument();
        writer.writeStartElement("command");
        cmd.writeProperties(writer);
        writer.writeEndElement();
        writer.writeEndDocument();
        return xml;
    }

    // Helper: parse XML string into a boost ptree for readProperties.
    boost::property_tree::wptree parseXml(const QString& xml)
    {
        std::wstring wxml = xml.toStdWString();
        std::wistringstream stream(wxml);
        boost::property_tree::wptree pt;
        boost::property_tree::xml_parser::read_xml(stream, pt);
        return pt.get_child(L"command");
    }

private slots:
    void defaultValues()
    {
        PlayoutCommand cmd;
        QCOMPARE(cmd.getChannel(), Output::DEFAULT_CHANNEL);
        QCOMPARE(cmd.getVideolayer(), Output::DEFAULT_VIDEOLAYER);
        QCOMPARE(cmd.getDelay(), Output::DEFAULT_DELAY);
        QCOMPARE(cmd.getDuration(), Output::DEFAULT_DURATION);
        QCOMPARE(cmd.getTriggerBank(), 0);
        QCOMPARE(cmd.getCloneGroupId(), QString(""));
    }

    void xmlRoundtrip()
    {
        PlayoutCommand cmd;
        cmd.setChannel(3);
        cmd.setVideolayer(15);
        cmd.setDelay(500);
        cmd.setDuration(2000);
        cmd.setAllowGpi(true);
        cmd.setAllowRemoteTriggering(true);
        cmd.setRemoteTriggerId("remote-1");
        cmd.setTriggerBank(5);

        QString xml = writeToXml(cmd);

        PlayoutCommand restored;
        auto pt = parseXml(xml);
        restored.readProperties(pt);

        QCOMPARE(restored.getChannel(), 3);
        QCOMPARE(restored.getVideolayer(), 15);
        QCOMPARE(restored.getDelay(), 500);
        QCOMPARE(restored.getDuration(), 2000);
        QCOMPARE(restored.getAllowGpi(), true);
        QCOMPARE(restored.getAllowRemoteTriggering(), true);
        QCOMPARE(restored.getRemoteTriggerId(), QString("remote-1"));
        QCOMPARE(restored.getTriggerBank(), 5);
    }

    void channelOverride()
    {
        PlayoutCommand cmd;
        cmd.setChannel(2);
        QCOMPARE(cmd.getChannel(), 2);

        cmd.setChannelOverride(7);
        QCOMPARE(cmd.getChannel(), 7);

        cmd.clearChannelOverride();
        QCOMPARE(cmd.getChannel(), 2);
    }

    void propertyChangedSignal()
    {
        PlayoutCommand cmd;
        QSignalSpy spy(&cmd, &AbstractCommand::propertyChanged);

        cmd.setChannel(5);
        QVERIFY(spy.count() >= 1);
    }

    void triggerBankRoundtrip()
    {
        PlayoutCommand cmd;
        cmd.setTriggerBank(7);

        QString xml = writeToXml(cmd);
        PlayoutCommand restored;
        auto pt = parseXml(xml);
        restored.readProperties(pt);

        QCOMPARE(restored.getTriggerBank(), 7);
    }
};

int runCommandSerializationTest(int argc, char* argv[])
{
    CommandSerializationTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "CommandSerializationTest.moc"
