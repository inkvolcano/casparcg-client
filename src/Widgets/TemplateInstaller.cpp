#include "TemplateInstaller.h"

#include "DatabaseManager.h"
#include "Models/DeviceModel.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QMap>
#include <QtCore/QSaveFile>
#include <QtCore/QStringList>
#include <QtCore/QSysInfo>

bool TemplateInstaller::isEnabled()
{
    return DatabaseManager::getInstance().getConfigurationByName("TemplatePushEnabled").getValue() == "true";
}

QString TemplateInstaller::token()
{
    return DatabaseManager::getInstance().getConfigurationByName("TemplatePushToken").getValue().trimmed();
}

QString TemplateInstaller::templatesRoot()
{
    QString configured = DatabaseManager::getInstance().getConfigurationByName("TemplatePushPath").getValue().trimmed();
    if (!configured.isEmpty())
        return configured;

    // Nothing configured: the first device that has a template path. On an ordinary
    // single-server setup that is the answer, and there is nothing to fill in.
    foreach (const DeviceModel& device, DatabaseManager::getInstance().getDevice())
    {
        QString path = device.getTemplatePath().trimmed();
        if (!path.isEmpty())
            return path;
    }

    return QString();
}

// Case-insensitive, and only at the root of a pack: a webcg/project.js would be
// template code rather than the connection file the operator owns.
bool TemplateInstaller::isProtected(const QString& relativePath)
{
    QString normalised = QString(relativePath).replace('\\', '/');
    if (normalised.contains('/'))
        return false;

    return normalised.compare("project.js", Qt::CaseInsensitive) == 0
        || normalised.compare("extensions.json", Qt::CaseInsensitive) == 0;
}

bool TemplateInstaller::isSafeSegment(const QString& segment)
{
    if (segment.isEmpty() || segment == "." || segment == "..")
        return false;

    // No separators, no drive letters, no wildcards, nothing a shell or a path
    // parser downstream could read as an instruction.
    foreach (const QChar& c, segment)
    {
        if (c.isLetterOrNumber())
            continue;
        if (c == '.' || c == '-' || c == '_' || c == ' ' || c == '(' || c == ')' || c == '+')
            continue;

        return false;
    }

    // Trailing dots and spaces are stripped by Windows, so "a. " and "a" would be
    // the same file by two names.
    if (segment.endsWith('.') || segment.endsWith(' '))
        return false;

    // These are device names on Windows, with or without an extension: opening
    // "con.html" writes to the console rather than to a file. Refused rather than
    // left for QSaveFile to fail on in a way nobody would read as this.
    static const QStringList DEVICES = QStringList()
        << "CON" << "PRN" << "AUX" << "NUL"
        << "COM1" << "COM2" << "COM3" << "COM4" << "COM5" << "COM6" << "COM7" << "COM8" << "COM9"
        << "LPT1" << "LPT2" << "LPT3" << "LPT4" << "LPT5" << "LPT6" << "LPT7" << "LPT8" << "LPT9";

    return !DEVICES.contains(segment.section('.', 0, 0).toUpper());
}

bool TemplateInstaller::isSafeRelativePath(const QString& relativePath)
{
    QString normalised = QString(relativePath).replace('\\', '/');
    if (normalised.isEmpty() || normalised.startsWith('/'))
        return false;

    QStringList segments = normalised.split('/');
    if (segments.count() > 8)
        return false;   // nothing in a pack is buried that deep

    foreach (const QString& segment, segments)
        if (!isSafeSegment(segment))
            return false;

    return true;
}

QJsonObject TemplateInstaller::listPacks()
{
    QJsonObject result;
    QString root = templatesRoot();
    result.insert("root", root);

    QJsonArray packs;
    QDir directory(root);
    if (!root.isEmpty() && directory.exists())
    {
        foreach (const QString& name, directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        {
            int files = 0;
            qint64 bytes = 0;
            QDateTime newest;

            QDirIterator it(directory.filePath(name), QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext())
            {
                it.next();
                QFileInfo info = it.fileInfo();
                files++;
                bytes += info.size();
                if (!newest.isValid() || info.lastModified() > newest)
                    newest = info.lastModified();
            }

            QJsonObject pack;
            pack.insert("name", name);
            pack.insert("files", files);
            pack.insert("bytes", bytes);
            pack.insert("newest", newest.isValid() ? newest.toUTC().toString(Qt::ISODate) : QString());
            packs.append(pack);
        }
    }

    result.insert("packs", packs);
    return result;
}

// Every file with a digest, so the pusher can send only what differs rather than
// the whole pack every time.
QJsonObject TemplateInstaller::describePack(const QString& pack, bool* found)
{
    if (found != nullptr)
        *found = false;

    QJsonObject result;
    result.insert("name", pack);

    QString root = templatesRoot();
    if (root.isEmpty() || !isSafeSegment(pack))
        return result;

    QDir packDir(QDir(root).filePath(pack));
    if (!packDir.exists())
        return result;

    if (found != nullptr)
        *found = true;

    QJsonArray files;
    QDirIterator it(packDir.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        it.next();
        QFileInfo info = it.fileInfo();
        QString relative = packDir.relativeFilePath(info.absoluteFilePath());

        QJsonObject entry;
        entry.insert("path", relative);
        entry.insert("bytes", info.size());
        entry.insert("protected", isProtected(relative));

        QFile file(info.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly))
        {
            entry.insert("sha1", QString::fromLatin1(
                QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha1).toHex()));
            file.close();
        }

        files.append(entry);
    }

    result.insert("files", files);
    return result;
}

int TemplateInstaller::installFile(const QString& pack, const QString& relativePath,
                                   const QByteArray& body, QString* error)
{
    QString root = templatesRoot();
    if (root.isEmpty())
    {
        if (error != nullptr)
            *error = "No template path is configured and no device has one";

        return 500;
    }

    if (!isSafeSegment(pack))
    {
        if (error != nullptr)
            *error = QString("'%1' is not a usable pack name").arg(pack);

        return 400;
    }

    if (!isSafeRelativePath(relativePath))
    {
        if (error != nullptr)
            *error = QString("'%1' is not a usable path inside a pack").arg(relativePath);

        return 400;
    }

    if (isProtected(relativePath))
    {
        // Not an error the pusher should retry: it is the rule working.
        if (error != nullptr)
            *error = QString("%1 belongs to this machine and is never written by a push").arg(relativePath);

        return 409;
    }

    QDir packDir(QDir(root).filePath(pack));
    QString destination = QDir::cleanPath(packDir.filePath(relativePath));

    // The segment checks should already have made this impossible; it is here
    // because "should" is not a guarantee about a path that came off a socket.
    QString packPrefix = QDir::cleanPath(packDir.absolutePath()) + "/";
    if (!QDir::cleanPath(destination).startsWith(packPrefix))
    {
        if (error != nullptr)
            *error = "That path would land outside the pack";

        return 400;
    }

    QFileInfo info(destination);
    if (!QDir().mkpath(info.absolutePath()))
    {
        if (error != nullptr)
            *error = QString("Cannot create %1").arg(info.absolutePath());

        return 500;
    }

    // Written whole or not at all: a template half-replaced while CasparCG has it
    // open is worse than one that was not updated.
    QSaveFile file(destination);
    if (!file.open(QIODevice::WriteOnly))
    {
        if (error != nullptr)
            *error = QString("Cannot write %1: %2").arg(destination, file.errorString());

        return 500;
    }

    file.write(body);
    if (!file.commit())
    {
        if (error != nullptr)
            *error = QString("Cannot finish writing %1: %2").arg(destination, file.errorString());

        return 500;
    }

    return 200;
}

// ---- who this is, and keeping probes out ----

QJsonObject TemplateInstaller::identify()
{
    QJsonObject info;
    info.insert("application", "CasparCG Client");
    info.insert("host", QSysInfo::machineHostName());
    info.insert("os", QSysInfo::prettyProductName());
    info.insert("templatesRoot", templatesRoot());
    info.insert("pushEnabled", isEnabled());
    info.insert("packs", listPacks().value("packs").toArray().count());
    info.insert("at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    return info;
}

// Deliberately in memory only. A restart clears it, which is the right trade for
// something whose job is to slow a probe down rather than to keep a ledger.
namespace
{
    struct Attempts
    {
        int failures = 0;
        qint64 blockedUntil = 0;
    };

    QMap<QString, Attempts>& attemptsByPeer()
    {
        static QMap<QString, Attempts> attempts;
        return attempts;
    }

    const int MAXIMUM_FAILURES = 5;
    const qint64 BLOCK_MS = 5 * 60 * 1000;
}

bool TemplateInstaller::isThrottled(const QString& peer)
{
    if (!attemptsByPeer().contains(peer))
        return false;

    return attemptsByPeer().value(peer).blockedUntil > QDateTime::currentMSecsSinceEpoch();
}

void TemplateInstaller::noteBadToken(const QString& peer)
{
    Attempts& attempts = attemptsByPeer()[peer];
    attempts.failures++;

    if (attempts.failures >= MAXIMUM_FAILURES)
    {
        attempts.blockedUntil = QDateTime::currentMSecsSinceEpoch() + BLOCK_MS;
        attempts.failures = 0;
    }
}

// One good token clears the count: an operator who mistyped it four times and then
// got it right is not a probe.
void TemplateInstaller::noteGoodToken(const QString& peer)
{
    attemptsByPeer().remove(peer);
}
