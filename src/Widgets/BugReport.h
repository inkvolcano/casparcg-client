#pragma once

#include "Shared.h"

#include <QtCore/QString>
#include <QtCore/QStringList>

class QWidget;

// Help -> Collect a Bug Report.
//
// Everything an operator was asked to gather by hand after a crash, gathered
// by the client itself into one folder on the desktop and zipped: the log for
// today and yesterday, a redacted copy of the database, the Windows crash
// records for this client, the newest Windows Error Reporting folder for it,
// and about.txt - the build, Qt, Windows, the machine, and the settings that
// decide what the client does on a click. Nothing is sent anywhere; the
// operator sends the zip.
class WIDGETS_EXPORT BugReport
{
    public:
        // Collects into a new folder under the desktop. Returns the zip path, or
        // the folder path when zipping was not possible; empty with a reason in
        // error when nothing could be written at all. notes gets one line per
        // thing collected or skipped, for the dialog.
        static QString collect(const QString& versionLine, QStringList* notes, QString* error);

        // Collects and tells the operator where it went, with a button that opens
        // the folder.
        static void show(QWidget* parent, const QString& versionLine);

    private:
        static QString dataFolder();
        static bool copyLogs(const QString& into, QStringList* notes);
        static bool copyDatabase(const QString& into, QStringList* notes);
        static bool writeAbout(const QString& into, const QString& versionLine, QStringList* notes);
        static bool writeCrashRecords(const QString& into, QStringList* notes);
        static bool copyWerReport(const QString& into, QStringList* notes);
        static QString zipFolder(const QString& folder, QStringList* notes);
        static bool copyFolder(const QString& from, const QString& to);
};
