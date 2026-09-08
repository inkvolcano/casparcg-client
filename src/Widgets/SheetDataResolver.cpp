#include "SheetDataResolver.h"

#include <algorithm>

#include "DatabaseManager.h"
#include "Models/ConfigurationModel.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtCore/QSysInfo>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

SheetDataResolver::SheetDataResolver()
    : QObject()
{
    this->networkManager = new QNetworkAccessManager(this);

    // Publish strain outward on a slow tick. Costs nothing when no destination is
    // configured, which is the default.
    this->reportTimer = new QTimer(this);
    this->reportTimer->setInterval(10000);
    QObject::connect(this->reportTimer, &QTimer::timeout, this, &SheetDataResolver::publishStrain);
    this->reportTimer->start();

    // Not a schedule: this only ever runs while something is waiting to be warmed,
    // and it exists to space those out rather than to make them happen.
    this->warmTimer = new QTimer(this);
    this->warmTimer->setSingleShot(true);
    QObject::connect(this->warmTimer, &QTimer::timeout, this, &SheetDataResolver::processWarmQueue);
}

SheetDataResolver& SheetDataResolver::getInstance()
{
    static SheetDataResolver instance;
    return instance;
}

QString SheetDataResolver::cacheServiceUrl()
{
    QString configured = DatabaseManager::getInstance().getConfigurationByName("SheetsCacheUrl").getValue().trimmed();
    if (!configured.isEmpty())
        return configured;

    // When the client hosts the cache itself there is nothing to configure: it knows
    // its own port, and going through the socket rather than the file keeps one set of
    // rules about bypass and about what counts as a read.
    if (DatabaseManager::getInstance().getConfigurationByName("SheetsHostCache").getValue() == "true")
    {
        QString port = DatabaseManager::getInstance().getConfigurationByName("SheetsHostCachePort").getValue();
        return QString("http://localhost:%1/cache").arg(port.toInt() > 0 ? port.toInt() : 3000);
    }

    return "http://localhost:3000/local_server.php";
}


QString SheetDataResolver::templateFilePath(const QString& deviceName, const QString& templateName)
{
    if (deviceName.isEmpty() || templateName.isEmpty())
        return QString();

    QString templatePath = DatabaseManager::getInstance().getDeviceByName(deviceName).getTemplatePath();
    if (templatePath.isEmpty())
        return QString();

    QString path = QDir(templatePath).filePath(templateName + ".html");
    return QFile::exists(path) ? path : QString();
}

QList<SheetRow> SheetDataResolver::rowsFromCacheJson(const QByteArray& body, QStringList* columns)
{
    QList<SheetRow> rows;

    QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isArray())
        return rows;   // an error object, or something we did not write

    // QJsonObject hands keys back sorted, so the order the sheet has them in has to
    // be read off the text: the first object's keys, in the order they were written.
    // Whoever wrote the cache built each object from the header row, in order.
    if (columns != nullptr)
    {
        columns->clear();
        int open = body.indexOf('{');
        int close = (open >= 0) ? body.indexOf('}', open) : -1;
        if (open >= 0 && close > open)
        {
            QRegularExpression keyRegex("\"([^\"]+)\"\\s*:");
            QRegularExpressionMatchIterator it = keyRegex.globalMatch(QString::fromUtf8(body.mid(open, close - open)));
            while (it.hasNext())
                columns->append(it.next().captured(1));
        }
    }

    foreach (const QJsonValue& value, document.array())
    {
        if (!value.isObject())
            continue;

        QJsonObject object = value.toObject();
        SheetRow row;
        foreach (const QString& column, object.keys())
            row[column] = object.value(column).toVariant().toString();

        rows.append(row);
    }

    return rows;
}

QList<SheetRow> SheetDataResolver::rowsFromApiJson(const QByteArray& body, QStringList* columns)
{
    QList<SheetRow> rows;

    QJsonDocument document = QJsonDocument::fromJson(body);
    QJsonArray values = document.object().value("values").toArray();
    if (values.count() < 2)
        return rows;   // header only, or nothing at all

    QStringList headers;
    foreach (const QJsonValue& header, values.at(0).toArray())
        headers.append(header.toVariant().toString());

    if (columns != nullptr)
    {
        columns->clear();
        foreach (const QString& header, headers)
            if (!header.isEmpty())
                columns->append(header);
    }

    for (int i = 1; i < values.count(); i++)
    {
        QJsonArray cells = values.at(i).toArray();
        SheetRow row;
        for (int column = 0; column < headers.count(); column++)
        {
            if (headers[column].isEmpty())
                continue;
            row[headers[column]] = (column < cells.count()) ? cells.at(column).toVariant().toString() : QString();
        }
        rows.append(row);
    }

    return rows;
}

void SheetDataResolver::fetchRows(const SheetsProject& project, const QString& tab, const QString& requestId, bool forceReload)
{
    if (!project.isValid() || tab.isEmpty())
    {
        emit rowsFailed(requestId, "No sheet project for this template");
        return;
    }

    // Somebody wants this tab, which is the only reason anything gets warmed.
    noteDemand(project, tab);

    // Answered from memory when it was read a moment ago, so walking a list of rows
    // does not become a request apiece. Deferred so callers always hear back after
    // they return, exactly as they would from the network.
    QString key = warmKey(project.spreadsheetId, tab);
    if (!forceReload
        && this->rowCache.contains(key)
        && QDateTime::currentMSecsSinceEpoch() - this->rowCache.value(key).takenAt < 10000)
    {
        CachedRows held = this->rowCache.value(key);
        QTimer::singleShot(0, this, [this, requestId, held]() {
            emit rowsReady(requestId, held.rows, held.origin);
        });

        return;
    }

    QString url = QString("%1?spreadsheetId=%2&sheetNumber=%3")
        .arg(cacheServiceUrl(), project.spreadsheetId, QString(QUrl::toPercentEncoding(tab)));

    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::UserAgentHeader, "CasparCG-Client");

    QNetworkReply* reply = this->networkManager->get(request);
    SheetsProject captured = project;
    QString capturedTab = tab;

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, captured, capturedTab, requestId]() {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QList<SheetRow> rows;
        QStringList columns;
        if (reply->error() == QNetworkReply::NoError && status == 200)
            rows = rowsFromCacheJson(reply->readAll(), &columns);

        if (!rows.isEmpty())
        {
            SheetRowsOrigin origin;
            origin.source = SheetRowsOrigin::Cache;
            origin.cachedAt = cachedAtOf(reply);
            origin.columns = columns;

            CachedRows held;
            held.takenAt = QDateTime::currentMSecsSinceEpoch();
            held.rows = rows;
            held.origin = origin;
            this->rowCache.insert(warmKey(captured.spreadsheetId, capturedTab), held);

            emit rowsReady(requestId, rows, origin);
            return;
        }

        // A miss, a bypass, or no service at all — all of them mean the same thing
        // here, which is that the sheet itself has to answer.
        requestFromApi(captured, capturedTab, requestId);
    });
}

void SheetDataResolver::requestFromApi(const SheetsProject& project, const QString& tab, const QString& requestId)
{
    recordApiCall(project.spreadsheetId);

    QString url = QString("https://sheets.googleapis.com/v4/spreadsheets/%1/values/%2?key=%3")
        .arg(project.spreadsheetId, QString(QUrl::toPercentEncoding(tab)), project.apiKey);

    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::UserAgentHeader, "CasparCG-Client");

    QNetworkReply* reply = this->networkManager->get(request);
    SheetsProject captured = project;
    QString capturedTab = tab;

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, captured, capturedTab, requestId]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
        {
            emit rowsFailed(requestId, QString("Sheet unreachable: %1").arg(reply->errorString()));
            return;
        }

        QStringList columns;
        QList<SheetRow> rows = rowsFromApiJson(reply->readAll(), &columns);
        if (rows.isEmpty())
        {
            emit rowsFailed(requestId, QString("Tab '%1' returned no rows").arg(capturedTab));
            return;
        }

        // The call has been made and paid for either way, so leave the cache warmer
        // than we found it.
        warmCache(captured, capturedTab, rows);

        // Straight from the sheet, so it is as fresh as it gets.
        SheetRowsOrigin origin;
        origin.source = SheetRowsOrigin::Sheet;
        origin.cachedAt = QDateTime::currentDateTime();
        origin.columns = columns;

        CachedRows held;
        held.takenAt = QDateTime::currentMSecsSinceEpoch();
        held.rows = rows;
        held.origin = origin;
        this->rowCache.insert(warmKey(captured.spreadsheetId, capturedTab), held);

        emit rowsReady(requestId, rows, origin);
    });
}

void SheetDataResolver::warmCache(const SheetsProject& project, const QString& tab, const QList<SheetRow>& rows)
{
    if (!project.isValid() || tab.isEmpty() || rows.isEmpty())
        return;

    // Written in the shape the templates expect: an array of row objects keyed by
    // the header names. Anything else would corrupt the cache for them.
    QJsonArray array;
    foreach (const SheetRow& row, rows)
    {
        QJsonObject object;
        foreach (const QString& column, row.keys())
            object.insert(column, row.value(column));
        array.append(object);
    }

    // via=client so a cache that counts writes as reads does not count this one: it
    // follows a read already counted here.
    QString url = QString("%1?spreadsheetId=%2&sheetNumber=%3&via=client")
        .arg(cacheServiceUrl(), project.spreadsheetId, QString(QUrl::toPercentEncoding(tab)));

    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "CasparCG-Client");

    QNetworkReply* reply = this->networkManager->post(request, QJsonDocument(array).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

// ---- quota strain ----

int SheetDataResolver::quotaLimit()
{
    QString configured = DatabaseManager::getInstance().getConfigurationByName("SheetsQuotaPerMinute").getValue();
    int limit = configured.toInt();
    return (limit > 0) ? limit : 60;
}

void SheetDataResolver::pruneWindow(QList<qint64>& stamps)
{
    qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - 60000;
    while (!stamps.isEmpty() && stamps.first() < cutoff)
        stamps.removeFirst();
}

void SheetDataResolver::recordApiCall(const QString& spreadsheetId)
{
    QList<qint64>& stamps = this->clientCallStamps[spreadsheetId];
    stamps.append(QDateTime::currentMSecsSinceEpoch());
    pruneWindow(stamps);
}

// A sheet-driven template reads the sheet itself every time it renders, so a play is
// a call we cannot see but can account for. Only templates already known to declare a
// source are counted — this deliberately does not touch the disk while something is
// going to air, so the estimate warms up as templates are used rather than guessing.
void SheetDataResolver::noteTemplatePlayed(const QString& deviceName, const QString& templateName)
{
    QString key = deviceName + "|" + templateName;
    if (!this->connectionCache.contains(key) || !this->connectionCache.value(key).isValid())
        return;

    SheetsProject project = SheetsProjectRegistry::getInstance().projectForTemplate(templateName);
    if (!project.isValid())
        return;

    QList<qint64>& stamps = this->templatePlayStamps[project.spreadsheetId];
    stamps.append(QDateTime::currentMSecsSinceEpoch());
    pruneWindow(stamps);

    // What just went to air is what is most likely to go to air again, so this is
    // the moment its tab is worth refreshing — for the next play, not for this one.
    noteDemand(project, this->connectionCache.value(key).tab);
}

void SheetDataResolver::noteClientRead(const QString& spreadsheetId)
{
    if (spreadsheetId.isEmpty())
        return;

    recordApiCall(spreadsheetId);
}

void SheetDataResolver::noteTemplateRead(const QString& spreadsheetId, const QString& source)
{
    Q_UNUSED(source);

    if (spreadsheetId.isEmpty())
        return;

    QList<qint64>& stamps = this->templateReadStamps[spreadsheetId];
    stamps.append(QDateTime::currentMSecsSinceEpoch());
    pruneWindow(stamps);
}

// Counts inside a window, for one spreadsheet. Reads a template reported for itself
// are the truth; plays are only a stand-in for when it has not.
int SheetDataResolver::countInWindow(const QMap<QString, QList<qint64>>& stamps, const QString& spreadsheetId, qint64 cutoff)
{
    int count = 0;
    foreach (qint64 stamp, stamps.value(spreadsheetId))
        if (stamp >= cutoff)
            count++;

    return count;
}

QStringList SheetDataResolver::knownSpreadsheetIds() const
{
    QStringList ids;
    foreach (const QString& id, this->clientCallStamps.keys())
        ids.append(id);
    foreach (const QString& id, this->templatePlayStamps.keys())
        if (!ids.contains(id))
            ids.append(id);
    foreach (const QString& id, this->templateReadStamps.keys())
        if (!ids.contains(id))
            ids.append(id);
    foreach (const QString& id, this->proxyReadStamps.keys())
        if (!ids.contains(id))
            ids.append(id);
    foreach (const QString& id, externalSpreadsheetIds())
        if (!ids.contains(id))
            ids.append(id);

    return ids;
}

QString SheetDataResolver::budgetGroupFor(const QString& spreadsheetId, QString* projectName)
{
    SheetsProject project = SheetsProjectRegistry::getInstance().projectBySpreadsheetId(spreadsheetId);

    if (projectName != nullptr)
        *projectName = project.name;

    QString fingerprint = project.keyFingerprint();

    // No project, or a project with no key: keep it separate under its own id so it
    // is never added to a budget it does not draw on.
    return fingerprint.isEmpty() ? ("sheet:" + spreadsheetId) : fingerprint;
}

QList<SheetsKeyUsage> SheetDataResolver::quotaUsageByKey() const
{
    qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - 60000;

    QMap<QString, SheetsKeyUsage> perKey;

    foreach (const QString& id, knownSpreadsheetIds())
    {
        QString name;
        QString group = budgetGroupFor(id, &name);

        SheetsKeyUsage& usage = perKey[group];
        usage.keyId = group;
        if (usage.project.isEmpty())
            usage.project = name;

        usage.clientReads += countInWindow(this->clientCallStamps, id, cutoff);

        // A read a template reported for itself is the truth; a play is only a
        // stand-in for when it has not.
        int counted = countInWindow(this->templateReadStamps, id, cutoff);
        usage.templateReads += (counted > 0) ? counted
                                             : countInWindow(this->templatePlayStamps, id, cutoff);

        usage.externalReads += externalReadsFor(id);
    }

    QList<SheetsKeyUsage> result;
    foreach (const SheetsKeyUsage& usage, perKey)
        if (usage.total() > 0)
            result.append(usage);

    // Busiest first, so whoever draws this puts the key nearest its limit on top.
    std::sort(result.begin(), result.end(), [](const SheetsKeyUsage& a, const SheetsKeyUsage& b) {
        return a.total() > b.total();
    });

    return result;
}

void SheetDataResolver::quotaUsage(int& clientCalls, int& templateCalls, int* externalCalls,
                                   QString* busiestProject) const
{
    clientCalls = 0;
    templateCalls = 0;
    if (externalCalls != nullptr)
        *externalCalls = 0;
    if (busiestProject != nullptr)
        busiestProject->clear();

    QList<SheetsKeyUsage> perKey = quotaUsageByKey();
    if (perKey.isEmpty())
        return;

    // Already sorted busiest first.
    const SheetsKeyUsage& worst = perKey.first();
    clientCalls = worst.clientReads;
    templateCalls = worst.templateReads;
    if (externalCalls != nullptr)
        *externalCalls = worst.externalReads;
    if (busiestProject != nullptr)
        *busiestProject = worst.project;
}



// A report is only worth counting while it still describes now. One that stopped
// arriving fades out instead of holding a number up forever.
bool SheetDataResolver::isReportFresh(const ExternalReport& report)
{
    return QDateTime::currentMSecsSinceEpoch() - report.receivedAt <= EXTERNAL_REPORT_STALE_MS;
}

QStringList SheetDataResolver::externalSpreadsheetIds() const
{
    QStringList ids;
    foreach (const ExternalReport& report, this->externalReports)
    {
        if (!isReportFresh(report))
            continue;

        foreach (const QString& id, report.reads.keys())
            if (!ids.contains(id))
                ids.append(id);
    }

    return ids;
}

int SheetDataResolver::externalReadsFor(const QString& spreadsheetId) const
{
    int total = 0;
    foreach (const ExternalReport& report, this->externalReports)
        if (isReportFresh(report))
            total += report.reads.value(spreadsheetId, 0);

    return total;
}

// Everything in here is somebody else's text. It is read for numbers and identity
// and nothing else \xe2\x80\x94 no field of it reaches a file path, a URL or a command.
bool SheetDataResolver::acceptExternalReport(const QJsonObject& report, QString* error)
{
    QJsonArray sheets = report.value("sheets").toArray();
    if (sheets.isEmpty())
    {
        if (error != nullptr)
            *error = "Expected a sheets array with at least one entry";

        return false;
    }

    ExternalReport entry;
    entry.source = report.value("source").toString().left(64);
    entry.host = report.value("host").toString().left(64);
    entry.receivedAt = QDateTime::currentMSecsSinceEpoch();

    if (entry.source.isEmpty())
        entry.source = "unknown";
    if (entry.host.isEmpty())
        entry.host = "unknown";

    int window = report.value("windowSeconds").toInt(60);
    entry.windowSeconds = (window > 0) ? window : 60;

    for (int i = 0; i < sheets.count() && i < MAXIMUM_SHEETS_PER_REPORT; i++)
    {
        QJsonObject sheet = sheets.at(i).toObject();
        QString spreadsheetId = sheet.value("spreadsheetId").toString().left(128);
        if (spreadsheetId.isEmpty())
            continue;

        // "reads" is the plain way to say it. The client's own report is also
        // accepted verbatim, which is what lets one client report to another.
        int reads = 0;
        if (sheet.contains("reads"))
            reads = sheet.value("reads").toInt();
        else
            reads = sheet.value("clientReads").toInt() + sheet.value("templateReads").toInt();

        if (reads > 0)
            entry.reads.insert(spreadsheetId, reads);
    }

    if (entry.reads.isEmpty())
    {
        if (error != nullptr)
            *error = "No sheet entry carried a spreadsheetId and a read count";

        return false;
    }

    QString key = entry.source + "|" + entry.host;

    // A reporter that keeps changing its name must not grow this without bound;
    // when it is full the stalest entry makes room.
    if (!this->externalReports.contains(key) && this->externalReports.count() >= MAXIMUM_EXTERNAL_SOURCES)
    {
        QString oldest;
        qint64 oldestAt = 0;
        foreach (const QString& candidate, this->externalReports.keys())
        {
            qint64 at = this->externalReports.value(candidate).receivedAt;
            if (oldest.isEmpty() || at < oldestAt)
            {
                oldest = candidate;
                oldestAt = at;
            }
        }

        this->externalReports.remove(oldest);
    }

    this->externalReports.insert(key, entry);
    return true;
}

// ---- publishing strain outward ----

QString SheetDataResolver::strainReportUrl()
{
    return DatabaseManager::getInstance().getConfigurationByName("SheetsStrainReportUrl").getValue();
}

// What this client knows, said plainly: its own reads are counted, template reads are
// inferred from plays, and both are per spreadsheet because that is what the budget
// belongs to. Whoever collects these can add them to what other clients and the
// templates themselves report and arrive at a real total.
QJsonObject SheetDataResolver::strainReport() const
{
    qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - 60000;

    QJsonArray sheets;
    foreach (const QString& id, knownSpreadsheetIds())
    {
        int clientReads = countInWindow(this->clientCallStamps, id, cutoff);
        int countedReads = countInWindow(this->templateReadStamps, id, cutoff);
        int playReads = countInWindow(this->templatePlayStamps, id, cutoff);

        int proxyReads = countInWindow(this->proxyReadStamps, id, cutoff);
        int externalReads = externalReadsFor(id);

        int templateReads = (countedReads > 0) ? countedReads : playReads;
        if (clientReads == 0 && templateReads == 0 && proxyReads == 0 && externalReads == 0)
            continue;

        QJsonObject entry;
        QString projectName;
        QString group = budgetGroupFor(id, &projectName);

        entry.insert("spreadsheetId", id);
        entry.insert("project", projectName);
        // Which budget this draws on. A digest of the API key, never the key: enough
        // to group sheets that share a budget, useless to anyone who lacks it.
        entry.insert("keyId", group);
        entry.insert("clientReads", clientReads);
        entry.insert("templateReads", templateReads);
        entry.insert("templateReadsAre", (countedReads > 0) ? "counted" : "estimated");
        entry.insert("templateReadsCounted", countedReads);
        entry.insert("templateReadsEstimated", playReads);
        // Other applications spending the same key. Added into the total, and kept
        // separate as well so it is clear which part of it we did not see ourselves.
        entry.insert("externalReads", externalReads);
        entry.insert("total", clientReads + templateReads + externalReads);
        // Warming reads go through the keyless proxy, so they are traffic without
        // being budget. Reported separately rather than hidden or added in.
        entry.insert("proxyReads", proxyReads);
        sheets.append(entry);
    }

    QJsonObject report;
    report.insert("source", "casparcg-client");
    report.insert("host", QSysInfo::machineHostName());
    report.insert("windowSeconds", 60);
    report.insert("limitPerMinute", quotaLimit());
    report.insert("at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    report.insert("sheets", sheets);

    // Said out loud so a collector does not mistake the estimate for a count.
    report.insert("warmQueue", warmQueueLength());

    // Who else is reporting, and how long ago, so a total can be trusted or not.
    QJsonArray reporters;
    foreach (const ExternalReport& external, this->externalReports)
    {
        QJsonObject who;
        who.insert("source", external.source);
        who.insert("host", external.host);
        who.insert("ageSeconds", (QDateTime::currentMSecsSinceEpoch() - external.receivedAt) / 1000.0);
        who.insert("stale", !isReportFresh(external));
        reporters.append(who);
    }
    report.insert("reporters", reporters);
    report.insert("note", "clientReads are exact; templateReads are counted where a template reported them and otherwise estimated from plays; proxyReads cost nothing against the limit");

    return report;
}

void SheetDataResolver::publishStrain()
{
    QString url = strainReportUrl();
    if (url.isEmpty())
        return;

    QJsonObject report = strainReport();
    if (report.value("sheets").toArray().isEmpty())
        return;   // nothing happened; stay quiet

    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "CasparCG-Client");

    QNetworkReply* reply = this->networkManager->post(request, QJsonDocument(report).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}


// ---- demand-driven warming ----

QString SheetDataResolver::warmKey(const QString& spreadsheetId, const QString& tab)
{
    return spreadsheetId + "|" + tab;
}

int SheetDataResolver::warmStaleSeconds()
{
    QString configured = DatabaseManager::getInstance().getConfigurationByName("SheetsWarmStaleSeconds").getValue();
    int seconds = configured.toInt();
    return (seconds > 0) ? seconds : 30;
}

int SheetDataResolver::warmSpacingMs()
{
    QString configured = DatabaseManager::getInstance().getConfigurationByName("SheetsWarmSpacingMs").getValue();
    int spacing = configured.toInt();
    return (spacing > 0) ? spacing : 1500;
}

int SheetDataResolver::warmQueueLength() const
{
    return this->warmQueue.count();
}

// Demand is the whole trigger. What arrives here is "this tab is being used"; the
// decision about whether that is worth a read belongs below, not at the call sites.
void SheetDataResolver::noteDemand(const SheetsProject& project, const QString& tab)
{
    if (!project.isValid() || tab.isEmpty())
        return;

    QString key = warmKey(project.spreadsheetId, tab);
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // Still fresh, so the demand is already satisfied.
    if (this->warmedAt.contains(key) && now - this->warmedAt.value(key) < warmStaleSeconds() * 1000LL)
        return;

    for (int i = 0; i < this->warmQueue.count(); i++)
    {
        const QPair<SheetsProject, QString>& queued = this->warmQueue.at(i);
        if (queued.first.spreadsheetId == project.spreadsheetId && queued.second == tab)
            return;   // already waiting its turn
    }

    this->warmQueue.append(qMakePair(project, tab));

    if (!this->warmTimer->isActive())
        this->warmTimer->start(0);
}

void SheetDataResolver::processWarmQueue()
{
    if (this->warmQueue.isEmpty())
        return;

    QPair<SheetsProject, QString> next = this->warmQueue.takeFirst();
    warmNow(next.first, next.second);

    // Spaced out on purpose: a rundown full of sheet templates should trickle, not
    // arrive as a burst that a rate limit would answer for us.
    if (!this->warmQueue.isEmpty())
        this->warmTimer->start(warmSpacingMs());
}

void SheetDataResolver::warmNow(const SheetsProject& project, const QString& tab)
{
    // Marked before the read rather than after, so a slow or failing refresh cannot
    // queue itself again and again while it is still in flight.
    this->warmedAt.insert(warmKey(project.spreadsheetId, tab), QDateTime::currentMSecsSinceEpoch());

    if (!project.proxyUrl.trimmed().isEmpty())
    {
        warmFromProxy(project, tab);
        return;
    }

    // No proxy to read through, so this would have to come out of the budget. Only
    // worth it while there is room to spare; the show's own reads come first.
    int clientCalls = 0;
    int templateCalls = 0;
    quotaUsage(clientCalls, templateCalls);

    if (clientCalls + templateCalls >= quotaLimit() / 2)
        return;

    requestFromApi(project, tab, QString("warm|%1|%2").arg(project.spreadsheetId, tab));
}

// The proxy hands back an array of header-keyed row objects, which is already the
// shape the cache holds, so this is a copy rather than a conversion.
void SheetDataResolver::warmFromProxy(const SheetsProject& project, const QString& tab)
{
    QString base = project.proxyUrl.trimmed();
    if (!base.endsWith('/'))
        base += '/';

    QString url = QString("%1%2/%3").arg(base, project.spreadsheetId, QString(QUrl::toPercentEncoding(tab)));

    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::UserAgentHeader, "CasparCG-Client");

    QNetworkReply* reply = this->networkManager->get(request);
    SheetsProject captured = project;
    QString capturedTab = tab;

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, captured, capturedTab]() {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status != 200)
        {
            // Let it be asked for again shortly rather than pretending it is fresh.
            this->warmedAt.remove(warmKey(captured.spreadsheetId, capturedTab));
            return;
        }

        QByteArray body = reply->readAll();
        QStringList columns;
        QList<SheetRow> rows = rowsFromCacheJson(body, &columns);
        if (rows.isEmpty())
        {
            this->warmedAt.remove(warmKey(captured.spreadsheetId, capturedTab));
            return;
        }

        // Free of the budget, but still a read somebody made, so it is reported.
        QList<qint64>& stamps = this->proxyReadStamps[captured.spreadsheetId];
        stamps.append(QDateTime::currentMSecsSinceEpoch());
        pruneWindow(stamps);

        SheetRowsOrigin origin;
        origin.source = SheetRowsOrigin::Proxy;
        origin.cachedAt = QDateTime::currentDateTime();
        origin.columns = columns;

        CachedRows held;
        held.takenAt = QDateTime::currentMSecsSinceEpoch();
        held.rows = rows;
        held.origin = origin;
        this->rowCache.insert(warmKey(captured.spreadsheetId, capturedTab), held);

        warmCache(captured, capturedTab, rows);
    });
}


// ---- the template's own declaration ----

TemplateSheetConnection SheetDataResolver::parseConnection(const QString& templateFilePath)
{
    TemplateSheetConnection connection;

    QFile file(templateFilePath);
    if (templateFilePath.isEmpty() || !file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        return connection;

    QString content = QTextStream(&file).readAll();
    file.close();

    // Read the same way the other declarations are read: the object is flat, inline
    // and quoted, so it can be taken without running any of the template.
    QRegularExpression objectRegex("window\\.sheetConnection\\s*=\\s*\\{([^}]*)\\}");
    QRegularExpressionMatch objectMatch = objectRegex.match(content);
    if (!objectMatch.hasMatch())
        return connection;

    connection.declared = true;

    // Every "name": "value" pair. `tab` is the tab, `key` is the documented
    // single-field form, and anything else is a field with its default.
    QRegularExpression pairRegex("[\"'](\\w+)[\"']\\s*:\\s*[\"']([^\"']*)[\"']");
    QRegularExpressionMatchIterator it = pairRegex.globalMatch(objectMatch.captured(1));
    while (it.hasNext())
    {
        QRegularExpressionMatch pair = it.next();
        QString name = pair.captured(1).trimmed();
        QString value = pair.captured(2).trimmed();

        if (name == "tab")
        {
            connection.tab = value;
        }
        else if (name == "key")
        {
            // The documented single-field form: one line at that field.
            connection.blocks.append(qMakePair(value, 1));
        }
        else if (name == "sets")
        {
            // "COLUMN" or "COLUMN:digit" - the test that makes a row a set member.
            int colon = value.indexOf(':');
            connection.setsColumn = (colon >= 0) ? value.left(colon).trimmed() : value;
            connection.setsDigit = (colon >= 0) && value.mid(colon + 1).trimmed().toLower() == "digit";
        }
        else
        {
            // A line count; anything that is not a positive number reads as one.
            bool numeric = false;
            int lines = value.toInt(&numeric);
            connection.blocks.append(qMakePair(name, (numeric && lines > 0) ? lines : 1));
        }
    }

    return connection;
}

void SheetDataResolver::forgetConnection(const QString& deviceName, const QString& templateName)
{
    this->connectionCache.remove(deviceName + "|" + templateName);
}
TemplateSheetConnection SheetDataResolver::connectionFor(const QString& deviceName, const QString& templateName)
{
    QString key = deviceName + "|" + templateName;
    if (this->connectionCache.contains(key))
        return this->connectionCache.value(key);

    TemplateSheetConnection connection = parseConnection(templateFilePath(deviceName, templateName));
    this->connectionCache.insert(key, connection);

    return connection;
}


// Read from the answering cache. The client's own service sends both an ISO stamp and
// the standard header; the PHP one sends neither, and an unknown age is reported as
// unknown rather than guessed at.
QDateTime SheetDataResolver::cachedAtOf(QNetworkReply* reply)
{
    if (reply == nullptr)
        return QDateTime();

    QByteArray stamp = reply->rawHeader("X-Sheet-Cached-At");
    if (!stamp.isEmpty())
    {
        QDateTime parsed = QDateTime::fromString(QString::fromLatin1(stamp), Qt::ISODate);
        if (parsed.isValid())
            return parsed.toLocalTime();
    }

    QVariant lastModified = reply->header(QNetworkRequest::LastModifiedHeader);
    if (lastModified.isValid())
        return lastModified.toDateTime().toLocalTime();

    return QDateTime();
}
