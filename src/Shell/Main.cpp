#include "Version.h"
#include "Global.h"

#include "Application.h"

#include "../Common/LogRetention.h"
#include "../Core/DatabaseManager.h"
#include "../Core/EventManager.h"
#include "../Core/GpiManager.h"
#include "../Core/LibraryManager.h"
#include "../Core/DeviceManager.h"
#include "../Core/OscDeviceManager.h"
#include "../Core/OscWebSocketManager.h"
#include "../Core/Events/Rundown/OpenRundownEvent.h"

#include "../Widgets/MainWindow.h"
#include "../Widgets/SafeStart.h"
#include "../Widgets/KeepAwake.h"
#include "../Widgets/ServerProcessControl.h"
#include "../Common/StartupGuard.h"
#include "../Core/Events/StatusbarEvent.h"
#include "../Widgets/RelayClient.h"
#include "../Widgets/SheetCacheServer.h"

#ifdef Q_OS_MAC
    #include "Mac/AppNap.h"
#endif

#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>
#include <QtCore5Compat/QRegExp>
#include <QtCore/QDir>
#include <QtCore/QMutex>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QTextStream>
#include <QtCore/QCommandLineParser>
#include <QtCore/QCommandLineOption>

#include <QtGui/QPixmap>
#include <QtGui/QFontDatabase>

#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
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

    // One file, kept open, behind a lock. Every line used to check the folder,
    // open the file, write and close it - a file-system round trip per line, with
    // a virus scanner looking at every close - from whichever thread logged: the
    // GUI on every AMCP command and reply, and the OSC, NDI and thumbnail threads
    // besides, with nothing stopping two appends interleaving. The file is
    // reopened when the date changes, so each day still gets its own log, and
    // flushed after each line, so a crash loses nothing that was written.
    //
    // Allocated and never freed, on purpose. A static object is destroyed at exit,
    // and Qt still logs from its own global destructors after that - a database
    // connection still in use, a thread still running - which would then write
    // through a destroyed file and lock while the client closes. Every line is
    // flushed, so nothing is left unwritten when the process ends.
    static QMutex* logMutex = new QMutex();
    static QFile* logFile = new QFile();
    static QString* logDate = new QString();

    QMutexLocker locker(logMutex);

    const QString today = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    if (!logFile->isOpen() || today != *logDate)
    {
        logFile->close();

        QString path = QString("%1/.CasparCG/Client/Logs").arg(QDir::homePath());
        QDir().mkpath(path);

        logFile->setFileName(QString("%1/Client_%2.log").arg(path).arg(today));
        logFile->open(QIODevice::WriteOnly | QIODevice::Append);
        *logDate = today;

        // A new day's file, at startup or at midnight: the client's own logs
        // older than 30 days go (LogRetention.h). Nothing else in the folder is
        // touched. Nothing is logged about it either - this runs inside the log
        // handler, holding its lock, and a line from here would wait on itself.
        const QDir logDir(path);
        const QStringList names = logDir.entryList(QStringList() << "Client_*.log", QDir::Files);
        for (const QString& name : LogRetention::expired(names, QDate::currentDate()))
            QFile::remove(logDir.filePath(name));
    }

    if (logFile->isOpen())
    {
        logFile->write(logMessage.toUtf8());
        logFile->write(QByteArrayLiteral("\n"));
        logFile->flush();
    }

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
        "Live", "Clock", "ServerStatus", "Activity", "TriggerBanks",
        "NDI", "Performance"
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

    // Channel colours: before build 313 they were applied only once a slider
    // in Settings was moved, so a venue's colours were lost at every start.
    auto channelColor = [&cfgVal](const char* key, double fallback) {
        bool ok = false;
        const double v = cfgVal(key).toDouble(&ok);
        return ok ? v : fallback;
    };
    ChannelColor::setAngle(channelColor("ChannelColorAngle", ChannelColor::DEFAULT_ANGLE));
    ChannelColor::setOffset(channelColor("ChannelColorOffset", ChannelColor::DEFAULT_OFFSET));
    ChannelColor::setSaturation(channelColor("ChannelColorSaturation", ChannelColor::DEFAULT_SATURATION));
    ChannelColor::setLightness(channelColor("ChannelColorLightness", ChannelColor::DEFAULT_LIGHTNESS));

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

    // An OGraf graphic is an ES module that the preview page imports, and
    // Chromium refuses module imports between file:// URLs because their origin
    // is null. Without this, every OGraf graphic fails to load with a CORS error
    // and nothing else explains why.
    //
    // The scope is narrow in practice: it lets local pages read local files, and
    // the only local page this client ever loads is the one it writes itself,
    // beside the operator's own templates on their own machine.
    if (!qEnvironmentVariableIsSet("QTWEBENGINE_CHROMIUM_FLAGS"))
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--allow-file-access-from-files");
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

    // Did the last start finish, and did the last session close? Two markers in
    // the data folder say (Common/StartupGuard.h). A start that never got as far
    // as its window offers a safe start; holding Shift asks for one outright.
    const QString dataFolder = QString("%1/.CasparCG/Client").arg(QDir::homePath());
    QDir().mkpath(dataFolder);
    const QString startingPath = QDir(dataFolder).filePath(StartupGuard::startingName());
    const QString runningPath = QDir(dataFolder).filePath(StartupGuard::runningName());

    auto readMarker = [](const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return StartupGuard::Marker();
        return StartupGuard::parseMarker(QString::fromUtf8(file.readAll()));
    };
    auto writeMarker = [](const QString& path) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            file.write(StartupGuard::markerText(quint32(QCoreApplication::applicationPid()), QDateTime::currentDateTime(),
                                                QString(DEV_BUILD_ID)).toUtf8());
    };
    const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    auto alive = [exeName](quint32 pid) {
        if (pid == quint32(QCoreApplication::applicationPid()))
            return false;
        for (const ServerProcessControl::Running& process : ServerProcessControl::runningNamed(exeName))
        {
            if (process.pid == pid)
                return true;
        }
        return false;
    };

    const StartupGuard::Last last = StartupGuard::judge(readMarker(startingPath), readMarker(runningPath), alive);

    bool safe = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier);
    if (!safe && last == StartupGuard::Last::StartFailed)
    {
        QMessageBox box;
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle("CasparCG Client");
        box.setText("The client did not finish starting last time.");
        box.setInformativeText(QString("Start safely? A safe start leaves off:\n\n- %1\n\nNo setting is changed, and the next start is a "
                                       "normal one. Holding Shift while the client starts asks for a safe start too.")
                                   .arg(StartupGuard::safeStartSkips().join("\n- ")));
        QPushButton* safeButton = box.addButton("Safe Start", QMessageBox::AcceptRole);
        box.addButton("Start Normally", QMessageBox::RejectRole);
        box.setDefaultButton(safeButton);
        box.exec();
        safe = box.clickedButton() == safeButton;
    }
    SafeStart::setActive(safe);
    if (safe)
        qWarning("Safe start: %s are off", qPrintable(StartupGuard::safeStartSkips().join(", ")));
    if (last == StartupGuard::Last::DidNotClose)
        qWarning("The last session did not close normally");

    QFile::remove(runningPath);
    writeMarker(startingPath);

    QElapsedTimer startClock;
    startClock.start();

    QSplashScreen splashScreen(QPixmap(":/Graphics/Images/SplashScreen.png"));
    splashScreen.show();

    loadDatabase(&args);
    DatabaseManager::getInstance().initialize();

    loadStyleSheets(application);
    loadFonts(application);

    EventManager::getInstance().initialize();
    GpiManager::getInstance().initialize();

    // How long the client takes to be on screen, split into the window's own
    // construction - every panel, and the Inspector's sections inside it - and
    // everything before it. Written once per start, so a slow startup on the
    // user's machine can be read from an ordinary log.
    QElapsedTimer windowClock;
    windowClock.start();

    MainWindow window;
    const qint64 windowBuilt = windowClock.elapsed();

    splashScreen.finish(&window);

    loadConfiguration(window, &args);

    window.show();

    // A playout machine stays awake while the client runs (Settings > General).
    KeepAwake::apply();

    qDebug("Starting the client took %lld ms, of which the window took %lld ms",
           startClock.elapsed(), windowBuilt);

    LibraryManager::getInstance().initialize();
    DeviceManager::getInstance().initialize();
    OscDeviceManager::getInstance().initialize();
    OscWebSocketManager::getInstance().initialize();
    if (!safe)
    {
        SheetCacheServer::getInstance().start();
        RelayClient::getInstance().start();
    }

    // On screen for a few seconds: this start finished. From now on a marker
    // left behind means the session did not close normally.
    QTimer::singleShot(5000, &window, [startingPath, runningPath, writeMarker]() {
        QFile::remove(startingPath);
        writeMarker(runningPath);
    });

    QTimer::singleShot(1500, &window, [safe, last]() {
        if (safe)
            EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(
                QString("SAFE START: %1 are off. Start the client normally to have them back.")
                    .arg(StartupGuard::safeStartSkips().join(", ")), 30000, true));
        else if (last == StartupGuard::Last::DidNotClose)
            EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(
                "The client did not close normally last time. Help > Collect a Bug Report includes what Windows recorded about it.",
                20000, true));
    });

    int returnValue = application.exec();

    // Closed normally.
    QFile::remove(startingPath);
    QFile::remove(runningPath);

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
