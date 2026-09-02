#include "SheetsProjectRegistry.h"

#include "DatabaseManager.h"
#include "Models/DeviceModel.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QTextStream>

SheetsProjectRegistry::SheetsProjectRegistry()
    : QObject()
{
}

SheetsProjectRegistry& SheetsProjectRegistry::getInstance()
{
    static SheetsProjectRegistry instance;
    return instance;
}

void SheetsProjectRegistry::discover()
{
    this->discovered.clear();

    foreach (const DeviceModel& deviceModel, DatabaseManager::getInstance().getDevice())
    {
        QString templatePath = deviceModel.getTemplatePath();
        if (templatePath.isEmpty())
            continue;

        QStringList candidates;
        candidates.append(templatePath);
        QDir templateDir(templatePath);
        foreach (const QString& sub, templateDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            candidates.append(templateDir.filePath(sub));

        foreach (const QString& folder, candidates)
        {
            QFile file(QDir(folder).filePath("project.js"));
            if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QString content = QTextStream(&file).readAll();
            file.close();

            QRegularExpression idRegex("spreadsheetId\\s*=\\s*[\"']([^\"']+)[\"']");
            QRegularExpression keyRegex("apiKey\\s*=\\s*[\"']([^\"']+)[\"']");
            QRegularExpressionMatch idMatch = idRegex.match(content);
            QRegularExpressionMatch keyMatch = keyRegex.match(content);
            if (!idMatch.hasMatch() || !keyMatch.hasMatch())
                continue;

            SheetsProject project;
            project.name = QDir(folder).dirName();
            project.folder = folder;
            project.spreadsheetId = idMatch.captured(1);
            project.apiKey = keyMatch.captured(1);
            project.deviceName = deviceModel.getName();

            project.usesLocalCache = readLocalFlag(folder);

            QRegularExpression proxyRegex("mainurl\\s*=\\s*[\"']([^\"']+)[\"']");
            QRegularExpressionMatch proxyMatch = proxyRegex.match(content);
            if (proxyMatch.hasMatch())
                project.proxyUrl = proxyMatch.captured(1);

            // Avoid duplicates when multiple devices share a template path.
            bool known = false;
            foreach (const SheetsProject& existing, this->discovered)
                if (existing.folder == project.folder) { known = true; break; }
            if (!known)
                this->discovered.append(project);
        }
    }
}

const QList<SheetsProject>& SheetsProjectRegistry::projects() const
{
    return this->discovered;
}

SheetsProject SheetsProjectRegistry::projectByName(const QString& name) const
{
    foreach (const SheetsProject& project, this->discovered)
        if (project.name == name)
            return project;

    return SheetsProject();
}

SheetsProject SheetsProjectRegistry::projectForTemplate(const QString& templateName) const
{
    // "SEVILLE/lineup" -> "SEVILLE". A template sitting directly in a device's
    // template path has no project folder and therefore no sheet connection.
    int separator = templateName.indexOf('/');
    if (separator <= 0)
        return SheetsProject();

    return projectByName(templateName.left(separator));
}

// One assignment, matched the same way whether it is const, let, var or bare.
static QRegularExpression localFlagRegex()
{
    return QRegularExpression("(\\blocal\\s*=\\s*)(true|false)");
}

bool SheetsProjectRegistry::readLocalFlag(const QString& projectFolder, bool* found)
{
    if (found != nullptr)
        *found = false;

    QFile file(QDir(projectFolder).filePath("project.js"));
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QString content = QTextStream(&file).readAll();
    file.close();

    QRegularExpressionMatch match = localFlagRegex().match(content);
    if (!match.hasMatch())
        return false;

    if (found != nullptr)
        *found = true;

    return match.captured(2) == "true";
}

// Only the value is replaced. Everything else \xe2\x80\x94 comments, spacing, line endings,
// the keyword in front of it \xe2\x80\x94 is left untouched, because this is a file the
// operator maintains and we are a guest in it.
bool SheetsProjectRegistry::writeLocalFlag(const QString& projectFolder, bool useLocalCache, QString* error)
{
    QString path = QDir(projectFolder).filePath("project.js");

    QFile file(path);
    if (!file.exists())
    {
        if (error != nullptr)
            *error = QString("No project.js in %1").arg(projectFolder);

        return false;
    }

    if (!file.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
            *error = QString("Cannot read %1").arg(path);

        return false;
    }

    QByteArray raw = file.readAll();
    file.close();

    QString content = QString::fromUtf8(raw);
    QRegularExpressionMatch match = localFlagRegex().match(content);
    if (!match.hasMatch())
    {
        // Nothing is appended: a project without the flag is not one we understand,
        // and inventing a declaration could collide with one written elsewhere.
        if (error != nullptr)
            *error = QString("No 'local = true/false' found in %1").arg(path);

        return false;
    }

    QString updated = content;
    updated.replace(match.capturedStart(2), match.capturedLength(2), useLocalCache ? "true" : "false");

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (error != nullptr)
            *error = QString("Cannot write %1").arg(path);

        return false;
    }

    file.write(updated.toUtf8());
    file.close();

    for (int i = 0; i < this->discovered.count(); i++)
        if (this->discovered.at(i).folder == projectFolder)
            this->discovered[i].usesLocalCache = useLocalCache;

    return true;
}
