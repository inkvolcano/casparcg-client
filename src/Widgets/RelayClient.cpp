#include "RelayClient.h"

#include "CheckInTarget.h"

#include "DatabaseManager.h"
#include "TemplateInstaller.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMap>
#include <QtCore/QPair>
#include <QtCore/QTimer>
#include <QtCore/QSysInfo>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace
{
    // A single poll should never turn into an afternoon of downloads. This is far
    // above any real pack and low enough that a misconfigured relay is noticed as a
    // refusal rather than as an hour of traffic.
    const int MAXIMUM_FILES_PER_POLL = 500;

    // An INACTIVITY timeout, not a deadline. Measured and confirmed against Qt
    // 6.5.3: a transfer that keeps delivering bytes runs as long as it needs, and
    // one that stops delivering is cut at exactly this interval.
    //
    // So this does not need raising for a large template on a slow link, and
    // raising it would only mean waiting longer to notice a dead connection.
    const int TIMEOUT_MS = 30000;

    // Two more goes after the first. A venue link that drops a request is the
    // ordinary case over the internet, not a reason to wait out the whole interval
    // before trying that file again.
    const int MAXIMUM_ATTEMPTS = 2;
}

// A refusal will be the same refusal next time. A connection that went away, or a
// host having a moment, will not be.
bool RelayClient::worthRetrying(int httpStatus)
{
    return httpStatus == 0 || httpStatus >= 500;
}

RelayClient& RelayClient::getInstance()
{
    static RelayClient instance;
    return instance;
}

RelayClient::RelayClient()
    : QObject(nullptr)
{
    this->network = new QNetworkAccessManager(this);
    this->network->setTransferTimeout(TIMEOUT_MS);

    this->timer = new QTimer(this);
    QObject::connect(this->timer, &QTimer::timeout, this, [this]() { checkNow(); });
}

// ---- settings ----

bool RelayClient::isEnabled()
{
    return DatabaseManager::getInstance().getConfigurationByName("RelayEnabled").getValue() == "true";
}

QString RelayClient::url()
{
    return DatabaseManager::getInstance().getConfigurationByName("RelayUrl").getValue().trimmed();
}

QString RelayClient::token()
{
    return DatabaseManager::getInstance().getConfigurationByName("RelayToken").getValue().trimmed();
}

// Where this machine reports, when that is not simply where it pulls from. The
// case this exists for is a venue pulling templates from GitHub: its token there
// is read-only and must stay that way, so it reports to a relay instead.
QString RelayClient::checkInUrl()
{
    return DatabaseManager::getInstance().getConfigurationByName("RelayCheckInUrl").getValue().trimmed();
}

QString RelayClient::checkInToken()
{
    return DatabaseManager::getInstance().getConfigurationByName("RelayCheckInToken").getValue().trimmed();
}

int RelayClient::pollMinutes()
{
    int minutes = DatabaseManager::getInstance().getConfigurationByName("RelayPollMinutes").getValue().toInt();
    return minutes > 0 ? minutes : 15;
}

QStringList RelayClient::packFilter()
{
    QString raw = DatabaseManager::getInstance().getConfigurationByName("RelayPacks").getValue().trimmed();
    if (raw.isEmpty())
        return QStringList();

    QStringList packs;
    foreach (const QString& name, raw.split(',', Qt::SkipEmptyParts))
        packs.append(name.trimmed());

    return packs;
}

// ---- which kind of source is configured ----

bool RelayClient::isGitHub()
{
    return url().startsWith("github:", Qt::CaseInsensitive);
}

// "github:owner/repo@branch" -> "owner/repo"
QString RelayClient::gitHubOwnerRepo()
{
    if (!isGitHub())
        return QString();

    QString rest = url().mid(QString("github:").length()).trimmed();
    int at = rest.indexOf('@');
    if (at >= 0)
        rest = rest.left(at);

    while (rest.endsWith('/'))
        rest.chop(1);

    return rest;
}

// Empty means whatever the repository calls its default branch. Asking for HEAD
// gets that without a second request to find out its name.
QString RelayClient::gitHubBranch()
{
    if (!isGitHub())
        return QString();

    QString rest = url().mid(QString("github:").length()).trimmed();
    int at = rest.indexOf('@');

    return (at >= 0) ? rest.mid(at + 1).trimmed() : QString();
}

// A GitHub Enterprise Server answers the same API at its own address, usually
// https://github.example.com/api/v3. Without somewhere to put that, a client on one
// could not reach its own repositories at all.
//
// There is no field for this in the settings, deliberately: almost nobody needs it,
// and an empty value does the right thing. It is a database setting, and GITHUB.md
// says where to put it.
QString RelayClient::gitHubApi()
{
    QString configured = DatabaseManager::getInstance()
        .getConfigurationByName("RelayGitHubApi").getValue().trimmed();

    while (configured.endsWith('/'))
        configured.chop(1);

    return configured.isEmpty() ? QString("https://api.github.com") : configured;
}

QString RelayClient::sourceLabel()
{
    if (!isGitHub())
        return url();

    QString branch = gitHubBranch();
    return branch.isEmpty() ? QString("github.com/%1").arg(gitHubOwnerRepo())
                            : QString("github.com/%1 (%2)").arg(gitHubOwnerRepo(), branch);
}

// GitHub wants a bearer token, a stated API version and a user agent; it refuses
// requests without the last of those. A relay wants one header of its own.
void RelayClient::authorise(QNetworkRequest& request) const
{
    if (isGitHub())
    {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + token().toUtf8());
        request.setRawHeader("Accept", "application/vnd.github+json");
        request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
        request.setRawHeader("User-Agent", "CasparCG-Client");
        return;
    }

    request.setRawHeader("X-Relay-Token", token().toUtf8());
}

// What the source says this machine should have.
//
// Keyed on machine name, matched without regard to case because nobody types a
// hostname the same way twice. "*" is the answer for a machine that is not named,
// which is how a whole estate gets a shared pack without listing every box.
QStringList RelayClient::assignedPacks(const QJsonObject& assignments)
{
    QStringList packs;
    if (assignments.isEmpty())
        return packs;

    QString me = QSysInfo::machineHostName();

    QJsonValue mine;
    foreach (const QString& key, assignments.keys())
    {
        if (key.compare(me, Qt::CaseInsensitive) == 0)
        {
            mine = assignments.value(key);
            break;
        }
    }

    if (mine.isUndefined() || mine.isNull())
        mine = assignments.value("*");

    if (!mine.isArray())
        return packs;

    foreach (const QJsonValue& value, mine.toArray())
    {
        QString name = value.toString().trimmed();
        if (!name.isEmpty())
            packs.append(name);
    }

    return packs;
}

bool RelayClient::packsDecidedLocally()
{
    return DatabaseManager::getInstance()
        .getConfigurationByName("RelayPacksLocal").getValue() == "true";
}

// The source decides when it has an opinion about this machine; the local setting
// decides when it does not, and whenever this machine has been told to ignore it.
//
// A source that names this machine with an empty list is an opinion: it means this
// machine takes nothing. That is different from saying nothing, and the two must not
// collapse into each other, because an empty filter means "every pack" further down.
QStringList RelayClient::packsForThisMachine(const QJsonObject& assignments)
{
    // The way out, for the one machine that needs to differ from whatever the estate
    // was told. Deliberately a decision made at the machine, because that is where
    // somebody is standing when they need it.
    if (packsDecidedLocally())
        return packFilter();

    QString me = QSysInfo::machineHostName();

    bool named = assignments.contains("*");
    foreach (const QString& key, assignments.keys())
    {
        if (key.compare(me, Qt::CaseInsensitive) == 0)
            named = true;
    }

    if (named)
    {
        QStringList assigned = assignedPacks(assignments);

        // Named with nothing is a real instruction, and the only way to say it here
        // is a name no pack will ever have.
        return assigned.isEmpty() ? QStringList("\x01none") : assigned;
    }

    return packFilter();
}

// The operator may paste the URL with a query already on it, or without. Both
// should work rather than one of them silently doing nothing.
QString RelayClient::endpoint(const QString& action)
{
    QString base = url();
    if (base.isEmpty())
        return QString();

    return base + (base.contains('?') ? "&action=" : "?action=") + action;
}

// ---- running ----

void RelayClient::start()
{
    this->timer->stop();

    if (!isEnabled() || url().isEmpty() || token().isEmpty())
        return;

    this->timer->start(pollMinutes() * 60 * 1000);

    // A client that has just been switched on is the one most likely to be behind,
    // so ask once now rather than waiting out the first interval.
    QTimer::singleShot(5000, this, [this]() { checkNow(); });
}

void RelayClient::stop()
{
    this->timer->stop();
}

void RelayClient::say(const QString& line)
{
    emit progress(line);
}

bool RelayClient::checkNow()
{
    if (this->busy)
        return false;

    if (url().isEmpty() || token().isEmpty())
    {
        // An empty token would be sent as an empty header and refused, which is a
        // worse way to learn this than being told.
        say("No address or no token, nothing to poll.");
        return false;
    }

    if (TemplateInstaller::templatesRoot().isEmpty())
    {
        say("No template folder is configured on this machine.");
        return false;
    }

    this->busy = true;
    this->installed = 0;
    this->failed = 0;
    this->queue.clear();
    this->manifestAttempts = 0;
    this->versionByPack.clear();
    this->packsWithFailures.clear();
    this->gitHubAssignments = QJsonObject();

    requestManifest();
    return true;
}

void RelayClient::ping()
{
    if (url().isEmpty() || token().isEmpty())
    {
        say("No address or no token.");
        return;
    }

    // For GitHub the same question is "does this token open this repository", which
    // the repository endpoint answers without touching any file.
    QString target = isGitHub()
        ? QString("%1/repos/%2").arg(gitHubApi(), gitHubOwnerRepo())
        : endpoint("ping");

    QNetworkRequest request((QUrl(target)));
    authorise(request);

    bool github = isGitHub();

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, github]() {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200)
        {
            // GitHub answers 404 rather than 403 for a private repository a token
            // cannot see, so saying "or the token cannot see it" is the honest reading.
            QString why = (status == 401) ? "the token was refused"
                        : (status == 404 && github) ? "no such repository, or this token cannot see it"
                        : (status == 403) ? "refused, which on GitHub usually means the rate limit"
                        : reply->errorString();

            say(QString("Source: %1").arg(why));
            return;
        }

        QJsonObject info = QJsonDocument::fromJson(reply->readAll()).object();

        if (github)
        {
            say(QString("GitHub: reached %1, default branch %2, %3.")
                .arg(info.value("full_name").toString(),
                     info.value("default_branch").toString(),
                     info.value("private").toBool() ? "private" : "PUBLIC \xE2\x80\x94 templates here are visible to anyone"));
            return;
        }

        say(QString("Relay: reached \"%1\", %2 pack(s) available.")
            .arg(info.value("relay").toString(), QString::number(info.value("packs").toInt())));
    });
}

void RelayClient::requestManifest()
{
    say(isGitHub() ? "GitHub: reading the tree" : "Relay: asking what is there");

    // One request lists every file in the repository with a Git blob digest each,
    // which is the whole manifest in a single call.
    QString branch = gitHubBranch().isEmpty() ? QString("HEAD") : gitHubBranch();
    QString target = isGitHub()
        ? QString("%1/repos/%2/git/trees/%3?recursive=1")
            .arg(gitHubApi(), gitHubOwnerRepo(), QString::fromUtf8(QUrl::toPercentEncoding(branch)))
        : endpoint("manifest");

    QNetworkRequest request((QUrl(target)));
    authorise(request);

    bool github = isGitHub();

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, github]() {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200)
        {
            // The next poll may be a quarter of an hour away, so a blip here is worth
            // a second look rather than a wasted interval.
            if (worthRetrying(status) && this->manifestAttempts < MAXIMUM_ATTEMPTS)
            {
                int delay = ++this->manifestAttempts * 2000;
                say(QString("Relay: no answer, trying again in %1s").arg(delay / 1000));
                QTimer::singleShot(delay, this, [this]() { requestManifest(); });
                return;
            }

            QString why = (status == 401) ? "wrong or missing token"
                        : (status == 404 && github) ? "no such repository or branch, or this token cannot see it"
                        : (status == 403 && github) ? "refused by GitHub, usually the rate limit"
                        : QString("could not read the manifest (%1)").arg(reply->errorString());

            done(why);
            return;
        }

        if (github)
        {
            // A repository has nowhere to put this but a file, so it is read out of
            // the tree before anything is planned against it.
            QByteArray tree = reply->readAll();
            QString assignmentsBlob;

            foreach (const QJsonValue& value,
                     QJsonDocument::fromJson(tree).object().value("tree").toArray())
            {
                QJsonObject entry = value.toObject();
                if (entry.value("type").toString() == "blob"
                    && entry.value("path").toString().compare("assignments.json", Qt::CaseInsensitive) == 0)
                {
                    assignmentsBlob = entry.value("sha").toString();
                    break;
                }
            }

            if (!assignmentsBlob.isEmpty())
                fetchGitHubAssignments(tree, assignmentsBlob);
            else
                planFromGitHubTree(tree);
        }
        else
        {
            planFrom(reply->readAll());
        }
    });
}

// One extra request, and only when the repository actually carries the file.
//
// A failure here is not a reason to stop: an unreadable assignment file should leave
// the client on whatever it was set to locally rather than pulling nothing, so the
// tree is planned either way.
void RelayClient::fetchGitHubAssignments(const QByteArray& treeJson, const QString& blobSha)
{
    QNetworkRequest request((QUrl(QString("%1/repos/%2/git/blobs/%3")
        .arg(gitHubApi(), gitHubOwnerRepo(), blobSha))));
    authorise(request);
    request.setRawHeader("Accept", "application/vnd.github.raw");

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, treeJson]() {
        reply->deleteLater();

        this->gitHubAssignments = QJsonObject();

        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200)
        {
            QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
            if (document.isObject())
                this->gitHubAssignments = document.object();
            else
                say("  assignments.json is not a JSON object; using the local pack list");
        }
        else
        {
            say("  could not read assignments.json; using the local pack list");
        }

        planFromGitHubTree(treeJson);
    });
}

// The same job as planFrom, against a GitHub tree instead of a relay manifest.
//
// A tree is flat: every file in the repository with its full path. The first path
// segment is the pack, and the rest is where the file sits inside it. The digests
// are Git blob hashes, so they are compared against local files hashed the same
// way rather than against a plain sha1.
void RelayClient::planFromGitHubTree(const QByteArray& treeJson)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(treeJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        done("GitHub did not answer with a tree");
        return;
    }

    QJsonObject root = document.object();
    if (root.value("truncated").toBool())
    {
        // Not something to work around quietly: a truncated tree would look exactly
        // like a repository that is missing files, and this would delete nothing but
        // would report "up to date" while being wrong.
        done("the repository is too large for one tree listing, so this cannot tell what is missing");
        return;
    }

    QStringList wantedPacks = packsForThisMachine(this->gitHubAssignments);
    int skippedPacks = 0;

    // Group by pack first, so each pack's local digests are read once.
    QMap<QString, QList<QPair<QString, QString> > > byPack;   // pack -> [(path, blob sha)]

    foreach (const QJsonValue& value, root.value("tree").toArray())
    {
        QJsonObject entry = value.toObject();
        if (entry.value("type").toString() != "blob")
            continue;

        QString full = entry.value("path").toString();
        int slash = full.indexOf('/');
        if (slash <= 0)
            continue;   // a file at the repository root belongs to no pack

        QString pack = full.left(slash);
        QString relative = full.mid(slash + 1);

        // A repository carries its own machinery. Without this, .github would be
        // installed as a pack called ".github", and every file in it fetched first.
        if (pack.startsWith('.') || !TemplateInstaller::isSafeSegment(pack))
            continue;

        if (!wantedPacks.isEmpty() && !wantedPacks.contains(pack, Qt::CaseInsensitive))
            continue;

        byPack[pack].append(qMakePair(relative, entry.value("sha").toString()));
    }

    if (!wantedPacks.isEmpty())
        skippedPacks = wantedPacks.count() - byPack.count();

    foreach (const QString& pack, byPack.keys())
    {
        QMap<QString, QString> mine = TemplateInstaller::packDigests(pack, true);

        QList<QPair<QString, QString> > entries = byPack.value(pack);
        for (int i = 0; i < entries.count(); i++)
        {
            QString relative = entries.at(i).first;
            QString sha = entries.at(i).second;

            if (relative.isEmpty() || sha.isEmpty())
                continue;

            if (TemplateInstaller::isProtected(relative))
                continue;

            if (!TemplateInstaller::isSafeRelativePath(relative))
            {
                say(QString("  %1 / %2 refused: not a usable path").arg(pack, relative));
                continue;
            }

            if (mine.value(relative) == sha)
                continue;

            Wanted wanted;
            wanted.pack = pack;
            wanted.relativePath = relative;
            wanted.sha1 = sha;          // a Git blob hash here, checked as one after the fetch
            this->queue.append(wanted);
        }
    }

    if (this->queue.count() > MAXIMUM_FILES_PER_POLL)
    {
        done(QString("the repository offered %1 files, which is more than one poll will take. "
                     "Check the address and the pack list.").arg(this->queue.count()));
        return;
    }

    if (byPack.isEmpty())
    {
        // Not the same thing as being up to date, and saying so would be wrong. The
        // usual cause is packs sitting in a subfolder rather than at the root.
        done("no packs found in the repository. Each folder at its root is one pack.");
        return;
    }

    if (this->queue.isEmpty())
    {
        done(skippedPacks > 0 ? QString("already up to date (%1 named pack(s) not in the repository)").arg(skippedPacks)
                              : "already up to date");
        return;
    }

    say(QString("GitHub: %1 file(s) to fetch").arg(this->queue.count()));
    fetchNext();
}

// What the relay has, minus what this machine already has. Everything that decides
// whether a file is fetched happens here, so the fetch loop stays a fetch loop.
void RelayClient::planFrom(const QByteArray& manifestJson)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(manifestJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        done("the relay did not answer with a manifest");
        return;
    }

    // What this machine is assigned, if the source says. Falls back to the local
    // setting when it does not.
    QStringList wantedPacks = packsForThisMachine(document.object().value("assignments").toObject());
    int skippedPacks = 0;

    foreach (const QJsonValue& value, document.object().value("packs").toArray())
    {
        QJsonObject pack = value.toObject();
        QString name = pack.value("name").toString();
        if (name.isEmpty())
            continue;

        // An empty filter follows everything. A filter that names packs lets one relay
        // carry every venue while each client takes only the packs that are its own.
        if (!wantedPacks.isEmpty() && !wantedPacks.contains(name, Qt::CaseInsensitive))
        {
            skippedPacks++;
            continue;
        }

        // What this poll is working towards. Reported back once it is reached, which
        // is how the dev machine learns this venue has the newest pack.
        this->versionByPack.insert(name, pack.value("version").toString());

        // What is already here, by digest, so an unchanged file is never fetched.
        QMap<QString, QString> mine;
        foreach (const QJsonValue& entry, TemplateInstaller::describePack(name).value("files").toArray())
        {
            QJsonObject file = entry.toObject();
            mine.insert(QString(file.value("path").toString()).replace('\\', '/'),
                        file.value("sha1").toString());
        }

        foreach (const QJsonValue& entry, pack.value("files").toArray())
        {
            QJsonObject file = entry.toObject();
            QString path = QString(file.value("path").toString()).replace('\\', '/');
            QString sha1 = file.value("sha1").toString();

            if (path.isEmpty() || sha1.isEmpty())
                continue;

            // Refused by the installer anyway; skipped here so it is not a download
            // that gets thrown away and reported as a failure.
            if (TemplateInstaller::isProtected(path))
                continue;

            // The same rule that guards the installer, applied before the fetch so a
            // relay serving a path this client will not write costs nothing.
            if (!TemplateInstaller::isSafeRelativePath(path))
            {
                say(QString("  %1 / %2 refused: not a usable path").arg(name, path));
                continue;
            }

            if (mine.value(path) == sha1)
                continue;   // identical, and the digest is the evidence rather than a guess

            Wanted wanted;
            wanted.pack = name;
            wanted.relativePath = path;
            wanted.sha1 = sha1;
            wanted.bytes = static_cast<qint64>(file.value("bytes").toDouble());
            this->queue.append(wanted);
        }
    }

    if (this->queue.count() > MAXIMUM_FILES_PER_POLL)
    {
        done(QString("the relay offered %1 files, which is more than one poll will take. "
                     "Check the address and the pack list.").arg(this->queue.count()));
        return;
    }

    if (this->queue.isEmpty())
    {
        done(skippedPacks > 0 ? QString("already up to date (%1 pack(s) not followed)").arg(skippedPacks)
                              : "already up to date");
        return;
    }

    say(QString("Relay: %1 file(s) to fetch").arg(this->queue.count()));
    fetchNext();
}

// One at a time, on purpose: this runs on a machine that may be on air, over a link
// that may be someone's phone, against a host that may be shared.
void RelayClient::fetchNext()
{
    if (this->queue.isEmpty())
    {
        done(QString());
        return;
    }

    fetchOne(this->queue.takeFirst());
}

void RelayClient::fetchOne(const Wanted& wanted)
{
    // Asking for the blob by its own hash rather than by path: the answer cannot be
    // a different file than the one the tree listed, even if the branch moved while
    // this poll was running.
    QString target = isGitHub()
        ? QString("%1/repos/%2/git/blobs/%3").arg(gitHubApi(), gitHubOwnerRepo(), wanted.sha1)
        : QString("%1&pack=%2&path=%3")
            .arg(endpoint("fetch"),
                 QString::fromUtf8(QUrl::toPercentEncoding(wanted.pack)),
                 QString::fromUtf8(QUrl::toPercentEncoding(wanted.relativePath, "/")));

    QNetworkRequest request((QUrl(target)));
    authorise(request);

    if (isGitHub())
        request.setRawHeader("Accept", "application/vnd.github.raw");   // the bytes, not JSON

    bool github = isGitHub();

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, wanted, github]() {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200)
        {
            if (worthRetrying(status) && wanted.attempts < MAXIMUM_ATTEMPTS)
            {
                Wanted again = wanted;
                again.attempts++;

                int delay = again.attempts * 2000;
                say(QString("  %1 / %2 no answer, trying again in %3s")
                    .arg(wanted.pack, wanted.relativePath).arg(delay / 1000));

                QTimer::singleShot(delay, this, [this, again]() { fetchOne(again); });
                return;
            }

            this->failed++;
            this->packsWithFailures.insert(wanted.pack);
            say(QString("  %1 / %2 failed: %3").arg(wanted.pack, wanted.relativePath,
                                                    status == 401 ? "wrong or missing token"
                                                                  : reply->errorString()));
            fetchNext();
            return;
        }

        QByteArray body = reply->readAll();

        // The listing said what these bytes would be. If they are not that, something
        // between here and there changed them, and the last thing to do with bytes
        // like that is write them where CasparCG will run them. A Git blob hash for
        // GitHub, a plain sha1 for a relay, checked the same way either way.
        QString actual = github ? TemplateInstaller::gitBlobSha(body)
                                : QString::fromLatin1(
                                      QCryptographicHash::hash(body, QCryptographicHash::Sha1).toHex());
        if (actual != wanted.sha1)
        {
            this->failed++;
            this->packsWithFailures.insert(wanted.pack);
            say(QString("  %1 / %2 refused: the bytes do not match the digest it was listed with")
                .arg(wanted.pack, wanted.relativePath));
            fetchNext();
            return;
        }

        QString error;
        int result = TemplateInstaller::installFile(wanted.pack, wanted.relativePath, body, &error);
        if (result == 200)
        {
            this->installed++;
            say(QString("  %1 / %2 installed").arg(wanted.pack, wanted.relativePath));
        }
        else
        {
            this->failed++;
            this->packsWithFailures.insert(wanted.pack);
            say(QString("  %1 / %2 failed: %3").arg(wanted.pack, wanted.relativePath, error));
        }

        fetchNext();
    });
}

void RelayClient::done(const QString& note)
{
    this->busy = false;
    this->ranAt = QDateTime::currentDateTime();

    // "Up to date" is a note and still a success. Anything else that came with a
    // note stopped the poll early, which a status light should show as a fault.
    this->ok = (this->failed == 0)
            && (note.isEmpty() || note.startsWith("already up to date"));

    if (!note.isEmpty())
        this->summary = note;
    else if (this->failed > 0)
        this->summary = QString("%1 installed, %2 failed").arg(this->installed).arg(this->failed);
    else
        this->summary = QString("%1 file(s) installed").arg(this->installed);

    say(QString("%1: %2").arg(isGitHub() ? "GitHub" : "Relay", this->summary));
    emit finished(this->installed, this->failed, this->summary);

    sendCheckIn();
}

// Reports what this machine holds, and nothing else. A relay records it under this
// machine's name and can tell the dev machine which venues are behind, which is the
// question worth answering before a show.
//
// A GitHub source is still never written to: a client's token there is read-only
// by design, and keeping it that way is worth more than the report. A venue on
// that route reports by being given a check-in address of its own — a relay, or
// anything that accepts the same POST — which needs no token upgrade anywhere.
void RelayClient::sendCheckIn()
{
    const CheckInTarget::Decision target =
        CheckInTarget::decide(isGitHub(), url(), token(), checkInUrl(), checkInToken());

    if (!target.send)
        return;

    if (this->versionByPack.isEmpty())
        return;   // the poll never got as far as a listing; there is nothing to claim

    QJsonObject packs;
    foreach (const QString& pack, this->versionByPack.keys())
    {
        // A pack that had a file fail is left out rather than claimed. Being absent
        // from the report is honest; being listed as current would not be.
        if (this->packsWithFailures.contains(pack))
            continue;

        packs.insert(pack, this->versionByPack.value(pack));
    }

    QJsonObject body;
    body.insert("host", QSysInfo::machineHostName());
    body.insert("os", QSysInfo::prettyProductName());
    body.insert("packs", packs);

    // What happened here, so a venue that is stuck can say why from the other side
    // of the internet. Nobody can open this machine's log, and "behind on SEVILLE"
    // without a reason is the start of a phone call rather than the end of one.
    body.insert("source", isGitHub() ? "github" : "relay");
    body.insert("result", this->summary);
    body.insert("failed", this->failed);
    body.insert("installed", this->installed);

    QNetworkRequest request((QUrl(CheckInTarget::endpointFor(target.url))));

    // Always the relay's header, never GitHub's: the destination is a relay even
    // when the templates came from somewhere else entirely.
    request.setRawHeader("X-Relay-Token", target.token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = this->network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, this, [reply]() {
        // Nothing is retried and nothing is reported on success: this is bookkeeping
        // for somebody else's benefit, and it must never be why a poll looks failed.
        reply->deleteLater();
    });
}
