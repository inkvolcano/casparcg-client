#pragma once

#include "Shared.h"
#include "SheetsProjectRegistry.h"

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QDateTime>
#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QTimer>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtNetwork/QNetworkAccessManager>

class QNetworkReply;

// One row of a sheet, keyed by the header names above it.
typedef QMap<QString, QString> SheetRow;

// Where a set of rows came from, and when that copy was made. Both are shown in the
// inspector, because "24 rows" is a different fact depending on whether they were
// read a second ago or left over from yesterday.
struct SheetRowsOrigin
{
    enum Source { Memory, Cache, Sheet, Proxy };

    Source source = Sheet;
    QDateTime cachedAt;   // invalid when the source could not say

    // The columns in the order the sheet has them. SheetRow is a QMap and sorts its
    // keys, so without this the inspector would show ID, QUESTION, REFERENCE where
    // the sheet says ID, REFERENCE, QUESTION.
    QStringList columns;

    bool isCached() const { return this->source == Memory || this->source == Cache; }

    QString describe() const
    {
        switch (this->source)
        {
            case Memory:
            case Cache:  return QStringLiteral("cache");
            case Proxy:  return QStringLiteral("proxy");
            default:     return QStringLiteral("live");
        }
    }
};

// What a template says about the sheet it reads:
//
//   window.sheetConnection = { "tab": "LINEUP", "key": "homelineup" };
//
// `tab` is the sheet tab. `key` names the template's own data field whose value
// picks the row \xe2\x80\x94 and that value is an index into the rows, not a column to match
// on, because functions.js resolves it as data[key]. A template that reads no sheet
// declares nothing, and that silence is meaningful: it gets no box rather than an
// empty one.
struct TemplateSheetConnection
{
    // The shape every template carries:
    //
    //   window.sheetConnection = { "tab": "LINEUP", "homelineup": "12", "awaylineup": "12" };
    //
    // `tab` is the sheet tab. Every other property names one of the template's own
    // data fields whose value picks a place in the sheet, and the number is HOW MANY
    // LINES are read from there, the set's own header line included. One line is the
    // common case; the calendar takes six, a lineup side twelve. The documented
    // `"key": "row"` form means one line.
    //
    // A declaration with only a tab is valid: the template reads the whole tab. One
    // that exists but has no tab is `declared` and not `isValid()`, so the inspector
    // can say the declaration is broken instead of quietly showing nothing.
    QString tab;
    QList<QPair<QString, int>> blocks;   // (field name, line count), in declared order
    bool declared = false;

    // How a field's value is turned into a place in the tab. Read from the templates,
    // not invented: every one of them does one of three things.
    //
    //   one line, no `sets`   -> the row whose ID column equals the value, else the
    //                            value as a 1-based position          (findRow)
    //   N lines,  no `sets`   -> base row (value - 1) * N               (getNumberForRow)
    //   `sets` declared       -> the value-th run of member rows, the row above a
    //                            run being its header                  (findTeams)
    //
    // `sets` names the member-row test: "NUMBER:digit" means the NUMBER cell holds a
    // digit, "ID" means the ID cell is filled. It is the one thing a template with
    // variable-length sets has to say, because no count can stand in for it.
    QString setsColumn;
    bool setsDigit = false;

    bool countsSets() const { return !this->setsColumn.isEmpty(); }

    bool isValid() const { return !this->tab.isEmpty(); }
    bool hasBlocks() const { return !this->blocks.isEmpty(); }

    QStringList fields() const
    {
        QStringList names;
        for (const auto& pair : this->blocks)
            names.append(pair.first);
        return names;
    }
};

// One API key's worth of reads over the last minute. The budget is per key, so
// this is the unit a meter should be drawn in.
struct SheetsKeyUsage
{
    QString keyId;      // digest of the key, or "sheet:<id>" when no project owns it
    QString project;    // empty when the spreadsheet belongs to no discovered project
    int clientReads = 0;
    int templateReads = 0;
    int externalReads = 0;

    int total() const { return this->clientReads + this->templateReads + this->externalReads; }
};
class WIDGETS_EXPORT SheetDataResolver : public QObject
{
    Q_OBJECT

    public:
        static SheetDataResolver& getInstance();

        // Parse the three declaration objects out of a template's HTML.

        // The declaration the templates actually carry.
        static TemplateSheetConnection parseConnection(const QString& templateFilePath);
        TemplateSheetConnection connectionFor(const QString& deviceName, const QString& templateName);

        // Drop the remembered declaration so the file is read again. Without this a
        // template corrected while the client runs is not seen until a restart.
        void forgetConnection(const QString& deviceName, const QString& templateName);

        // Absolute path of a template's HTML, or empty when it cannot be located.
        static QString templateFilePath(const QString& deviceName, const QString& templateName);

        // Fetch every row of a tab. Answers with rowsReady or rowsFailed, tagged with
        // the same requestId so several requests cannot be confused for one another.
        // forceReload skips the short-lived memory copy, for when somebody asked for a
        // refresh and means it.
        void fetchRows(const SheetsProject& project, const QString& tab, const QString& requestId,
                       bool forceReload = false);

        // Write rows into the cache so templates find them warm. Never reports failure:
        // warming is best effort by definition.
        void warmCache(const SheetsProject& project, const QString& tab, const QList<SheetRow>& rows);

        // ---- demand-driven warming ----
        // Nothing is warmed on a schedule. A tab is refreshed because something asked
        // for it — a template went to air, the inspector resolved a row, the cache came
        // back empty — and then only if what we hold has gone stale. That keeps the
        // traffic proportional to the show instead of to the clock.
        void noteDemand(const SheetsProject& project, const QString& tab);

        // What has been asked for and when it was last refreshed, for the report.
        int warmQueueLength() const;

        // Pick the row whose key column carries this value.

        static QString cacheServiceUrl();

        // ---- quota strain ----
        // Sixty reads a minute is shared by everything using the project's key, and
        // most of it is spent by graphics rather than by this client. Calls made here
        // are counted exactly; template calls are estimated from the plays we fire,
        // since a sheet-driven template reads the sheet every time it renders.
        void noteTemplatePlayed(const QString& deviceName, const QString& templateName);

        // A template saying it just read the sheet. Counted rather than inferred, so
        // where this arrives it replaces the guess made from plays instead of being
        // added to it — the same read must not appear twice.
        void noteTemplateRead(const QString& spreadsheetId, const QString& source);
        // Reports the API key closest to its own limit, not the sum across keys:
        // two projects on separate keys each spending half a budget are not one
        // budget fully spent. busiestProject names whose key that is, for the
        // meter to say so.
        // Every key with reads in the last minute, busiest first.
        QList<SheetsKeyUsage> quotaUsageByKey() const;

        void quotaUsage(int& clientCalls, int& templateCalls, int* externalCalls = nullptr,
                        QString* busiestProject = nullptr) const;
        static int quotaLimit();

        // Strain seen from outside, in both directions. The client publishes what it
        // knows, and accepts what other applications know, so whichever end does the
        // adding up has the whole picture rather than its own corner of it.
        QJsonObject strainReport() const;
        static QString strainReportUrl();

        // A read this client made outside the resolver \xe2\x80\x94 the Sheets panel fetches the
        // API directly, and a read is a read whoever in here made it.
        void noteClientRead(const QString& spreadsheetId);

        // A report from another application \xe2\x80\x94 a connector, a second client, anything
        // spending the same key. Counted as read, never as instruction.
        bool acceptExternalReport(const QJsonObject& report, QString* error = nullptr);

    Q_SIGNALS:
        void rowsReady(const QString& requestId, const QList<SheetRow>& rows, const SheetRowsOrigin& origin);
        void rowsFailed(const QString& requestId, const QString& reason);

    private:
        explicit SheetDataResolver();

        // What another application last told us it was spending, keyed by who said so.
        // Held as a snapshot with its arrival time rather than as a running tally,
        // because a reporter that goes away should fade out rather than linger.
        struct ExternalReport
        {
            QString source;
            QString host;
            qint64 receivedAt = 0;
            int windowSeconds = 60;
            QMap<QString, int> reads;   // spreadsheetId -> reads in that window
        };


        void requestFromApi(const SheetsProject& project, const QString& tab, const QString& requestId);

        // Warming reads through the keyless proxy the templates already name in
        // project.js, so a refresh costs nothing against the sixty-a-minute budget.
        // Falls back to the API only when there is no proxy and there is headroom.
        void warmNow(const SheetsProject& project, const QString& tab);
        void warmFromProxy(const SheetsProject& project, const QString& tab);

        Q_SLOT void processWarmQueue();

        static QString warmKey(const QString& spreadsheetId, const QString& tab);
        static int warmStaleSeconds();
        static int warmSpacingMs();

        // The cache holds rows already shaped as objects; the API hands back a values
        // grid. Both are normalised here so nothing downstream can tell them apart.
        static QList<SheetRow> rowsFromCacheJson(const QByteArray& body, QStringList* columns = nullptr);
        static QList<SheetRow> rowsFromApiJson(const QByteArray& body, QStringList* columns = nullptr);

        // When the answering cache says it wrote this copy, or an invalid time when it
        // did not say. The PHP service does not; the client's own does.
        static QDateTime cachedAtOf(QNetworkReply* reply);

        void recordApiCall(const QString& spreadsheetId);
        static void pruneWindow(QList<qint64>& stamps);
        static int countInWindow(const QMap<QString, QList<qint64>>& stamps, const QString& spreadsheetId, qint64 cutoff);
        QStringList knownSpreadsheetIds() const;
        QStringList externalSpreadsheetIds() const;

        // spreadsheetId -> the key fingerprint it is read with, and the project name.
        // A sheet whose project cannot be resolved is its own group, so it is still
        // counted rather than silently folded in with somebody else's budget.
        static QString budgetGroupFor(const QString& spreadsheetId, QString* projectName = nullptr);
        int externalReadsFor(const QString& spreadsheetId) const;
        static bool isReportFresh(const ExternalReport& report);

        static constexpr qint64 EXTERNAL_REPORT_STALE_MS = 90000;
        static constexpr int MAXIMUM_EXTERNAL_SOURCES = 64;
        static constexpr int MAXIMUM_SHEETS_PER_REPORT = 64;

        QNetworkAccessManager* networkManager;

        QMap<QString, TemplateSheetConnection> connectionCache;   // device|template -> connection
        // Counted per spreadsheet, because the budget belongs to the key rather than
        // to the client — an aggregator needs to know which pool it is adding to.
        QMap<QString, QList<qint64>> clientCallStamps;     // spreadsheetId -> our API reads
        QMap<QString, QList<qint64>> templatePlayStamps;   // spreadsheetId -> plays of sheet templates
        QMap<QString, QList<qint64>> templateReadStamps;   // spreadsheetId -> reads templates reported
        QMap<QString, QList<qint64>> proxyReadStamps;      // spreadsheetId -> reads through the proxy (free)

        QMap<QString, ExternalReport> externalReports;   // "source|host" -> what it said

        // spreadsheetId|tab -> when it was last refreshed. Absent means never.
        QMap<QString, qint64> warmedAt;
        QList<QPair<SheetsProject, QString>> warmQueue;   // project + tab, in the order asked for
        QTimer* warmTimer = nullptr;

        // Rows held briefly in memory so picking through a list of them does not
        // become a request per keystroke.
        // spreadsheetId|tab -> what we hold and where it came from. The origin is kept
        // so a memory hit still reports the age of the underlying copy rather than the
        // age of our own copy of it.
        struct CachedRows
        {
            qint64 takenAt = 0;
            QList<SheetRow> rows;
            SheetRowsOrigin origin;
        };

        QMap<QString, CachedRows> rowCache;

        QTimer* reportTimer = nullptr;

        Q_SLOT void publishStrain();
};
