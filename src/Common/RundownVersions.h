#pragma once

// The last saves of a rundown, kept beside it (build 300).
//
// Each save that changes the file keeps what was there before, in a folder next
// to the rundown named after it:
//
//   Show.xml
//   Show.xml versions\Show 2026-11-16 143205.xml
//
// The newest ten are kept; File > Rundown Versions... lists them to open or put
// back. Only files named the way this writes them are ever counted or removed -
// anything else someone puts in that folder is left alone. Autosave covers a
// crash; this covers an edit that should not have been saved. Tested by
// tools/test-rundownversions.

#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>
#include <QtCore/QList>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <algorithm>

namespace RundownVersions
{
    const int KEEP = 10;

    inline QString folderFor(const QString& rundownPath)
    {
        return rundownPath + " versions";
    }

    inline QString baseName(const QString& rundownPath)
    {
        return QFileInfo(rundownPath).completeBaseName();
    }

    inline QString versionName(const QString& rundownPath, const QDateTime& when)
    {
        return QString("%1 %2.xml").arg(baseName(rundownPath), when.toString("yyyy-MM-dd HHmmss"));
    }

    // When a version file was saved, from its name; invalid when it is not one
    // of this rundown's versions.
    inline QDateTime versionTime(const QString& rundownPath, const QString& fileName)
    {
        const QString prefix = baseName(rundownPath) + " ";
        if (!fileName.startsWith(prefix) || !fileName.endsWith(".xml", Qt::CaseInsensitive))
            return QDateTime();
        const QString stamp = fileName.mid(prefix.length(), fileName.length() - prefix.length() - 4);
        if (!QRegularExpression("^\\d{4}-\\d\\d-\\d\\d \\d{6}$").match(stamp).hasMatch())
            return QDateTime();
        return QDateTime::fromString(stamp, "yyyy-MM-dd HHmmss");
    }

    // This rundown's versions among a folder's file names, newest first.
    inline QStringList versionsAmong(const QString& rundownPath, const QStringList& fileNames)
    {
        QStringList versions;
        for (const QString& name : fileNames)
        {
            if (versionTime(rundownPath, name).isValid())
                versions << name;
        }
        std::sort(versions.begin(), versions.end(), [&rundownPath](const QString& a, const QString& b) {
            return versionTime(rundownPath, a) > versionTime(rundownPath, b);
        });
        return versions;
    }

    // The versions past the newest ten, to be removed.
    inline QStringList beyondKept(const QString& rundownPath, const QStringList& fileNames, int keep = KEEP)
    {
        const QStringList versions = versionsAmong(rundownPath, fileNames);
        return versions.mid(qMin(keep, versions.count()));
    }
}
