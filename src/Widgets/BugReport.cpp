#include "BugReport.h"

#include "BugReportRules.h"
#include "PreviewWidget.h"
#include "RelayClient.h"
#include "RepoPublisher.h"
#include "TemplateInstaller.h"

#include "DatabaseManager.h"
#include "Models/DeviceModel.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>
#include <QtCore/QSysInfo>
#include <QtCore/QTextStream>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

namespace
{
    // wevtutil and Compress-Archive are quick; a machine where either takes
    // longer than this has a bigger problem than the report.
    const int TOOL_TIMEOUT_MS = 60 * 1000;

    // How many recent application-log faults to read before filtering ours.
    const int CRASH_EVENTS_TO_READ = 60;

    void note(QStringList* notes, const QString& line)
    {
        if (notes != nullptr)
            notes->append(line);
    }
}

QString BugReport::dataFolder()
{
    return QString("%1/.CasparCG/Client").arg(QDir::homePath());
}

QString BugReport::collect(const QString& versionLine, QStringList* notes, QString* error)
{
    const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    const QString folder = QDir(desktop.isEmpty() ? QDir::homePath() : desktop)
        .filePath(BugReportRules::reportFolderName(QDateTime::currentDateTime()));

    if (!QDir().mkpath(folder))
    {
        if (error != nullptr)
            *error = QString("could not create %1").arg(QDir::toNativeSeparators(folder));
        return QString();
    }

    copyLogs(folder, notes);
    copyDatabase(folder, notes);
    writeAbout(folder, versionLine, notes);
    writeCrashRecords(folder, notes);
    copyWerReport(folder, notes);

    const QString zip = zipFolder(folder, notes);
    return zip.isEmpty() ? folder : zip;
}

bool BugReport::copyLogs(const QString& into, QStringList* notes)
{
    const QString logs = dataFolder() + "/Logs";
    int copied = 0;

    foreach (const QString& name, BugReportRules::logFileNames(QDate::currentDate()))
    {
        const QString source = QDir(logs).filePath(name);
        if (!QFile::exists(source))
            continue;

        if (QFile::copy(source, QDir(into).filePath(name)))
            copied++;
    }

    note(notes, copied > 0 ? QString("%1 log file(s)").arg(copied) : QString("no log file for today or yesterday"));
    return copied > 0;
}

bool BugReport::copyDatabase(const QString& into, QStringList* notes)
{
    const QString target = QDir(into).filePath("Database.s3db");

    // A consistent copy of an open database, made by SQLite itself. Falls back
    // to a plain file copy on an engine too old to know VACUUM INTO.
    {
        QSqlQuery vacuum;
        if (!vacuum.exec(QString("VACUUM INTO '%1'").arg(QString(target).replace('\'', "''"))))
        {
            if (!QFile::copy(dataFolder() + "/Database.s3db", target))
            {
                note(notes, "database: could not be copied");
                return false;
            }
        }
    }

    // Redacted on the copy, never on the live one. The copy is opened on its own
    // connection so nothing here can touch the connection the client is using.
    bool redacted = true;
    {
        QSqlDatabase copy = QSqlDatabase::addDatabase("QSQLITE", "bugreport");
        copy.setDatabaseName(target);

        if (copy.open())
        {
            foreach (const QString& statement, BugReportRules::redactionStatements())
            {
                QSqlQuery query(copy);
                if (!query.exec(statement))
                    redacted = false;
            }
            copy.close();
        }
        else
        {
            redacted = false;
        }
    }
    QSqlDatabase::removeDatabase("bugreport");

    if (!redacted)
    {
        // Better no database than one with the tokens in it.
        QFile::remove(target);
        note(notes, "database: left out, because its secrets could not be redacted");
        return false;
    }

    note(notes, "database copy, tokens and keys redacted");
    return true;
}

bool BugReport::writeAbout(const QString& into, const QString& versionLine, QStringList* notes)
{
    QFile file(QDir(into).filePath("about.txt"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        note(notes, "about.txt: could not be written");
        return false;
    }

    QTextStream out(&file);
    out << versionLine << "\n";
    out << "Qt " << qVersion() << " at run time\n";
    out << QSysInfo::prettyProductName() << ", " << QSysInfo::currentCpuArchitecture() << "\n";
    out << "Machine: " << QSysInfo::machineHostName() << "\n";
    out << "Collected: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "\n";

    out << "Servers\n";
    foreach (const DeviceModel& device, DatabaseManager::getInstance().getDevice())
    {
        out << "  " << device.getName() << "  " << device.getAddress() << ":" << device.getPort()
            << "  templates: " << device.getTemplatePath()
            << "  media: " << device.getMediaPath() << "\n";
    }
    out << "\n";

    out << "Preview\n";
    out << "  legacy: " << (PreviewWidget::legacyMode() ? "yes" : "no") << "\n";
    out << "  stills from file: " << (PreviewWidget::showStillsFromFile() ? "on" : "off")
        << ", movies: " << (PreviewWidget::showMovies() ? "on" : "off")
        << ", templates: " << (PreviewWidget::showTemplates() ? "on" : "off")
        << ", audio meters: " << (PreviewWidget::showAudioMeters() ? "on" : "off")
        << ", autoplay: " << (PreviewWidget::autoPlayVideo() ? "on" : "off") << "\n";
    out << "  OGraf: " << (PreviewWidget::ografEnabled() ? "on" : "off") << "\n";
    out << "\n";

    out << "Templates\n";
    out << "  pull: " << (RelayClient::isEnabled() ? "on" : "off")
        << "  source: " << (RelayClient::url().isEmpty() ? QString("(none)") : RelayClient::sourceLabel()) << "\n";
    out << "  packs: " << (RelayClient::packFilter().isEmpty() ? QString("(all)") : RelayClient::packFilter().join(", ")) << "\n";
    out << "  master: " << (RepoPublisher::isMaster() ? "yes" : "no") << "\n";
    out << "  last poll: " << RelayClient::getInstance().lastSummary() << "\n";
    out << "  push endpoint: " << (TemplateInstaller::isEnabled() ? "on" : "off") << "\n";
    out << "\n";

    // The sizes of the templates on this machine: a 28 MB template explained a
    // twenty-second hang once, and it was one command to see.
    out << "Largest templates\n";
    QList<QPair<qint64, QString> > sizes;
    foreach (const DeviceModel& device, DatabaseManager::getInstance().getDevice())
    {
        if (device.getTemplatePath().isEmpty())
            continue;

        QDirIterator it(device.getTemplatePath(), QStringList() << "*.html", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            it.next();
            sizes.append(qMakePair(it.fileInfo().size(), it.filePath()));
        }
    }
    std::sort(sizes.begin(), sizes.end(), [](const QPair<qint64, QString>& a, const QPair<qint64, QString>& b) { return a.first > b.first; });
    for (int i = 0; i < sizes.count() && i < 8; i++)
        out << "  " << QString::number(sizes.at(i).first / (1024.0 * 1024.0), 'f', 1) << " MB  " << sizes.at(i).second << "\n";
    out << "  (" << sizes.count() << " templates in all)\n";

    file.close();
    note(notes, "about.txt");
    return true;
}

bool BugReport::writeCrashRecords(const QString& into, QStringList* notes)
{
#ifdef Q_OS_WIN
    // The Application log's faults (1000) and reports (1001), newest first, as
    // text, then only the blocks about this client.
    QProcess wevtutil;
    wevtutil.start("wevtutil", QStringList()
        << "qe" << "Application"
        << "/q:*[System[(EventID=1000 or EventID=1001)]]"
        << "/rd:true" << QString("/c:%1").arg(CRASH_EVENTS_TO_READ) << "/f:text");

    if (!wevtutil.waitForFinished(TOOL_TIMEOUT_MS))
    {
        wevtutil.kill();
        note(notes, "windows-crash.txt: wevtutil did not answer");
        return false;
    }

    const QString all = QString::fromLocal8Bit(wevtutil.readAllStandardOutput());
    QStringList ours;
    foreach (const QString& block, all.split("\nEvent[", Qt::SkipEmptyParts))
    {
        if (BugReportRules::isClientEvent(block))
            ours.append("Event[" + block.trimmed());
    }

    QFile file(QDir(into).filePath("windows-crash.txt"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    if (ours.isEmpty())
        out << "No Windows crash record for the client among the last " << CRASH_EVENTS_TO_READ
            << " application faults. A client that was closed by hand, or that hung, leaves none.\n";
    else
        out << ours.join("\n\n") << "\n";
    file.close();

    note(notes, ours.isEmpty() ? QString("windows-crash.txt: no crash record for the client")
                               : QString("windows-crash.txt: %1 record(s)").arg(ours.count()));
    return true;
#else
    Q_UNUSED(into);
    note(notes, "windows-crash.txt: not on this platform");
    return false;
#endif
}

bool BugReport::copyWerReport(const QString& into, QStringList* notes)
{
#ifdef Q_OS_WIN
    const QString archive = QString("%1/Microsoft/Windows/WER/ReportArchive").arg(qEnvironmentVariable("ProgramData"));
    QDir dir(archive);
    if (!dir.exists())
    {
        note(notes, "WER report: none");
        return false;
    }

    QFileInfo newest;
    foreach (const QFileInfo& info, dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time))
    {
        if (BugReportRules::isClientCrashFolder(info.fileName()))
        {
            newest = info;
            break;
        }
    }

    if (!newest.exists())
    {
        note(notes, "WER report: none for the client");
        return false;
    }

    if (!copyFolder(newest.absoluteFilePath(), QDir(into).filePath("wer")))
    {
        note(notes, "WER report: found but could not be copied");
        return false;
    }

    note(notes, QString("WER report from %1").arg(newest.lastModified().toString("yyyy-MM-dd HH:mm")));
    return true;
#else
    Q_UNUSED(into);
    Q_UNUSED(notes);
    return false;
#endif
}

bool BugReport::copyFolder(const QString& from, const QString& to)
{
    if (!QDir().mkpath(to))
        return false;

    QDirIterator it(from, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        it.next();
        const QString relative = QDir(from).relativeFilePath(it.filePath());
        const QString target = QDir(to).filePath(relative);
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile::copy(it.filePath(), target);
    }

    return true;
}

QString BugReport::zipFolder(const QString& folder, QStringList* notes)
{
#ifdef Q_OS_WIN
    const QString zip = folder + ".zip";

    QProcess powershell;
    powershell.start("powershell", QStringList()
        << "-NoProfile" << "-NonInteractive" << "-Command"
        << QString("Compress-Archive -Path '%1\\*' -DestinationPath '%2' -Force")
               .arg(QDir::toNativeSeparators(folder), QDir::toNativeSeparators(zip)));

    if (!powershell.waitForFinished(TOOL_TIMEOUT_MS) || powershell.exitCode() != 0 || !QFile::exists(zip))
    {
        powershell.kill();
        note(notes, "not zipped - send the folder");
        return QString();
    }

    note(notes, "zipped");
    return zip;
#else
    Q_UNUSED(folder);
    note(notes, "not zipped - send the folder");
    return QString();
#endif
}

void BugReport::show(QWidget* parent, const QString& versionLine)
{
    QStringList notes;
    QString error;
    const QString result = collect(versionLine, &notes, &error);

    QMessageBox box(parent);
    box.setWindowTitle("Bug Report");

    if (result.isEmpty())
    {
        box.setIcon(QMessageBox::Warning);
        box.setText(QString("Nothing could be collected: %1").arg(error));
        box.exec();
        return;
    }

    box.setIcon(QMessageBox::Information);
    box.setText(QString("Collected to\n%1").arg(QDir::toNativeSeparators(result)));
    box.setInformativeText("Send that file with a line on what you were doing when it happened.\n\n"
                           "Tokens, API keys and passwords were removed from the database copy.\n\n"
                           + notes.join("\n"));

    QPushButton* open = box.addButton("Open Folder", QMessageBox::ActionRole);
    box.addButton(QMessageBox::Close);
    box.exec();

    if (box.clickedButton() == open)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(result).absolutePath()));
}
