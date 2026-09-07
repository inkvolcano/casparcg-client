#include <QtTest>
#include "Commands/ShellCommand.h"

#include <QtCore/QProcess>
#include <QtCore/QXmlStreamWriter>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <sstream>

class ShellCommandTest : public QObject
{
    Q_OBJECT

private:
    QString writeToXml(ShellCommand& cmd)
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
    void defaultsAreTheCautiousOnes()
    {
        ShellCommand cmd;

        QVERIFY(cmd.getCommandLine().isEmpty());
        QVERIFY(cmd.getWorkingDirectory().isEmpty());

        // Both of these hold up a rundown if they are on, so neither may be on
        // until somebody asks for it.
        QCOMPARE(cmd.getTriggerOnNext(), false);
        QCOMPARE(cmd.getWaitForFinish(), false);
        QCOMPARE(cmd.getTimeout(), 10);
    }

    void timeoutIsClamped()
    {
        ShellCommand cmd;

        // A wait with no ceiling is a client that never comes back, so there is
        // always a ceiling and always a floor.
        cmd.setTimeout(0);
        QCOMPARE(cmd.getTimeout(), 1);

        cmd.setTimeout(-30);
        QCOMPARE(cmd.getTimeout(), 1);

        cmd.setTimeout(999);
        QCOMPARE(cmd.getTimeout(), 300);

        cmd.setTimeout(45);
        QCOMPARE(cmd.getTimeout(), 45);
    }

    void survivesTheRoundTrip()
    {
        ShellCommand original;
        original.setCommandLine("\"C:/Program Files/kit/router.exe\" --take 3");
        original.setWorkingDirectory("C:/shows/tonight");
        original.setTriggerOnNext(true);
        original.setWaitForFinish(true);
        original.setTimeout(25);

        boost::property_tree::wptree pt = parseXml(writeToXml(original));

        ShellCommand loaded;
        loaded.readProperties(pt);

        QCOMPARE(loaded.getCommandLine(), original.getCommandLine());
        QCOMPARE(loaded.getWorkingDirectory(), original.getWorkingDirectory());
        QCOMPARE(loaded.getTriggerOnNext(), true);
        QCOMPARE(loaded.getWaitForFinish(), true);
        QCOMPARE(loaded.getTimeout(), 25);
    }

    void anOldRundownLoadsWithTheCautiousDefaults()
    {
        // A rundown saved before this item existed has none of these elements.
        // Reading it must not leave a command that waits, or that runs on next.
        QString xml = "<?xml version=\"1.0\"?><command><channel>1</channel></command>";

        ShellCommand cmd;
        boost::property_tree::wptree pt = parseXml(xml);
        cmd.readProperties(pt);

        QCOMPARE(cmd.getTriggerOnNext(), false);
        QCOMPARE(cmd.getWaitForFinish(), false);
        QCOMPARE(cmd.getTimeout(), 10);
        QVERIFY(cmd.getCommandLine().isEmpty());
    }

    void quotedPathsSplitTheWayATerminalWouldSplitThem()
    {
        // The widget hands the stored line to QProcess::splitCommand and runs the
        // first token as the program. If that ever stopped honouring quotes, every
        // path with a space in it would silently become the wrong program name,
        // so the assumption is pinned here rather than trusted.
        QStringList parts = QProcess::splitCommand(
            QString("\"C:/Program Files/kit/router.exe\" --take 3"));

        QCOMPARE(parts.size(), 3);
        QCOMPARE(parts.at(0), QString("C:/Program Files/kit/router.exe"));
        QCOMPARE(parts.at(1), QString("--take"));
        QCOMPARE(parts.at(2), QString("3"));
    }

    void anEmptyCommandLineYieldsNoProgram()
    {
        // executePlay() returns early on an empty line; this is the reason it has
        // to, because there would be no program to name.
        QVERIFY(QProcess::splitCommand(QString("")).isEmpty());
        QVERIFY(QProcess::splitCommand(QString("   ")).isEmpty());
    }
};

int runShellCommandTest(int argc, char* argv[])
{
    ShellCommandTest t;
    return QTest::qExec(&t, argc, argv);
}

#include "ShellCommandTest.moc"
