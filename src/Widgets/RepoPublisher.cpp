#include "RepoPublisher.h"

#include "RelayClient.h"
#include "TemplateInstaller.h"

#include "DatabaseManager.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSysInfo>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace
{
    // A handful of small JSON calls and one blob per file. A stall this long is a
    // fault, not a slow link.
    const int TRANSFER_TIMEOUT_MS = 60 * 1000;

    // How many of the files are named in a "pull first" refusal.
    const int NAMED_IN_REFUSAL = 3;
}

RepoPublisher& RepoPublisher::getInstance()
{
    static RepoPublisher instance;
    return instance;
}

RepoPublisher::RepoPublisher()
    : QObject(nullptr)
{
    this->network = new QNetworkAccessManager(this);
    this->network->setTransferTimeout(TRANSFER_TIMEOUT_MS);
}

bool RepoPublisher::isMaster()
{
    return DatabaseManager::getInstance().getConfigurationByName("RelayMaster").getValue() == "true";
}

QString RepoPublisher::pushToken()
{
    return DatabaseManager::getInstance().getConfigurationByName("RelayPushToken").getValue().trimmed();
}

QList<RepoPublisher::Change> RepoPublisher::plan(const QString& pack, const QMap<QString, QString>& local,
                                                 const QMap<QString, QString>& remote, QStringList* missingLocally)
{
    QList<Change> changes;

    for (auto it = local.constBegin(); it != local.constEnd(); ++it)
    {
        const QString path = QString(it.key()).replace('\\', '/');

        // Each client owns these; they travel in neither direction.
        if (TemplateInstaller::isProtected(path))
            continue;

        // A name the installer would refuse at a venue is not worth sending.
        if (!TemplateInstaller::isSafeRelativePath(path))
            continue;

        const bool known = remote.contains(path);
        if (known && remote.value(path) == it.value())
            continue;

        Change change;
        change.pack = pack;
        change.path = path;
        change.sha = it.value();
        change.isNew = !known;
        changes.append(change);
    }

    if (missingLocally != nullptr)
    {
        for (auto it = remote.constBegin(); it != remote.constEnd(); ++it)
        {
            if (!local.contains(it.key()) && !TemplateInstaller::isProtected(it.key()))
                missingLocally->append(pack + "/" + it.key());
        }
    }

    return changes;
}

QStringList RepoPublisher::plannedFiles() const
{
    QStringList files;
    foreach (const Change& change, this->changes)
        files.append(change.repoPath());

    return files;
}

void RepoPublisher::prepare()
{
    if (this->busy)
    {
        emit planned(QStringList(), "a push is already in progress");
        return;
    }

    if (!isMaster())
    {
        emit planned(QStringList(), "this client is not a master");
        return;
    }

    if (!RelayClient::isGitHub())
    {
        emit planned(QStringList(), "the source is a relay; a client pushes to GitHub only");
        return;
    }

    if (pushToken().isEmpty())
    {
        emit planned(QStringList(), "no write token - the pull token cannot write, and must not");
        return;
    }

    if (TemplateInstaller::templatesRoot().isEmpty())
    {
        emit planned(QStringList(), "no template folder is configured on this machine");
        return;
    }

    this->busy = true;
    this->pushing = false;
    this->changes.clear();
    this->treeEntries.clear();
    this->headSha.clear();
    this->baseTreeSha.clear();
    this->branch = RelayClient::gitHubBranch();

    say("GitHub: reading the repository");

    if (!this->branch.isEmpty())
    {
        readTree();
        return;
    }

    // The address named no branch, so the repository says which is its default.
    call("GET", QString(), QJsonObject(), [this](int status, const QJsonObject& body) {
        if (status != 200)
        {
            fail(QString("could not reach the repository (%1)").arg(status));
            return;
        }

        this->branch = body.value("default_branch").toString();
        if (this->branch.isEmpty())
        {
            fail("the repository names no default branch");
            return;
        }

        readTree();
    });
}

void RepoPublisher::readTree()
{
    const QString ref = QString::fromUtf8(QUrl::toPercentEncoding(this->branch));

    // The head commit first: the new commit has to name it as parent, and the
    // branch move is refused if it has moved since - which is what stops a push
    // from overwriting something pushed in between.
    call("GET", QString("/git/ref/heads/%1").arg(ref), QJsonObject(), [this, ref](int status, const QJsonObject& body) {
        if (status != 200)
        {
            fail(QString("could not read branch %1 (%2)").arg(this->branch).arg(status));
            return;
        }

        this->headSha = body.value("object").toObject().value("sha").toString();
        if (this->headSha.isEmpty())
        {
            fail(QString("branch %1 has no head commit").arg(this->branch));
            return;
        }

        call("GET", QString("/git/trees/%1?recursive=1").arg(ref), QJsonObject(), [this](int status, const QJsonObject& body) {
            if (status != 200)
            {
                fail(QString("could not read the tree (%1)").arg(status));
                return;
            }

            if (body.value("truncated").toBool())
            {
                fail("the repository is too large for one tree listing, so this cannot tell what differs");
                return;
            }

            this->baseTreeSha = body.value("sha").toString();
            planFromTree(body);
        });
    });
}

void RepoPublisher::planFromTree(const QJsonObject& tree)
{
    QMap<QString, QMap<QString, QString> > remoteByPack;

    foreach (const QJsonValue& value, tree.value("tree").toArray())
    {
        QJsonObject entry = value.toObject();
        if (entry.value("type").toString() != "blob")
            continue;

        const QString full = entry.value("path").toString();
        const QString pack = RelayClient::packOfTreePath(full);
        if (pack.isEmpty())
            continue;

        remoteByPack[pack].insert(full.mid(pack.length() + 1), entry.value("sha").toString());
    }

    // Which packs: the ones this client follows. Following everything means
    // every pack it holds that the repository also has - a master does not
    // create a pack by accident. A pack named in the list is pushed even when
    // the repository lacks it, because naming it is the way to create one.
    QStringList packs = RelayClient::packFilter();
    if (packs.isEmpty())
    {
        QDir root(TemplateInstaller::templatesRoot());
        foreach (const QString& name, root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        {
            if (remoteByPack.contains(name))
                packs.append(name);
        }
    }

    QStringList missing;
    foreach (const QString& pack, packs)
    {
        if (!TemplateInstaller::isSafeSegment(pack))
            continue;

        QMap<QString, QString> local = TemplateInstaller::packDigests(pack, true);
        if (local.isEmpty())
            continue;   // not on this machine; nothing to push

        this->changes.append(plan(pack, local, remoteByPack.value(pack), &missing));
    }

    this->busy = false;

    if (!missing.isEmpty())
    {
        QStringList named = missing.mid(0, NAMED_IN_REFUSAL);
        const QString problem = QString("the repository holds %1 file(s) this machine has not pulled (%2%3). "
                                        "Pull first, then push.")
            .arg(missing.count())
            .arg(named.join(", "), missing.count() > NAMED_IN_REFUSAL ? ", ..." : "");

        say("GitHub: " + problem);
        this->changes.clear();
        emit planned(QStringList(), problem);
        return;
    }

    say(QString("GitHub: %1 file(s) differ from the repository").arg(this->changes.count()));
    emit planned(plannedFiles(), QString());
}

void RepoPublisher::push(const QString& message)
{
    if (this->busy)
    {
        emit pushed(false, "a push is already in progress");
        return;
    }

    if (this->changes.isEmpty() || this->headSha.isEmpty())
    {
        emit pushed(false, "nothing is prepared - press the button again");
        return;
    }

    if (message.trimmed().isEmpty())
    {
        emit pushed(false, "a commit message is needed");
        return;
    }

    this->busy = true;
    this->pushing = true;
    this->message = message.trimmed();
    this->treeEntries.clear();
    this->nextBlob = 0;

    say(QString("GitHub: sending %1 file(s)").arg(this->changes.count()));
    uploadNextBlob();
}

void RepoPublisher::uploadNextBlob()
{
    if (this->nextBlob >= this->changes.count())
    {
        createTree();
        return;
    }

    const Change change = this->changes.at(this->nextBlob);

    QFile file(QDir(QDir(TemplateInstaller::templatesRoot()).filePath(change.pack)).filePath(change.path));
    if (!file.open(QIODevice::ReadOnly))
    {
        fail(QString("could not read %1").arg(change.repoPath()));
        return;
    }

    const QByteArray content = file.readAll();
    file.close();

    // The bytes have to be the bytes prepare listed. A template saved between
    // the list and the push is not what the operator confirmed.
    if (TemplateInstaller::gitBlobSha(content) != change.sha)
    {
        fail(QString("%1 changed since it was listed; press the button again").arg(change.repoPath()));
        return;
    }

    QJsonObject body;
    body.insert("content", QString::fromLatin1(content.toBase64()));
    body.insert("encoding", "base64");

    call("POST", "/git/blobs", body, [this, change](int status, const QJsonObject& answer) {
        if (status != 201 && status != 200)
        {
            fail(QString("could not upload %1 (%2)").arg(change.repoPath()).arg(status));
            return;
        }

        // GitHub names a blob the way this machine already did, so a different
        // answer means the bytes did not arrive whole.
        const QString sha = answer.value("sha").toString();
        if (sha != change.sha)
        {
            fail(QString("%1 arrived changed (sent %2, stored %3)")
                 .arg(change.repoPath(), change.sha.left(7), sha.left(7)));
            return;
        }

        QJsonObject entry;
        entry.insert("path", change.repoPath());
        entry.insert("mode", "100644");
        entry.insert("type", "blob");
        entry.insert("sha", sha);
        this->treeEntries.append(entry);

        say(QString("  %1 %2").arg(change.isNew ? "new" : "changed", change.repoPath()));

        this->nextBlob++;
        uploadNextBlob();
    });
}

void RepoPublisher::createTree()
{
    QJsonArray tree;
    foreach (const QJsonObject& entry, this->treeEntries)
        tree.append(entry);

    QJsonObject body;
    body.insert("base_tree", this->baseTreeSha);
    body.insert("tree", tree);

    call("POST", "/git/trees", body, [this](int status, const QJsonObject& answer) {
        if (status != 201 && status != 200)
        {
            fail(QString("could not build the tree (%1)").arg(status));
            return;
        }

        const QString sha = answer.value("sha").toString();
        if (sha.isEmpty())
        {
            fail("GitHub returned no tree");
            return;
        }

        createCommit(sha);
    });
}

void RepoPublisher::createCommit(const QString& treeSha)
{
    // Authored as the machine, so the history says which venue changed what.
    const QString host = QSysInfo::machineHostName();

    QJsonObject author;
    author.insert("name", QString("%1 (CasparCG Client)").arg(host));
    author.insert("email", QString("%1@casparcg-client.invalid").arg(host.toLower()));

    QJsonObject body;
    body.insert("message", this->message);
    body.insert("tree", treeSha);
    body.insert("parents", QJsonArray() << this->headSha);
    body.insert("author", author);

    call("POST", "/git/commits", body, [this](int status, const QJsonObject& answer) {
        if (status != 201 && status != 200)
        {
            fail(QString("could not create the commit (%1)").arg(status));
            return;
        }

        const QString sha = answer.value("sha").toString();
        if (sha.isEmpty())
        {
            fail("GitHub returned no commit");
            return;
        }

        moveBranch(sha);
    });
}

void RepoPublisher::moveBranch(const QString& commitSha)
{
    QJsonObject body;
    body.insert("sha", commitSha);
    // No force. If the branch moved since prepare read it, this is refused and
    // nothing of anybody else's is overwritten.

    const QString ref = QString::fromUtf8(QUrl::toPercentEncoding(this->branch));

    call("PATCH", QString("/git/refs/heads/%1").arg(ref), body, [this, commitSha](int status, const QJsonObject&) {
        if (status != 200)
        {
            fail(QString("could not move %1 (%2) - the repository may have moved on; pull, then push again")
                 .arg(this->branch).arg(status));
            return;
        }

        const int count = this->changes.count();

        this->busy = false;
        this->pushing = false;
        this->changes.clear();
        this->headSha.clear();

        const QString summary = QString("%1 file(s) pushed as %2").arg(count).arg(commitSha.left(7));
        say("GitHub: " + summary);
        emit pushed(true, summary);
    });
}

void RepoPublisher::call(const QString& verb, const QString& path, const QJsonObject& body, Answer answer)
{
    QNetworkRequest request((QUrl(QString("%1/repos/%2%3")
        .arg(RelayClient::gitHubApi(), RelayClient::gitHubOwnerRepo(), path))));

    request.setRawHeader("Authorization", QByteArray("Bearer ") + pushToken().toUtf8());
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setRawHeader("User-Agent", "CasparCG-Client");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    const QByteArray payload = body.isEmpty() ? QByteArray() : QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = (verb == "GET")  ? this->network->get(request)
                         : (verb == "POST") ? this->network->post(request, payload)
                                            : this->network->sendCustomRequest(request, verb.toLatin1(), payload);

    QObject::connect(reply, &QNetworkReply::finished, this, [reply, answer]() {
        reply->deleteLater();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        answer(status, QJsonDocument::fromJson(reply->readAll()).object());
    });
}

void RepoPublisher::fail(const QString& why)
{
    this->busy = false;
    say("GitHub: " + why);

    if (this->pushing)
    {
        this->pushing = false;
        emit pushed(false, why);
    }
    else
    {
        this->changes.clear();
        emit planned(QStringList(), why);
    }
}

void RepoPublisher::say(const QString& line)
{
    emit progress(line);
}
