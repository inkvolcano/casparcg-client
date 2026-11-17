#pragma once

// Watch folders (build 317): a group that watches a folder inside a server's
// media and gets an item for every new file in it. The rules here decide what
// is in the folder, what is new to the group and what has gone; WatchFolders
// (Widgets) does the asking and the adding. tools/test-watchfolder.
//
// Names are the server's media names - "INCOMING/INTERVIEW_01" - compared
// without regard to case, since the scanner reports them in capitals and an
// operator types a folder however they like.

#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <algorithm>

namespace WatchFolderRules
{
    struct Media
    {
        QString name;
        QString type;       // MOVIE, STILL or AUDIO; anything else is not added
        QString timecode;
    };

    // "incoming\", "/Incoming/" and "INCOMING" are the same folder.
    inline QString normaliseFolder(QString folder)
    {
        folder = folder.trimmed();
        folder.replace('\\', '/');
        while (folder.startsWith('/'))
            folder.remove(0, 1);
        while (folder.endsWith('/'))
            folder.chop(1);
        return folder.toUpper();
    }

    // Anywhere under the folder, subfolders included.
    inline bool inFolder(const QString& mediaName, const QString& folder)
    {
        const QString f = normaliseFolder(folder);
        if (f.isEmpty())
            return false;
        return mediaName.toUpper().startsWith(f + "/");
    }

    inline bool playable(const QString& type)
    {
        return type == "MOVIE" || type == "STILL" || type == "AUDIO";
    }

    // What the group gets now: files in the folder it has not had before, in
    // name order. Something it once added and the operator deleted is in
    // `seen`, so it does not come back.
    inline QList<Media> fresh(const QList<Media>& all, const QString& folder, const QStringList& seen)
    {
        QSet<QString> had;
        for (const QString& name : seen)
            had.insert(name.toUpper());

        QList<Media> found;
        for (const Media& media : all)
        {
            if (playable(media.type) && inFolder(media.name, folder) && !had.contains(media.name.toUpper()))
            {
                found.append(media);
                had.insert(media.name.toUpper());
            }
        }
        std::sort(found.begin(), found.end(), [](const Media& a, const Media& b) {
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });
        return found;
    }

    // The group's items whose file is no longer in the folder (for the missing
    // media marks; the items stay).
    inline QStringList gone(const QList<Media>& all, const QString& folder, const QStringList& itemNames)
    {
        QSet<QString> present;
        for (const Media& media : all)
            present.insert(media.name.toUpper());

        QStringList missing;
        for (const QString& name : itemNames)
        {
            if (inFolder(name, folder) && !present.contains(name.toUpper()))
                missing.append(name);
        }
        return missing;
    }

    // Every folder the server's media names show, nested ones included, for the
    // Inspector's choice: "A/B/CLIP" gives "A" and "A/B".
    inline QStringList foldersOf(const QStringList& mediaNames)
    {
        QSet<QString> folders;
        for (const QString& name : mediaNames)
        {
            const QStringList parts = name.toUpper().split('/', Qt::SkipEmptyParts);
            QString path;
            for (int i = 0; i + 1 < parts.count(); i++)
            {
                path = path.isEmpty() ? parts.at(i) : path + "/" + parts.at(i);
                folders.insert(path);
            }
        }
        QStringList sorted(folders.begin(), folders.end());
        sorted.sort();
        return sorted;
    }

    // What the group has had, as it is saved with the rundown: one name a line.
    inline QString joinSeen(const QStringList& seen)
    {
        return seen.join('\n');
    }

    inline QStringList splitSeen(const QString& text)
    {
        QStringList names;
        for (const QString& line : text.split('\n', Qt::SkipEmptyParts))
        {
            const QString name = line.trimmed().toUpper();
            if (!name.isEmpty() && !names.contains(name))
                names.append(name);
        }
        return names;
    }
}
