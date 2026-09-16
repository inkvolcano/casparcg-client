#pragma once

// Which of the client's own daily log files are old enough to delete.
//
// The client writes one file a day, Client_yyyy-MM-dd.log, and until build 277
// never removed one, so the log folder grew for as long as the machine ran the
// client. The user's decision: keep 30 days.
//
// Only a name that is exactly the client's own pattern, with a real date in it,
// is ever returned. Anything else in the folder - a copy someone renamed, a
// bug-report note, a file from another tool - is left alone, and so is a file
// dated in the future (a clock that was wrong once is not a reason to delete).
//
// tools/test-logretention checks the rules.

#include <QtCore/QDate>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace LogRetention
{
    static const int KEEP_DAYS = 30;

    // The date in a client log file name, or an invalid date if the name is not one.
    inline QDate dateOf(const QString& fileName)
    {
        static const QRegularExpression pattern("^Client_(\\d{4}-\\d{2}-\\d{2})\\.log$");

        const QRegularExpressionMatch match = pattern.match(fileName);
        if (!match.hasMatch())
            return QDate();

        return QDate::fromString(match.captured(1), "yyyy-MM-dd");
    }

    // The names to delete: client logs dated more than keepDays before today.
    // A file from exactly keepDays ago is kept.
    inline QStringList expired(const QStringList& fileNames, const QDate& today, int keepDays = KEEP_DAYS)
    {
        QStringList old;
        if (!today.isValid() || keepDays <= 0)
            return old;

        const QDate oldestKept = today.addDays(-keepDays);
        for (const QString& name : fileNames)
        {
            const QDate date = dateOf(name);
            if (date.isValid() && date < oldestKept)
                old.append(name);
        }

        return old;
    }
}
