#include <QtTest>
#include "Commands/TemplateCommand.h"

#include <QtCore/QXmlStreamWriter>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <sstream>

class TemplateCommandTest : public QObject
{
    Q_OBJECT

private:
    QString writeToXml(TemplateCommand& cmd)
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
        TemplateCommand cmd;
        QCOMPARE(cmd.getFlashlayer(), Template::DEFAULT_FLASHLAYER);
        QCOMPARE(cmd.getInvokeCount(), 1);
        QCOMPARE(cmd.getInvokeAt(0), QString(Template::DEFAULT_INVOKE));
        QCOMPARE(cmd.getUseStoredData(), Template::DEFAULT_USE_STORED_DATA);
        QCOMPARE(cmd.getSendAsJson(), Template::DEFAULT_SEND_AS_JSON);
        QCOMPARE(cmd.getNewlineBehavior(), Template::DEFAULT_NEWLINE_BEHAVIOR);
    }

    void invokesListRoundtrip()
    {
        TemplateCommand cmd;
        cmd.setInvokes({"play", "stop", "next"});
        QCOMPARE(cmd.getInvokeCount(), 3);
        QCOMPARE(cmd.getInvokeAt(0), QString("play"));
        QCOMPARE(cmd.getInvokeAt(1), QString("stop"));
        QCOMPARE(cmd.getInvokeAt(2), QString("next"));

        QString xml = writeToXml(cmd);

        TemplateCommand restored;
        auto pt = parseXml(xml);
        restored.readProperties(pt);

        QCOMPARE(restored.getInvokeCount(), 3);
        QCOMPARE(restored.getInvokeAt(0), QString("play"));
        QCOMPARE(restored.getInvokeAt(1), QString("stop"));
        QCOMPARE(restored.getInvokeAt(2), QString("next"));
    }

    void xmlRoundtripAllProperties()
    {
        TemplateCommand cmd;
        cmd.setFlashlayer(5);
        cmd.setUseStoredData(true);
        cmd.setSendAsJson(true);
        cmd.setUseUppercaseData(true);
        cmd.setTriggerOnNext(true);
        cmd.setNewlineBehavior(2);
        cmd.setInvokeHotkeyIndex(1);

        QString xml = writeToXml(cmd);

        TemplateCommand restored;
        auto pt = parseXml(xml);
        restored.readProperties(pt);

        QCOMPARE(restored.getFlashlayer(), 5);
        QCOMPARE(restored.getUseStoredData(), true);
        QCOMPARE(restored.getSendAsJson(), true);
        QCOMPARE(restored.getUseUppercaseData(), true);
        QCOMPARE(restored.getTriggerOnNext(), true);
        QCOMPARE(restored.getNewlineBehavior(), 2);
        QCOMPARE(restored.getInvokeHotkeyIndex(), 1);
    }

    void newlineBehaviorSetting()
    {
        TemplateCommand cmd;

        cmd.setNewlineBehavior(0);
        QCOMPARE(cmd.getNewlineBehavior(), 0);

        cmd.setNewlineBehavior(1);
        QCOMPARE(cmd.getNewlineBehavior(), 1);

        cmd.setNewlineBehavior(2);
        QCOMPARE(cmd.getNewlineBehavior(), 2);
    }

    void pendingInvokeOverride()
    {
        TemplateCommand cmd;
        cmd.setPendingInvokeOverride("custom_invoke");
        QCOMPARE(cmd.takePendingInvokeOverride(), QString("custom_invoke"));
        // Second take should return empty (consumed).
        QCOMPARE(cmd.takePendingInvokeOverride(), QString(""));
    }

    void templateDataModels()
    {
        TemplateCommand cmd;
        QList<KeyValueModel> models;
        models.append(KeyValueModel("f0", "Hello"));
        models.append(KeyValueModel("f1", "World", 1, "a,b,c"));
        cmd.setTemplateDataModels(models);

        QCOMPARE(cmd.getTemplateDataModels().size(), 2);
        QCOMPARE(cmd.getTemplateDataModels().at(0).getKey(), QString("f0"));
        QCOMPARE(cmd.getTemplateDataModels().at(1).getMode(), 1);
        QCOMPARE(cmd.getTemplateDataModels().at(1).getCycleValues(), QString("a,b,c"));
    }
};

int runTemplateCommandTest(int argc, char* argv[])
{
    TemplateCommandTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "TemplateCommandTest.moc"
