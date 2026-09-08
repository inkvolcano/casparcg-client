// Who decides which packs a machine takes.
//
// One repository or relay carries every project, and each machine takes a subset.
// That subset used to be decided on the machine, which meant visiting a venue to
// change it. Now the source can decide instead, and this is the rule that settles
// which answer wins.
//
//   the source names this machine   -> the source decides
//   the source has a "*" entry      -> that, for machines it does not name
//   the source says nothing at all  -> whatever the machine was set to locally
//   the machine is set to override  -> the machine, always
//
// The case worth being careful about is a machine named with an empty list. That is
// a real instruction meaning "take nothing", and it must not collapse into "the
// source said nothing", because an empty filter further down means "take everything".
// Getting those two confused would hand a venue every project in the estate.

#include "../src/Widgets/RelayClient.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QSysInfo>
#include <QtCore/QTextStream>

static int failures = 0;
static int checks = 0;

static void same(const QStringList& actual, const QStringList& wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n"
                        << "        wanted [" << wanted.join(", ") << "]\n"
                        << "        got    [" << actual.join(", ") << "]\n";
}

static QJsonObject parse(const QString& json)
{
    return QJsonDocument::fromJson(json.toUtf8()).object();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    // The tests speak about this machine by its real name, because that is the key
    // the rule matches on.
    const QString me = QSysInfo::machineHostName();
    const QString other = me + "-SOMEWHERE-ELSE";

    out << "This machine is named\n";

    same(RelayClient::assignedPacks(parse(
             QString("{\"%1\": [\"SEVILLE\", \"SHARED\"]}").arg(me))),
         QStringList() << "SEVILLE" << "SHARED",
         "it gets what it is assigned");

    // Nobody types a hostname the same way twice.
    same(RelayClient::assignedPacks(parse(
             QString("{\"%1\": [\"SEVILLE\"]}").arg(me.toLower()))),
         QStringList() << "SEVILLE",
         "matched without regard to case");

    same(RelayClient::assignedPacks(parse(
             QString("{\"%1\": [\"MARSEILLE\"], \"*\": [\"SHARED\"]}").arg(other))),
         QStringList() << "SHARED",
         "another machine's entry is not taken, the default is");

    out << "\nThis machine is not named\n";

    same(RelayClient::assignedPacks(parse("{\"*\": [\"SHARED\"]}")),
         QStringList() << "SHARED",
         "the default applies");

    same(RelayClient::assignedPacks(parse(
             QString("{\"%1\": [\"MARSEILLE\"]}").arg(other))),
         QStringList(),
         "with no default, nothing is assigned");

    out << "\nThe source says nothing at all\n";

    // No file, or an empty one. The machine keeps whatever it was set to, which is
    // what stops adding this feature from changing what any existing client does.
    same(RelayClient::packsForThisMachine(QJsonObject()),
         RelayClient::packFilter(),
         "an empty source leaves the local setting alone");

    same(RelayClient::packsForThisMachine(parse(
             QString("{\"%1\": [\"MARSEILLE\"]}").arg(other))),
         RelayClient::packFilter(),
         "and so does a source that only names other machines");

    out << "\nNamed with nothing is not the same as unnamed\n";

    {
        // The dangerous confusion. An empty list here means take nothing; an empty
        // list further down means take everything.
        QStringList takesNothing = RelayClient::packsForThisMachine(parse(
            QString("{\"%1\": []}").arg(me)));

        checks++;
        if (takesNothing.isEmpty())
        {
            failures++;
            out << "  FAIL  a machine assigned nothing came back with an empty list,\n"
                << "        which downstream means every pack in the estate\n";
        }

        checks++;
        if (takesNothing.contains("SEVILLE") || takesNothing.contains("SHARED"))
        {
            failures++;
            out << "  FAIL  a machine assigned nothing was given a real pack name\n";
        }
    }

    out << "\nAnd the way out\n";

    // Whatever the source says, a machine set to decide for itself does.
    // packsDecidedLocally reads the database, which the stubs answer from the
    // environment, so this drives it the same way the settings dialog would.
    qputenv("CASPARCG_TEST_RelayPacksLocal", "true");
    qputenv("CASPARCG_TEST_RelayPacks", "LOCALONLY");

    same(RelayClient::packsForThisMachine(parse(
             QString("{\"%1\": [\"SEVILLE\", \"SHARED\"]}").arg(me))),
         QStringList() << "LOCALONLY",
         "an overriding machine ignores what it is assigned");

    qputenv("CASPARCG_TEST_RelayPacksLocal", "false");

    same(RelayClient::packsForThisMachine(parse(
             QString("{\"%1\": [\"SEVILLE\", \"SHARED\"]}").arg(me))),
         QStringList() << "SEVILLE" << "SHARED",
         "and with the override off, the source decides again");

    out << "\nRubbish in the file\n";

    same(RelayClient::assignedPacks(parse(QString("{\"%1\": \"not-a-list\"}").arg(me))),
         QStringList(), "a value that is not a list assigns nothing");
    same(RelayClient::assignedPacks(parse(QString("{\"%1\": [\"\", \"  \"]}").arg(me))),
         QStringList(), "blank names are dropped");
    same(RelayClient::assignedPacks(parse("{}")), QStringList(), "an empty object assigns nothing");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    out.flush();

    return failures == 0 ? 0 : 1;
}
