#include "Version.h"
#include "Global.h"

#include "Application.h"

#include "../Core/DatabaseManager.h"
#include "../Core/EventManager.h"
#include "../Core/GpiManager.h"
#include "../Core/LibraryManager.h"
#include "../Core/DeviceManager.h"
#include "../Core/OscDeviceManager.h"
#include "../Core/OscWebSocketManager.h"
#include "../Core/Events/Rundown/OpenRundownEvent.h"

#include "../Widgets/MainWindow.h"
#include "../Widgets/RelayClient.h"
#include "../Widgets/SheetCacheServer.h"

#ifdef Q_OS_MAC
    #include "Mac/AppNap.h"
#endif

#include <QtCore5Compat/QRegExp>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QTextStream>
#include <QtCore/QCommandLineParser>
#include <QtCore/QCommandLineOption>

#include <QtGui/QPixmap>
#include <QtGui/QFontDatabase>

#include <QtWidgets/QApplication>
#include <QtWidgets/QSplashScreen>
#include <QtWidgets/QStyleFactory>

#include <QtSql/QSqlDatabase>

struct CommandLineArgs
{
    QString rundown;

    QString sqlitepath;

    bool dbmemory = false;
    bool fullscreen = false;
};

enum CommandLineParseResult
{
    CommandLineOk,
    CommandLineError,
    CommandLineVersionRequested,
    CommandLineHelpRequested
};

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    Q_UNUSED(context);

    QString logMessage;
    QString threadId = QString::number((long long)QThread::currentThreadId(), 16);
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

    switch (type)
    {
        case QtDebugMsg:
            logMessage = QString("[%1] [%2] [D] %3").arg(timestamp).arg(threadId).arg(message);
            break;
        case QtWarningMsg:
            logMessage = QString("[%1] [%2] [W] %3").arg(timestamp).arg(threadId).arg(message);
            break;
        case QtCriticalMsg:
            logMessage = QString("[%1] [%2] [C] %3").arg(timestamp).arg(threadId).arg(message);
            break;
        case QtFatalMsg:
            logMessage = QString("[%1] [%2] [F] %3").arg(timestamp).arg(threadId).arg(message);
        break;
        case QtInfoMsg:
            logMessage = QString("[%1] [%2] [I] %3").arg(timestamp).arg(threadId).arg(message);
        break;
    }

    fprintf(stderr, "%s\n", qPrintable(logMessage));

    QString path = QString("%1/.CasparCG/Client/Logs").arg(QDir::homePath());

    QDir directory(path);
    if (!directory.exists())
        directory.mkpath(".");

    QFile logFile(QString("%1/Client_%2.log").arg(path).arg(QDateTime::currentDateTime().toString("yyyy-MM-dd")));
    logFile.open(QIODevice::WriteOnly | QIODevice::Append);

    QTextStream logStream(&logFile);
    logStream << logMessage << Qt::endl;

    logFile.close();

    if (type == QtFatalMsg)
       abort();
}

void loadDatabase(CommandLineArgs* args)
{
    QString path = QString("%1/.CasparCG/Client").arg(QDir::homePath());

    QDir directory(path);
    if (!directory.exists())
        directory.mkpath(".");

    QSqlDatabase database;
    if (args->dbmemory)
    {
        qDebug("Using SQLite in memory database");

        database = QSqlDatabase::addDatabase("QSQLITE");
        database.setDatabaseName(":memory:");
    }
    else
    {
        qDebug("Using SQLite database");

        database = QSqlDatabase::addDatabase("QSQLITE");
        QString databaseLocation = QString("%1/Database.s3db").arg(path);
        if (!args->sqlitepath.isEmpty())
            databaseLocation = args->sqlitepath;

        database.setDatabaseName(databaseLocation);
    }

    if (!database.open())
        qCritical("Unable to open database");
}

void loadStyleSheets(QApplication& application)
{
    QString stylesheet;

    // Bulk-load all config values in a single DB query (replaces ~49 individual SELECTs).
    QMap<QString, QString> cfg = DatabaseManager::getInstance().getAllConfigurations();
    auto cfgVal = [&cfg](const QString& key) -> QString { return cfg.value(key, QString()); };

    QString theme = cfgVal("Theme");

    // Load default stylesheet.
    QFile defaultStylesheet(QString(":/Appearances/Stylesheets/%1/Default.css").arg(theme));
    if (defaultStylesheet.open(QFile::ReadOnly))
    {
        QTextStream stream(&defaultStylesheet);
        stylesheet = stream.readAll();
        defaultStylesheet.close();
    }

    // Load extended stylesheet.
    QFile extendedStylesheet(QString(":/Appearances/Stylesheets/%1/Extended.css").arg(theme));
    if (extendedStylesheet.open(QFile::ReadOnly))
    {
        QTextStream stream(&extendedStylesheet);
        stylesheet += stream.readAll();
        extendedStylesheet.close();
    }

    // Load platform stylesheet.
#if defined(Q_OS_WIN)
    QFile platformStylesheet(QString(":/Appearances/Stylesheets/%1/Windows.css").arg(theme));
#elif defined(Q_OS_MAC)
    QFile platformStylesheet(QString(":/Appearances/Stylesheets/%1/Mac.css").arg(theme));
#elif defined(Q_OS_LINUX)
    QFile platformStylesheet(QString(":/Appearances/Stylesheets/%1/Linux.css").arg(theme));
#endif
    if (platformStylesheet.open(QFile::ReadOnly))
    {
        QTextStream stream(&platformStylesheet);
        stylesheet += stream.readAll();
        platformStylesheet.close();
    }

    // Populate all ColorCache values from the bulk-loaded cache.
    ColorCache::setPvwButton(cfgVal("PVWButtonColor"));
    ColorCache::setStepButton(cfgVal("STEPButtonColor"));
    ColorCache::setPreviewBorder(cfgVal("PreviewBorderColor"));
    ColorCache::setAutostepHighlight(cfgVal("AutostepHighlightColor"));
    ColorCache::setActiveIndicator(cfgVal("ActiveIndicatorColor"));
    ColorCache::setLibrarySectionLine(cfgVal("LibrarySectionLineColor"));

    // Header colors: master, rundown, per-widget overrides.
    ColorCache::setHeaderLineMaster(cfgVal("HeaderLineMaster"));
    ColorCache::setHeaderBlockMaster(cfgVal("HeaderBlockMaster"));
    ColorCache::setHeaderTextMaster(cfgVal("HeaderTextMaster"));
    ColorCache::setHeaderLineRundown(cfgVal("HeaderLineRundown"));
    ColorCache::setHeaderBlockRundown(cfgVal("HeaderBlockRundown"));
    ColorCache::setHeaderTextRundown(cfgVal("HeaderTextRundown"));
    ColorCache::setCustomizeHeaders(cfgVal("CustomizeWidgetHeaders") == "true");

    static const char* panelKeys[] = {
        "Library", "Inspector", "AudioLevels", "Preview",
        "Live", "Clock", "ServerStatus", "Activity", "TriggerBanks"
    };
    for (const auto& key : panelKeys)
    {
        QString lineVal  = cfgVal(QString("HeaderLine_%1").arg(key));
        QString blockVal = cfgVal(QString("HeaderBlock_%1").arg(key));
        QString textVal  = cfgVal(QString("HeaderText_%1").arg(key));
        if (!lineVal.isEmpty())  ColorCache::setLineOverride(key, lineVal);
        if (!blockVal.isEmpty()) ColorCache::setBlockOverride(key, blockVal);
        if (!textVal.isEmpty())  ColorCache::setTextOverride(key, textVal);
    }

    ColorCache::setClockColor1(cfgVal("ClockColor1"));
    ColorCache::setClockColor2(cfgVal("ClockColor2"));
    ColorCache::setClockShadow(cfgVal("ClockShadowColor"));

    QString headerCSS = WidgetHeaderCSS::generate();
    if (!headerCSS.isEmpty())
        stylesheet += headerCSS;

    // Append font-size so it's included in the single setStyleSheet call.
    QString fontSize = cfgVal("FontSize");
    if (!fontSize.isEmpty())
        stylesheet += QString("\nQWidget { font-size: %1px; }").arg(fontSize.toInt());

    // Apply the complete stylesheet once (previously called 5 times during startup).
    application.setStyleSheet(stylesheet);
}

void loadFonts(QApplication& application)
{
    QFontDatabase::addApplicationFont(":/Appearances/Fonts/OpenSans-Bold.ttf");
    QFontDatabase::addApplicationFont(":/Appearances/Fonts/OpenSans-BoldItalic.ttf");
    QFontDatabase::addApplicationFont(":/Appearances/Fonts/OpenSans-ExtraBold.ttf");
    QFontDatabase::addApplicationFont(":/Appearances/Fonts/OpenSans-ExtraBoldItalic.ttf");
    QFontDatabase::addApplicationFont(":/Appearances/Fonts/OpenSans-Italic.ttf");
    QFontDatabase::addApplicationFont(":/Appearances/Fonts/OpenSans-Light.ttf");
    QFontDatabase::addApplicationFont(":/Appearances/Fonts/OpenSans-LightItalic.ttf");
    QFontDatabase::addApplicationFont(":/Appearances/Fonts/OpenSans-Regular.ttf");
    QFontDatabase::addApplicationFont(":/Appearances/Fonts/OpenSans-Semibold.ttf");
    QFontDatabase::addApplicationFont(":/Appearances/Fonts/OpenSans-SemiboldItalic.ttf");

#if defined(Q_OS_UNIX)
    application.setFont(QFont("Open Sans"));
#elif defined(Q_OS_WIN)
   application.setFont(QFont("Open Sans Semibold"));
#endif
}

void loadConfiguration(QMainWindow& window, CommandLineArgs* args)
{
    // Check command line arguments followed by the configuration.
    if (args->fullscreen || DatabaseManager::getInstance().getConfigurationByName("StartFullscreen").getValue() == "true")
         window.showFullScreen();

    if (!args->rundown.isEmpty())
        EventManager::getInstance().fireOpenRundownEvent(OpenRundownEvent(args->rundown));
}

CommandLineParseResult parseCommandLine(QCommandLineParser& parser, CommandLineArgs* args)
{
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOption({{"f", "fullscreen"}, "Start application in fullscreen."});
    parser.addOption({{"m", "dbmemory"}, "Use SQLite in memory database."});
    parser.addOption({{"r", "rundown"}, "The rundown path.", "rundown"});
    parser.addOption({{"t", "sqlitepath"}, "The SQLite database path.", "sqlitepath"});

    if (!parser.parse(QApplication::arguments()))
        return CommandLineError;

    if (parser.isSet("version"))
        return CommandLineVersionRequested;

    if (parser.isSet("help"))
        return CommandLineHelpRequested;

    if (parser.isSet("rundown"))
        args->rundown = parser.value("rundown");

    if (parser.isSet("fullscreen"))
        args->fullscreen = true;

    if (parser.isSet("dbmemory"))
        args->dbmemory = true;

    if (parser.isSet("sqlitepath"))
        args->sqlitepath = parser.value("sqlitepath");

    return CommandLineOk;
}

int main(int argc, char* argv[])
{
#ifdef Q_OS_MAC
    AppNap appNap;
#endif

    qInstallMessageHandler(messageHandler);

    // QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

#ifdef CASPARCG_HAS_WEBENGINE
    // The Preview panel renders HTML templates in a QWebEngineView, which shares
    // an OpenGL context with the rest of the application. This has to be set
    // before the QApplication exists, so it cannot live with the panel.
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif

    Application application(argc, argv);
    application.setApplicationName("CasparCG Client");
    application.setApplicationVersion(QString("%1.%2.%3.%4").arg(MAJOR_VERSION).arg(MINOR_VERSION).arg(REVISION_VERSION).arg(BUILD_VERSION));

    qDebug("Starting %s %s", qPrintable(application.applicationName()), qPrintable(application.applicationVersion()));

    CommandLineArgs args;
    QCommandLineParser parser;
    switch (parseCommandLine(parser, &args))
    {
        case CommandLineOk:
            break;
        case CommandLineError:
            qCritical("Unable to parse command line: %s", qPrintable(parser.errorText()));
            parser.showHelp();
            return 0;
        case CommandLineVersionRequested:
            parser.showVersion();
            return 0;
        case CommandLineHelpRequested:
            parser.showHelp();
            return 0;
    }

    QSplashScreen splashScreen(QPixmap(":/Graphics/Images/SplashScreen.png"));
    splashScreen.show();

    loadDatabase(&args);
    DatabaseManager::getInstance().initialize();

    loadStyleSheets(application);
    loadFonts(application);

    EventManager::getInstance().initialize();
    GpiManager::getInstance().initialize();

    MainWindow window;
    splashScreen.finish(&window);

    loadConfiguration(window, &args);

    window.show();

    LibraryManager::getInstance().initialize();
    DeviceManager::getInstance().initialize();
    OscDeviceManager::getInstance().initialize();
    OscWebSocketManager::getInstance().initialize();
    SheetCacheServer::getInstance().start();
    RelayClient::getInstance().start();

    int returnValue = application.exec();

    EventManager::getInstance().uninitialize();
    DatabaseManager::getInstance().uninitialize();
    GpiManager::getInstance().uninitialize();
    RelayClient::getInstance().stop();
    SheetCacheServer::getInstance().stop();
    OscWebSocketManager::getInstance().uninitialize();
    OscDeviceManager::getInstance().uninitialize();
    DeviceManager::getInstance().uninitialize();
    LibraryManager::getInstance().uninitialize();

    return returnValue;
}
