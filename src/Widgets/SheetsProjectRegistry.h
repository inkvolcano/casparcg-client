#pragma once

#include "Shared.h"

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

// A project = a template folder containing a project.js (spreadsheetId + apiKey),
// the same connection file the standalone HTML templates use.
struct SheetsProject
{
    QString name;           // folder name, e.g. "SEVILLE"
    QString folder;         // absolute path to the project folder
    QString spreadsheetId;
    QString apiKey;
    QString deviceName;     // device whose template path contains this project
    // The keyless JSON proxy the templates' project.js names, used for warming so a
    // refresh costs nothing against the API budget.
    QString proxyUrl;

    // The project's own `local` flag. It decides where its templates look for the
    // cache: false is a relative path, true is localhost:3000 \xe2\x80\x94 which is where
    // this client listens when it hosts the cache.
    bool usesLocalCache = false;

    bool isValid() const { return !this->spreadsheetId.isEmpty(); }

    // The per-minute budget belongs to the API key, not to the spreadsheet, so
    // strain has to be grouped by key. The key itself is a secret and is never
    // published; this short digest is enough to group by and useless to anyone
    // who does not already hold the key.
    QString keyFingerprint() const;
};

// Single place that knows which sheet projects exist. Both the Sheets panel and
// the template data resolver read from here, so a project is discovered once and
// described the same way everywhere.
class WIDGETS_EXPORT SheetsProjectRegistry : public QObject
{
    Q_OBJECT

    public:
        static SheetsProjectRegistry& getInstance();

        // Rescan every device's template path (and its direct subfolders) for project.js.
        void discover();

        const QList<SheetsProject>& projects() const;
        SheetsProject projectByName(const QString& name) const;

        // A template is addressed as "PROJECT/template", and a project's name is its
        // folder name — so the first path segment identifies the project without
        // anything having to be configured.
        SheetsProject projectForTemplate(const QString& templateName) const;

        // Read and write the `local` flag in a project's project.js. Writing rewrites
        // that one assignment and leaves the rest of the file exactly as it was; it is
        // the operator's source, not ours.
        static bool readLocalFlag(const QString& projectFolder, bool* found = nullptr);
        bool writeLocalFlag(const QString& projectFolder, bool useLocalCache, QString* error = nullptr);

        // The API key a project reads with, in the project's own project.js. Written
        // the same way the local flag is: the one value, nothing else disturbed.
        static QString readApiKey(const QString& projectFolder);
        bool writeApiKey(const QString& projectFolder, const QString& apiKey, QString* error = nullptr);

        SheetsProject projectBySpreadsheetId(const QString& spreadsheetId) const;

    private:
        explicit SheetsProjectRegistry();

        QList<SheetsProject> discovered;
};
