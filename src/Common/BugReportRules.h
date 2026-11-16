#pragma once

#include <QtCore/QDate>
#include <QtCore/QDateTime>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <algorithm>

// What a bug report contains and what it must never contain.
//
// Help -> Collect a Bug Report gathers the things an operator was asked for by
// hand after every crash: the log, a copy of the database, the Windows crash
// record, the newest WER report, and a page about the build and its settings.
// The database is the dangerous part - it holds the relay token, the push
// token, the update token and every Sheets API key - so the copy is redacted
// before it is written anywhere an operator will zip and email. The rules for
// that are here, on their own, so they can be tested on names and not on a
// live database.
namespace BugReportRules
{
    // A setting whose value is a secret. Matched on the name, the way the
    // redaction SQL matches, so the two cannot disagree. "Key" on its own is
    // not enough - HotkeyPreview is a keyboard shortcut - so it is ApiKey.
    inline bool isSecretSetting(const QString& name)
    {
        const QString lower = name.toLower();
        return lower.contains("token") || lower.contains("apikey")
            || lower.contains("password") || lower.contains("secret");
    }

    // Run on the COPY, never on the live database.
    inline QStringList redactionStatements()
    {
        return QStringList()
            << "UPDATE Configuration SET Value = '<redacted>' WHERE Name LIKE '%Token%' "
               "OR Name LIKE '%ApiKey%' OR Name LIKE '%Password%' OR Name LIKE '%Secret%'"
            << "UPDATE Device SET Username = '', Password = ''";
    }

    // Today's log and yesterday's: a crash just after midnight is in the one
    // that closed a minute ago.
    inline QStringList logFileNames(const QDate& today)
    {
        return QStringList()
            << QString("Client_%1.log").arg(today.toString("yyyy-MM-dd"))
            << QString("Client_%1.log").arg(today.addDays(-1).toString("yyyy-MM-dd"));
    }

    inline QString reportFolderName(const QDateTime& when)
    {
        return QString("casparcg-bugreport-%1").arg(when.toString("yyyyMMdd-HHmm"));
    }

    // The folders Windows Error Reporting keeps for this client.
    inline bool isClientCrashFolder(const QString& folderName)
    {
        return folderName.startsWith("AppCrash_casparcg-client", Qt::CaseInsensitive);
    }

    // Whether one wevtutil event block is about this client. The Application
    // log holds every program's faults; only ours travel.
    inline bool isClientEvent(const QString& eventText)
    {
        return eventText.contains("casparcg", Qt::CaseInsensitive);
    }

    // The crash dumps Windows writes for this client in %LOCALAPPDATA%\CrashDumps:
    // "casparcg-client.exe.14952.dmp". With the release's .pdb they give the
    // exact line - that is how build 289's startup crash was found.
    inline bool isClientDump(const QString& fileName)
    {
        return fileName.startsWith("casparcg-client.exe.", Qt::CaseInsensitive)
            && fileName.endsWith(".dmp", Qt::CaseInsensitive);
    }

    struct DumpFile
    {
        QString name;
        qint64 size = 0;
        QDateTime modified;
    };

    const int DUMPS_KEPT = 3;
    const int DUMP_MAX_DAYS = 14;
    const qint64 DUMP_MAX_BYTES = 300LL * 1024 * 1024;

    // Which dumps go in a report: the client's, from the last two weeks, newest
    // first, at most three, none over 300 MB (a full dump of a client with a
    // big library can be larger than anyone will email). The rest are named in
    // `skipped` with the reason.
    inline QList<DumpFile> chooseDumps(QList<DumpFile> files, const QDateTime& now, QStringList* skipped = nullptr)
    {
        std::sort(files.begin(), files.end(), [](const DumpFile& a, const DumpFile& b) { return a.modified > b.modified; });

        QList<DumpFile> chosen;
        for (const DumpFile& file : files)
        {
            if (!isClientDump(file.name))
                continue;
            if (file.modified.isValid() && file.modified.daysTo(now) > DUMP_MAX_DAYS)
                continue;
            if (file.size > DUMP_MAX_BYTES)
            {
                if (skipped != nullptr)
                    skipped->append(QString("%1 (%2 MB, too big to send)").arg(file.name).arg(file.size / (1024 * 1024)));
                continue;
            }
            if (chosen.count() >= DUMPS_KEPT)
            {
                if (skipped != nullptr)
                    skipped->append(QString("%1 (older; the newest %2 are in)").arg(file.name).arg(DUMPS_KEPT));
                continue;
            }
            chosen.append(file);
        }
        return chosen;
    }
}
