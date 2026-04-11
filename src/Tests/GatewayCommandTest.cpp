#include <QtTest>
#include "Commands/GatewayCommand.h"

#include <QtCore/QXmlStreamWriter>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <sstream>

class GatewayCommandTest : public QObject
{
    Q_OBJECT

private:
    QString writeToXml(GatewayCommand& cmd)
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
        GatewayCommand cmd;
        QCOMPARE(cmd.getGatewayId(), QString(""));
        QCOMPARE(cmd.getIsExit(), false);
        QCOMPARE(cmd.getExitLabel(), QString("Exit A"));
        QCOMPARE(cmd.getSelectedExitLabel(), QString("Exit A"));
        QCOMPARE(cmd.getConditionEnabled(), false);
        QCOMPARE(cmd.getConditionOperator(), 0);
        QCOMPARE(cmd.getConditionHour(), 0);
        QCOMPARE(cmd.getConditionMinute(), 0);
        QCOMPARE(cmd.getConditionSecond(), 0);
    }

    void xmlRoundtrip()
    {
        GatewayCommand cmd;
        cmd.setGatewayId("gw-123");
        cmd.setIsExit(true);
        cmd.setExitLabel("Exit B");
        cmd.setSelectedExitLabel("Exit B");
        cmd.setConditionEnabled(true);
        cmd.setConditionOperator(1);
        cmd.setConditionHour(14);
        cmd.setConditionMinute(30);
        cmd.setConditionSecond(45);
        cmd.setConditionExitLabel("Exit C");

        QString xml = writeToXml(cmd);

        GatewayCommand restored;
        auto pt = parseXml(xml);
        restored.readProperties(pt);

        QCOMPARE(restored.getGatewayId(), QString("gw-123"));
        QCOMPARE(restored.getIsExit(), true);
        QCOMPARE(restored.getExitLabel(), QString("Exit B"));
        QCOMPARE(restored.getSelectedExitLabel(), QString("Exit B"));
        QCOMPARE(restored.getConditionEnabled(), true);
        QCOMPARE(restored.getConditionOperator(), 1);
        QCOMPARE(restored.getConditionHour(), 14);
        QCOMPARE(restored.getConditionMinute(), 30);
        QCOMPARE(restored.getConditionSecond(), 45);
        QCOMPARE(restored.getConditionExitLabel(), QString("Exit C"));
    }

    void effectiveExitLabelConditionDisabled()
    {
        GatewayCommand cmd;
        cmd.setSelectedExitLabel("Exit A");
        cmd.setConditionEnabled(false);
        cmd.setConditionExitLabel("Exit B");
        QCOMPARE(cmd.getEffectiveExitLabel(), QString("Exit A"));
    }

    void timeClampingHour()
    {
        GatewayCommand cmd;
        cmd.setConditionHour(25);
        QCOMPARE(cmd.getConditionHour(), 23);
    }

    void timeClampingMinute()
    {
        GatewayCommand cmd;
        cmd.setConditionMinute(61);
        QCOMPARE(cmd.getConditionMinute(), 59);
    }

    void timeClampingSecond()
    {
        GatewayCommand cmd;
        cmd.setConditionSecond(99);
        QCOMPARE(cmd.getConditionSecond(), 59);
    }

    void gatewayIdChangedSignal()
    {
        GatewayCommand cmd;
        QSignalSpy spy(&cmd, &GatewayCommand::gatewayIdChanged);
        cmd.setGatewayId("test-id");
        QCOMPARE(spy.count(), 1);
    }
};

int runGatewayCommandTest(int argc, char* argv[])
{
    GatewayCommandTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "GatewayCommandTest.moc"
