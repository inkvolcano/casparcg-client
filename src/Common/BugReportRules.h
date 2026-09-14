#pragma once

#include <QtCore/QDate>
#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QStringList>

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
}
