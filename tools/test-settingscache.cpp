// Settings answered from memory must be the settings in the database.
//
// DatabaseManager::getConfigurationByName has more than two hundred callers,
// several on the paths an operator feels, and it used to run a query every
// time. It now answers from a cache that updateConfiguration keeps in step.
// A cache that drifts from the table would be a client quietly running on
// settings it no longer has - so every answer here is checked against the
// table itself, read with plain SQL on the same connection.
//
// The real DatabaseManager.cpp, compiled into this test, over an in-memory
// SQLite database.

#include "../src/Core/DatabaseManager.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

static int checks = 0;
static int failures = 0;

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

// What the table says, first row by id, the way the old query answered.
static QString tableValue(const QString& name, bool* present = nullptr)
{
    QSqlQuery sql;
    sql.prepare("SELECT Value FROM Configuration WHERE Name = :Name ORDER BY Id");
    sql.bindValue(":Name", name);
    sql.exec();

    const bool found = sql.first();
    if (present != nullptr)
        *present = found;

    return found ? sql.value(0).toString() : QString();
}

static void exec(const QString& statement)
{
    QSqlQuery sql;
    if (!sql.exec(statement))
        QTextStream(stdout) << "  set-up failed: " << statement << "\n";
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE");
    database.setDatabaseName(":memory:");
    if (!database.open())
    {
        out << "could not open an in-memory database\n0 passed, 1 failed\n";
        return 1;
    }

    exec("CREATE TABLE Configuration (Id INTEGER PRIMARY KEY, Name TEXT, Value TEXT)");
    exec("INSERT INTO Configuration (Name, Value) VALUES ('StoreThumbnailsInDatabase', 'true')");
    exec("INSERT INTO Configuration (Name, Value) VALUES ('ShowLastAction', 'false')");
    exec("INSERT INTO Configuration (Name, Value) VALUES ('Twice', 'first')");
    exec("INSERT INTO Configuration (Name, Value) VALUES ('Twice', 'second')");
    exec("INSERT INTO Configuration (Name, Value) VALUES ('Empty', '')");

    DatabaseManager& manager = DatabaseManager::getInstance();

    out << "Reading\n";

    ConfigurationModel thumbnails = manager.getConfigurationByName("StoreThumbnailsInDatabase");
    expectTrue(thumbnails.getValue() == "true", "a stored setting reads back");
    expectTrue(thumbnails.getName() == "StoreThumbnailsInDatabase", "with its own name");
    expectTrue(thumbnails.getId() == 1, "and its own row id");

    ConfigurationModel missing = manager.getConfigurationByName("NeverStored");
    expectTrue(missing.getId() == 0 && missing.getName().isEmpty() && missing.getValue().isEmpty(),
               "a setting never stored answers exactly as the query did: id 0, no name, no value");

    expectTrue(manager.getConfigurationByName("Twice").getValue() == "first",
               "a name stored twice answers with its first row, as the query did");
    expectTrue(manager.getConfigurationByName("Empty").getValue().isEmpty()
               && manager.getConfigurationByName("Empty").getId() == 5,
               "an empty value is a stored value, not a missing one");
    expectTrue(manager.getConfigurationByName("storethumbnailsindatabase").getValue().isEmpty(),
               "names are case-sensitive, as SQLite compared them");

    out << "\nWriting\n";

    manager.updateConfiguration(ConfigurationModel(0, "ShowLastAction", "true"));
    expectTrue(manager.getConfigurationByName("ShowLastAction").getValue() == "true", "a changed setting reads back changed");
    expectTrue(tableValue("ShowLastAction") == "true", "and the table has it");

    manager.updateConfiguration(ConfigurationModel(0, "BrandNew", "42"));
    bool present = false;
    expectTrue(tableValue("BrandNew", &present) == "42" && present, "a new setting is inserted into the table");
    expectTrue(manager.getConfigurationByName("BrandNew").getValue() == "42", "and reads back without a restart");
    expectTrue(manager.getConfigurationByName("BrandNew").getId() == 6, "with the id the table gave it");

    manager.updateConfiguration(ConfigurationModel(0, "Twice", "both"));
    expectTrue(manager.getConfigurationByName("Twice").getValue() == "both", "updating a doubled name changes what reads back");
    expectTrue(tableValue("Twice") == "both", "which is what the table's first row now says");

    manager.updateConfiguration(ConfigurationModel(0, "Empty", "filled"));
    manager.updateConfiguration(ConfigurationModel(0, "Empty", ""));
    expectTrue(manager.getConfigurationByName("Empty").getValue().isEmpty() && tableValue("Empty").isEmpty(),
               "a setting can be emptied again");

    out << "\nAgreement\n";

    // Every name the table holds, and a few it does not: the cache and the table
    // give the same answer for all of them.
    QStringList names;
    {
        QSqlQuery sql("SELECT DISTINCT Name FROM Configuration");
        while (sql.next())
            names << sql.value(0).toString();
    }
    names << "NeverStored" << "" << "Show Last Action";

    int agreed = 0;
    foreach (const QString& name, names)
    {
        bool inTable = false;
        const QString wanted = tableValue(name, &inTable);
        const ConfigurationModel got = manager.getConfigurationByName(name);

        if (got.getValue() == wanted && (inTable ? got.getName() == name : got.getName().isEmpty()))
            agreed++;
        else
            out << "  disagrees on '" << name << "': table '" << wanted << "', cache '" << got.getValue() << "'\n";
    }
    expectTrue(agreed == names.count(), QString("the cache agrees with the table on all %1 names").arg(names.count()));

    out << "\nDevices and formats\n";

    exec("CREATE TABLE Device (Id INTEGER PRIMARY KEY, Name TEXT, Address TEXT, Port INTEGER, Username TEXT, Password TEXT, "
         "Description TEXT, Version TEXT, Shadow TEXT, Channels INTEGER, ChannelFormats TEXT, PreviewChannel INTEGER, "
         "LockedChannel INTEGER, TemplatePath TEXT, MediaPath TEXT, ServerPath TEXT)");
    exec("CREATE TABLE Format (Id INTEGER PRIMARY KEY, Name TEXT, Width INTEGER, Height INTEGER, FramesPerSecond TEXT)");
    exec("INSERT INTO Format (Name, Width, Height, FramesPerSecond) VALUES ('1080i5000', 1920, 1080, '25'), ('1080p5000', 1920, 1080, '50')");
    exec("INSERT INTO Device (Name, Address, Port, Username, Password, Description, Version, Shadow, Channels, ChannelFormats, "
         "PreviewChannel, LockedChannel, TemplatePath, MediaPath, ServerPath) "
         "VALUES ('Server A', '10.0.0.1', 5250, '', '', '', '2.3', 'No', 2, '1080i5000,1080p5000', 0, 0, '', '', '')");

    const auto tableFormats = [](const QString& name) {
        QSqlQuery sql;
        sql.prepare("SELECT ChannelFormats FROM Device WHERE Name = :Name");
        sql.bindValue(":Name", name);
        sql.exec();
        return sql.first() ? sql.value(0).toString() : QString();
    };

    expectTrue(manager.getDeviceByName("Server A").getChannelFormats() == "1080i5000,1080p5000", "a device reads back");
    expectTrue(manager.getDeviceByName("Server A").getChannelFormats() == "1080i5000,1080p5000", "and again from memory, the same");
    expectTrue(manager.getFormat("1080p5000").getFramesPerSecond() == "50", "a format reads back");
    expectTrue(manager.getFormat("1080p5000").getFramesPerSecond() == "50", "and again from memory, the same");
    expectTrue(manager.getFormat("nonsense").getFramesPerSecond().isEmpty(), "an unknown format is empty, as the query returned");

    DeviceModel changed(0, "Server A", "10.0.0.1", 5250, "", "", "", "2.3", "No", 2, "1080p5000,1080p5000", 0, 0, "", "", "");
    manager.updateDeviceChannelFormats(changed);
    expectTrue(tableFormats("Server A") == "1080p5000,1080p5000", "the channel formats changed in the table");
    expectTrue(manager.getDeviceByName("Server A").getChannelFormats() == "1080p5000,1080p5000",
               "and the device read after it has them - the cached copy was let go");

    const int idA = manager.getDeviceByName("Server A").getId();
    DeviceModel renamed(idA, "Server B", "10.0.0.1", 5250, "", "", "", "2.3", "No", 2, "1080p5000,1080p5000", 0, 0, "", "", "");
    manager.updateDevice(renamed);
    expectTrue(manager.getDeviceByName("Server A").getId() == 0, "a renamed device is no longer found by its old name");
    expectTrue(manager.getDeviceByName("Server B").getId() == idA, "and is found by its new one");

    DeviceModel added(0, "Server C", "10.0.0.2", 5250, "", "", "", "", "No", 1, "1080i5000", 0, 0, "", "", "");
    expectTrue(manager.getDeviceByName("Server C").getId() == 0, "a device not yet added is empty");
    manager.insertDevice(added);
    expectTrue(manager.getDeviceByName("Server C").getId() > 0, "and found as soon as it is added, though its absence was remembered");

    manager.updateDeviceVersion(DeviceModel(0, "", "10.0.0.2", 0, "", "", "", "2.4", "", 0, "", 0, 0, "", "", ""));
    expectTrue(manager.getDeviceByName("Server C").getVersion() == "2.4", "a version written by address is seen by name");

    manager.updateDeviceChannels(DeviceModel(0, "", "10.0.0.2", 0, "", "", "", "", "", 4, "", 0, 0, "", "", ""));
    expectTrue(manager.getDeviceByName("Server C").getChannels() == 4, "a channel count written by address is seen by name");

    exec("CREATE TABLE Thumbnail (Id INTEGER PRIMARY KEY, Data TEXT, Timestamp TEXT, Size TEXT)");
    exec("CREATE TABLE Library (Id INTEGER PRIMARY KEY, Name TEXT, DeviceId INTEGER, TypeId INTEGER, ThumbnailId INTEGER, Timecode TEXT, Size INTEGER DEFAULT -1, Timestamp TEXT)");
    manager.deleteDevice(manager.getDeviceByName("Server C").getId());
    expectTrue(manager.getDeviceByName("Server C").getId() == 0, "a deleted device is gone");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
