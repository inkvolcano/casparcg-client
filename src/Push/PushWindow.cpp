#include "PushWindow.h"
#include "../Common/GitBlobSha.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSettings>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <QtGui/QBrush>
#include <QtGui/QColor>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

PushWindow::PushWindow(QWidget* parent)
    : QMainWindow(parent)
{
    this->network = new QNetworkAccessManager(this);

    // An INACTIVITY timeout rather than a deadline. Confirmed against Qt 6.5.3: a
    // transfer still delivering bytes is left alone however long it takes, and one
    // that has stopped is cut at this interval. A large pack on a slow link is
    // therefore safe, and this is not a number to raise for one.
    this->network->setTransferTimeout(20000);

    setWindowTitle("CasparCG Template Push");
    resize(1000, 700);

    buildUi();
    loadSettings();
    refreshPacks();
}

// ---- where a target lives ----

namespace
{
    // <url>?action=X, whichever way the operator wrote the relay address.
    QString relayQuery(const QString& url, const QString& action)
    {
        return url + (url.contains('?') ? "&action=" : "?action=") + action;
    }

    QString encoded(const QString& value, const QByteArray& keep = QByteArray())
    {
        return QString::fromUtf8(QUrl::toPercentEncoding(value, keep));
    }
}

// One place that decides what an address means, so the table, the settings file and
// every request agree about it.
void PushTarget::readAddress(const QString& address)
{
    QString trimmed = address.trimmed();

    this->github = trimmed.startsWith("github:", Qt::CaseInsensitive);
    if (this->github)
    {
        this->host = trimmed;

        QString rest = trimmed.mid(QString("github:").length()).trimmed();

        // "github:https://github.example.com/api/v3/owner/repo" for an Enterprise
        // Server. Everything before the last two path segments is the API address.
        if (rest.startsWith("http://", Qt::CaseInsensitive) || rest.startsWith("https://", Qt::CaseInsensitive))
        {
            int lastSlash = rest.lastIndexOf('/');
            int ownerSlash = lastSlash > 0 ? rest.lastIndexOf('/', lastSlash - 1) : -1;
            if (ownerSlash > 0)
            {
                this->api = rest.left(ownerSlash);
                rest = rest.mid(ownerSlash + 1);
            }
        }

        int at = rest.indexOf('@');
        if (at >= 0)
        {
            this->branch = rest.mid(at + 1).trimmed();
            rest = rest.left(at);
        }

        while (rest.endsWith('/'))
            rest.chop(1);

        this->ownerRepo = rest;
        return;
    }

    // A scheme means a relay. Splitting a URL on its last colon would otherwise read
    // "https" as the host, which is a confusing way to find that out.
    this->relay = trimmed.contains("://");
    if (this->relay)
    {
        this->host = trimmed;
        return;
    }

    int colon = trimmed.lastIndexOf(':');
    this->host = colon > 0 ? trimmed.left(colon) : trimmed;
    this->port = colon > 0 ? trimmed.mid(colon + 1).toInt() : 3000;
}

QString PushTarget::base() const
{
    if (this->github)
    {
        QString host = this->api.isEmpty() ? QString("https://api.github.com") : this->api;
        return QString("%1/repos/%2").arg(host, this->ownerRepo);
    }

    return this->relay ? this->host : QString("http://%1:%2").arg(this->host).arg(this->port);
}

QString PushTarget::label() const
{
    if (!this->name.isEmpty())
        return this->name;

    if (this->github)
        return QString("github.com/%1").arg(this->ownerRepo);

    return this->relay ? QString("relay %1").arg(this->host) : base();
}

QString PushTarget::infoUrl() const
{
    if (this->github)
        return base();   // the repository itself answers "does this token open you"

    return this->relay ? relayQuery(this->host, "ping")
                       : QString("%1/templates/info").arg(base());
}

QString PushTarget::manifestUrl(const QString& pack) const
{
    if (this->github)
    {
        // One call lists the whole repository with a digest per file. It is not
        // per-pack, and the caller keeps only the pack it asked about.
        QString ref = this->branch.isEmpty() ? QString("HEAD") : this->branch;
        return QString("%1/git/trees/%2?recursive=1").arg(base(), encoded(ref));
    }

    if (this->relay)
        return QString("%1&pack=%2").arg(relayQuery(this->host, "manifest"), encoded(pack));

    return QString("%1/templates/%2").arg(base(), pack);
}

QString PushTarget::uploadUrl(const QString& pack, const QString& relativePath) const
{
    if (this->github)
        return QString("%1/contents/%2/%3").arg(base(), encoded(pack), encoded(relativePath, "/"));

    if (this->relay)
    {
        return QString("%1&pack=%2&path=%3").arg(relayQuery(this->host, "upload"),
                                                 encoded(pack), encoded(relativePath, "/"));
    }

    return QString("%1/templates/%2/%3").arg(base(), pack, encoded(relativePath, "/"));
}

QString PushTarget::removeUrl(const QString& pack, const QString& relativePath) const
{
    if (this->github)
        return QString("%1/contents/%2/%3").arg(base(), encoded(pack), encoded(relativePath, "/"));

    return QString("%1&pack=%2&path=%3").arg(relayQuery(this->host, "remove"),
                                             encoded(pack), encoded(relativePath, "/"));
}

void PushTarget::authorise(QNetworkRequest& request) const
{
    if (this->github)
    {
        // GitHub refuses a request with no user agent, so that is not optional.
        request.setRawHeader("Authorization", QByteArray("Bearer ") + this->token.toUtf8());
        request.setRawHeader("Accept", "application/vnd.github+json");
        request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
        request.setRawHeader("User-Agent", "CasparCG-Template-Push");
        return;
    }

    request.setRawHeader(this->relay ? "X-Relay-Token" : "X-Template-Token", this->token.toUtf8());
}

// Git names a file by sha1 of "blob <length>\0" then its bytes. A tree listing
// gives those, so a pack on disk can be compared against a repository without
// downloading any of it.
QString PushWindow::gitBlobSha(const QByteArray& content)
{
    return gitBlobShaOf(content);
}

namespace
{
    // Two more goes after the first. Enough to ride out a blip, few enough that a
    // genuinely unreachable client is reported rather than waited on.
    const int MAXIMUM_ATTEMPTS = 2;
}

void PushWindow::buildUi()
{
    QWidget* central = new QWidget(this);
    QVBoxLayout* outer = new QVBoxLayout(central);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    // ---- where the packs come from ----
    QHBoxLayout* sourceRow = new QHBoxLayout();
    sourceRow->addWidget(new QLabel("Templates folder:", central), 0);

    this->sourceEdit = new QLineEdit(central);
    this->sourceEdit->setPlaceholderText(R"(C:\CasparCG\templates)");
    this->sourceEdit->setToolTip(
        "The folder that HOLDS the packs, not a pack itself.\n"
        "Each subfolder is one pack: point this at ...\\templates, not at ...\\templates\\SEVILLE.");
    QObject::connect(this->sourceEdit, &QLineEdit::editingFinished, this, [this]() { refreshPacks(); });
    sourceRow->addWidget(this->sourceEdit, 1);

    QPushButton* browse = new QPushButton("Browse...", central);
    browse->setFocusPolicy(Qt::NoFocus);
    QObject::connect(browse, &QPushButton::clicked, this, &PushWindow::browseForSource);
    sourceRow->addWidget(browse, 0);

    outer->addLayout(sourceRow);

    // ---- packs and clients ----
    QSplitter* top = new QSplitter(Qt::Horizontal, central);

    QGroupBox* packBox = new QGroupBox("Packs", top);
    QVBoxLayout* packLayout = new QVBoxLayout(packBox);
    this->packList = new QListWidget(packBox);
    this->packList->setToolTip("Ticked packs are compared.");
    packLayout->addWidget(this->packList);
    top->addWidget(packBox);

    QGroupBox* targetBox = new QGroupBox("Clients", top);
    QVBoxLayout* targetLayout = new QVBoxLayout(targetBox);

    this->targetTable = new QTableWidget(0, 4, targetBox);
    this->targetTable->setHorizontalHeaderLabels(QStringList() << "Use" << "Name" << "Host : Port" << "Token");
    this->targetTable->horizontalHeader()->setStretchLastSection(true);
    this->targetTable->verticalHeader()->setVisible(false);
    this->targetTable->setColumnWidth(0, 40);
    this->targetTable->setColumnWidth(1, 110);
    this->targetTable->setColumnWidth(2, 150);
    this->targetTable->setToolTip(
        "Three kinds of address, told apart by how they are written:\n\n"
        "  10.0.0.5:3000                 a client, token from its Settings -> Templates\n"
        "  https://host/relay/relay.php  a relay, that relay's UPLOAD token\n"
        "  github:owner/repo@branch      a private repository, a GitHub token with\n"
        "                                read and write access to its contents\n\n"
        "Use a relay or a repository for any client you cannot reach directly. Those\n"
        "clients pull for themselves, so nothing has to reach in to them.");
    targetLayout->addWidget(this->targetTable);

    QHBoxLayout* targetButtons = new QHBoxLayout();
    QPushButton* add = new QPushButton("Add", targetBox);
    QPushButton* remove = new QPushButton("Remove", targetBox);
    add->setFocusPolicy(Qt::NoFocus);
    remove->setFocusPolicy(Qt::NoFocus);
    QObject::connect(add, &QPushButton::clicked, this, &PushWindow::addTarget);
    QObject::connect(remove, &QPushButton::clicked, this, &PushWindow::removeTarget);
    targetButtons->addWidget(add);
    targetButtons->addWidget(remove);
    targetButtons->addStretch();
    targetLayout->addLayout(targetButtons);

    top->addWidget(targetBox);
    top->setStretchFactor(0, 1);
    top->setStretchFactor(1, 2);
    top->setMaximumHeight(190);
    outer->addWidget(top, 0);

    // ---- what a push would do ----
    QHBoxLayout* reviewHeader = new QHBoxLayout();
    reviewHeader->addWidget(new QLabel("Files", central), 0);
    reviewHeader->addStretch();

    QPushButton* all = new QPushButton("Tick all", central);
    QPushButton* none = new QPushButton("Tick none", central);
    all->setFocusPolicy(Qt::NoFocus);
    none->setFocusPolicy(Qt::NoFocus);
    QObject::connect(all, &QPushButton::clicked, this, &PushWindow::tickAll);
    QObject::connect(none, &QPushButton::clicked, this, &PushWindow::tickNone);
    reviewHeader->addWidget(all);
    reviewHeader->addWidget(none);

    this->identifyButton = new QPushButton("Identify", central);
    this->identifyButton->setToolTip("Ask each ticked client who it is, and check the token. Writes nothing.");
    QObject::connect(this->identifyButton, &QPushButton::clicked, this, &PushWindow::identifyTargets);
    reviewHeader->addWidget(this->identifyButton);

    this->removeButton = new QPushButton("Clear", central);
    this->removeButton->setToolTip(
        "Take ticked \"only there\" files off a RELAY. Clients are never touched:\n"
        "removing a template from a machine that may be on air is not a decision\n"
        "to make from another network.");
    this->removeButton->setEnabled(false);
    QObject::connect(this->removeButton, &QPushButton::clicked, this, &PushWindow::removeExtras);
    reviewHeader->addWidget(this->removeButton);

    this->compareButton = new QPushButton("Compare", central);
    this->compareButton->setToolTip("Ask each client what it has and list what differs. Writes nothing.");
    QObject::connect(this->compareButton, &QPushButton::clicked, this, &PushWindow::startCompare);
    reviewHeader->addWidget(this->compareButton);

    this->pushButton = new QPushButton("Push ticked", central);
    this->pushButton->setToolTip("Send the ticked files, and only those.");
    QObject::connect(this->pushButton, &QPushButton::clicked, this, &PushWindow::startPush);
    reviewHeader->addWidget(this->pushButton);

    outer->addLayout(reviewHeader);

    QSplitter* bottom = new QSplitter(Qt::Vertical, central);

    this->fileTable = new QTableWidget(0, 6, bottom);
    this->fileTable->setHorizontalHeaderLabels(
        QStringList() << "Send" << "Client" << "Pack" << "File" << "State" << "Size");
    this->fileTable->verticalHeader()->setVisible(false);
    this->fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->fileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->fileTable->setColumnWidth(0, 44);
    this->fileTable->setColumnWidth(1, 110);
    this->fileTable->setColumnWidth(2, 110);
    this->fileTable->setColumnWidth(4, 80);
    this->fileTable->setColumnWidth(5, 70);
    this->fileTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    bottom->addWidget(this->fileTable);

    this->logView = new QPlainTextEdit(bottom);
    this->logView->setReadOnly(true);
    this->logView->setMaximumBlockCount(4000);
    bottom->addWidget(this->logView);

    bottom->setStretchFactor(0, 3);
    bottom->setStretchFactor(1, 1);
    outer->addWidget(bottom, 1);

    setCentralWidget(central);
}

// ---- settings ----

void PushWindow::loadSettings()
{
    QSettings settings("CasparCG", "TemplatePush");
    this->sourceEdit->setText(settings.value("source").toString());

    QJsonArray targets = QJsonDocument::fromJson(settings.value("targets").toByteArray()).array();
    foreach (const QJsonValue& value, targets)
    {
        QJsonObject entry = value.toObject();

        int row = this->targetTable->rowCount();
        this->targetTable->insertRow(row);

        QTableWidgetItem* use = new QTableWidgetItem();
        use->setCheckState(entry.value("send").toBool(true) ? Qt::Checked : Qt::Unchecked);
        this->targetTable->setItem(row, 0, use);
        this->targetTable->setItem(row, 1, new QTableWidgetItem(entry.value("name").toString()));
        QString host = entry.value("host").toString();
        bool whole = host.contains("://") || host.startsWith("github:", Qt::CaseInsensitive);
        this->targetTable->setItem(row, 2, new QTableWidgetItem(
            whole ? host : QString("%1:%2").arg(host).arg(entry.value("port").toInt(3000))));
        this->targetTable->setItem(row, 3, new QTableWidgetItem(entry.value("token").toString()));
    }

    if (this->targetTable->rowCount() == 0)
        addTarget();
}

void PushWindow::saveSettings()
{
    QSettings settings("CasparCG", "TemplatePush");
    settings.setValue("source", this->sourceEdit->text().trimmed());

    QJsonArray targets;
    for (int row = 0; row < this->targetTable->rowCount(); row++)
    {
        QString address = this->targetTable->item(row, 2) ? this->targetTable->item(row, 2)->text().trimmed() : QString();

        // Stored the way it was typed, and taken apart again on the way back in, so
        // there is one reading of an address rather than two that can drift.
        PushTarget parsed;
        parsed.readAddress(address);

        QJsonObject entry;
        entry.insert("send", this->targetTable->item(row, 0) && this->targetTable->item(row, 0)->checkState() == Qt::Checked);
        entry.insert("name", this->targetTable->item(row, 1) ? this->targetTable->item(row, 1)->text() : QString());
        entry.insert("host", (parsed.relay || parsed.github) ? address : parsed.host);
        entry.insert("port", parsed.port);
        entry.insert("token", this->targetTable->item(row, 3) ? this->targetTable->item(row, 3)->text() : QString());
        targets.append(entry);
    }

    settings.setValue("targets", QJsonDocument(targets).toJson(QJsonDocument::Compact));
}

void PushWindow::addTarget()
{
    int row = this->targetTable->rowCount();
    this->targetTable->insertRow(row);

    QTableWidgetItem* use = new QTableWidgetItem();
    use->setCheckState(Qt::Checked);
    this->targetTable->setItem(row, 0, use);
    this->targetTable->setItem(row, 1, new QTableWidgetItem("Gallery"));
    this->targetTable->setItem(row, 2, new QTableWidgetItem("127.0.0.1:3000"));
    this->targetTable->setItem(row, 3, new QTableWidgetItem(QString()));
}

void PushWindow::removeTarget()
{
    int row = this->targetTable->currentRow();
    if (row >= 0)
        this->targetTable->removeRow(row);
}

void PushWindow::browseForSource()
{
    QString chosen = QFileDialog::getExistingDirectory(this, "Templates folder", this->sourceEdit->text());
    if (chosen.isEmpty())
        return;

    this->sourceEdit->setText(QDir::toNativeSeparators(chosen));
    refreshPacks();
}

// ---- what is here ----

void PushWindow::refreshPacks()
{
    QStringList ticked = checkedPacks();
    this->packList->clear();

    QDir directory(this->sourceEdit->text().trimmed());
    if (!directory.exists())
        return;

    foreach (const QString& name, directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
    {
        QListWidgetItem* item = new QListWidgetItem(name, this->packList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(ticked.contains(name) ? Qt::Checked : Qt::Unchecked);
    }
}

QStringList PushWindow::checkedPacks() const
{
    QStringList packs;
    for (int i = 0; i < this->packList->count(); i++)
        if (this->packList->item(i)->checkState() == Qt::Checked)
            packs.append(this->packList->item(i)->text());

    return packs;
}

QList<PushTarget> PushWindow::checkedTargets() const
{
    QList<PushTarget> targets;
    for (int row = 0; row < this->targetTable->rowCount(); row++)
    {
        if (!this->targetTable->item(row, 0) || this->targetTable->item(row, 0)->checkState() != Qt::Checked)
            continue;

        QString address = this->targetTable->item(row, 2) ? this->targetTable->item(row, 2)->text().trimmed() : QString();

        PushTarget target;
        target.name = this->targetTable->item(row, 1) ? this->targetTable->item(row, 1)->text().trimmed() : QString();
        target.token = this->targetTable->item(row, 3) ? this->targetTable->item(row, 3)->text().trimmed() : QString();
        target.readAddress(address);

        if (!target.host.isEmpty())
            targets.append(target);
    }

    return targets;
}

// The same rule the client enforces, applied here as well so a protected file is
// never even offered.
bool PushWindow::isProtected(const QString& relativePath)
{
    if (relativePath.contains('/'))
        return false;

    return relativePath.compare("project.js", Qt::CaseInsensitive) == 0
        || relativePath.compare("extensions.json", Qt::CaseInsensitive) == 0;
}

QString PushWindow::humanBytes(qint64 bytes)
{
    if (bytes >= 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    if (bytes >= 1024)
        return QString("%1 KB").arg(bytes / 1024);

    return QString("%1 B").arg(bytes);
}

QMap<QString, QString> PushWindow::localFiles(const QString& pack, bool gitStyle) const
{
    QMap<QString, QString> files;

    QDir packDir(QDir(this->sourceEdit->text().trimmed()).filePath(pack));
    if (!packDir.exists())
        return files;

    QDirIterator it(packDir.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        it.next();
        QString relative = packDir.relativeFilePath(it.fileInfo().absoluteFilePath());
        if (isProtected(relative))
            continue;

        QFile file(it.fileInfo().absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;

        QByteArray content = file.readAll();
        file.close();

        files.insert(relative, gitStyle
            ? gitBlobSha(content)
            : QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha1).toHex()));
    }

    return files;
}

// ---- compare ----

void PushWindow::log(const QString& line)
{
    this->logView->appendPlainText(line);
}

void PushWindow::setBusy(bool value)
{
    this->busy = value;
    this->compareButton->setEnabled(!value);
    this->pushButton->setEnabled(!value);
    this->identifyButton->setEnabled(!value);
    this->removeButton->setEnabled(!value && this->hasExtras);
}

void PushWindow::startCompare()
{
    if (this->busy)
        return;

    saveSettings();

    QList<PushTarget> targets = checkedTargets();
    QStringList packs = checkedPacks();

    if (targets.isEmpty() || packs.isEmpty())
    {
        log(targets.isEmpty() ? "No clients ticked." : "No packs ticked.");
        return;
    }

    this->results.clear();
    this->fileTable->setRowCount(0);
    this->pairQueue.clear();
    this->hasExtras = false;

    foreach (const PushTarget& target, targets)
        foreach (const QString& pack, packs)
            this->pairQueue.append(qMakePair(target, pack));

    this->logView->clear();
    log(QString("Comparing %1 pack(s) on %2 client(s)").arg(packs.count()).arg(targets.count()));

    setBusy(true);
    nextPair();
}

void PushWindow::nextPair()
{
    if (this->pairQueue.isEmpty())
    {
        showResults();
        setBusy(false);
        return;
    }

    QPair<PushTarget, QString> pair = this->pairQueue.takeFirst();
    PushTarget target = pair.first;
    QString pack = pair.second;

    QNetworkRequest request((QUrl(target.manifestUrl(pack))));
    target.authorise(request);

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, target, pack]() {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray body = reply->readAll();

        // 404 is not a failure: it is a pack that end has never had. A relay says the
        // same thing as a 200 with no files in it, which needs no special case.
        if (reply->error() != QNetworkReply::NoError && status != 404)
        {
            QString why = (status == 401) ? "wrong or missing token"
                        : (status == 403) ? "template push is switched off on that client"
                        : (status == 429) ? "temporarily refusing this address after repeated wrong tokens"
                        : (status == 422) ? "the file arrived changed and was not stored"
                        : reply->errorString();
            log(QString("  %1 / %2 \xE2\x80\x94 %3").arg(target.label(), pack, why));
            nextPair();
            return;
        }

        QJsonObject answer = QJsonDocument::fromJson(body).object();

        QMap<QString, QString> remote;
        if (target.github)
        {
            if (answer.value("truncated").toBool())
            {
                // A truncated tree looks exactly like a repository missing files, and
                // acting on it would offer to re-upload things that are already there.
                log(QString("  %1 \xE2\x80\x94 the repository is too large for one tree listing").arg(target.label()));
                nextPair();
                return;
            }

            // The tree is the whole repository; keep the part under this pack.
            QString prefix = pack + "/";
            foreach (const QJsonValue& value, answer.value("tree").toArray())
            {
                QJsonObject entry = value.toObject();
                if (entry.value("type").toString() != "blob")
                    continue;

                QString full = entry.value("path").toString();
                if (!full.startsWith(prefix))
                    continue;

                remote.insert(full.mid(prefix.length()), entry.value("sha").toString());
            }
        }
        else
        {
            foreach (const QJsonValue& value, answer.value("files").toArray())
            {
                QJsonObject entry = value.toObject();
                remote.insert(entry.value("path").toString(), entry.value("sha1").toString());
            }
        }

        QMap<QString, QString> local = localFiles(pack, target.github);
        if (local.isEmpty())
        {
            log(QString("  %1 / %2 \xE2\x80\x94 no such pack in the templates folder").arg(target.label(), pack));
            nextPair();
            return;
        }

        if (target.github && remote.isEmpty())
        {
            // Worth saying once rather than listing every file as new without
            // comment: the first push of a pack looks exactly like this.
            log(QString("  %1 / %2 is not in the repository yet; every file counts as new")
                .arg(target.label(), pack));
        }

        QString packRoot = QDir(this->sourceEdit->text().trimmed()).filePath(pack);
        foreach (const QString& relative, local.keys())
        {
            PushJob job;
            job.target = target;
            job.pack = pack;
            job.relativePath = relative;
            job.absolutePath = QDir(packRoot).filePath(relative);
            job.bytes = QFileInfo(job.absolutePath).size();

            // What the far end calls its copy. GitHub needs it to replace a file, and
            // there is nothing to name when the file is not there yet.
            job.remoteId = remote.value(relative);

            if (!remote.contains(relative))
                job.state = PushJob::New;
            else if (remote.value(relative) != local.value(relative))
                job.state = PushJob::Changed;
            else
                job.state = PushJob::Unchanged;

            this->results.append(job);
        }

        // The other direction. Neither a push nor a pull ever deletes, so a file
        // that was renamed or dropped from the pack stays wherever it landed and
        // keeps being served. Listing it is how that stops being invisible.
        foreach (const QString& relative, remote.keys())
        {
            if (local.contains(relative) || isProtected(relative))
                continue;

            PushJob job;
            job.target = target;
            job.pack = pack;
            job.relativePath = relative;
            job.remoteId = remote.value(relative);
            job.state = PushJob::Extra;
            this->results.append(job);
        }

        nextPair();
    });
}

// Everything found, unchanged included. Listing what will NOT be sent is the point:
// it is the evidence that an unchanged file is left alone, rather than a promise.
void PushWindow::showResults()
{
    this->fileTable->setRowCount(0);

    int toSend = 0;
    int extras = 0;
    foreach (const PushJob& job, this->results)
    {
        int row = this->fileTable->rowCount();
        this->fileTable->insertRow(row);

        bool wanted = (job.state == PushJob::New || job.state == PushJob::Changed);
        if (wanted)
            toSend++;
        if (job.state == PushJob::Extra)
            extras++;

        QTableWidgetItem* send = new QTableWidgetItem();
        send->setCheckState(wanted ? Qt::Checked : Qt::Unchecked);
        this->fileTable->setItem(row, 0, send);
        this->fileTable->setItem(row, 1, new QTableWidgetItem(job.target.label()));
        this->fileTable->setItem(row, 2, new QTableWidgetItem(job.pack));
        this->fileTable->setItem(row, 3, new QTableWidgetItem(job.relativePath));
        this->fileTable->setItem(row, 4, new QTableWidgetItem(job.stateText()));
        this->fileTable->setItem(row, 5, new QTableWidgetItem(humanBytes(job.bytes)));

        // Unchanged rows stay visible but dimmed: they are the answer to "what did
        // it leave alone", and greying them says so without hiding them.
        if (!wanted)
        {
            QBrush dim(QColor(140, 140, 140));
            for (int column = 1; column < 6; column++)
                this->fileTable->item(row, column)->setForeground(dim);
        }
        else if (job.state == PushJob::New)
        {
            this->fileTable->item(row, 4)->setForeground(QBrush(QColor(110, 170, 110)));
        }
        else
        {
            this->fileTable->item(row, 4)->setForeground(QBrush(QColor(215, 175, 90)));
        }

        // An extra is dimmed like an unchanged row but says something different, so
        // its state cell keeps a colour of its own rather than disappearing into the
        // list of things that are fine.
        if (job.state == PushJob::Extra)
            this->fileTable->item(row, 4)->setForeground(QBrush(QColor(130, 160, 210)));
    }

    this->hasExtras = (extras > 0);
    this->removeButton->setEnabled(this->hasExtras);

    log(QString("%1 file(s) listed, %2 ticked to send, %3 already current.")
        .arg(this->results.count()).arg(toSend)
        .arg(this->results.count() - toSend - extras));

    if (extras > 0)
    {
        log(QString("%1 file(s) exist only at the far end \xE2\x80\x94 renamed or dropped from the pack. "
                    "Nothing deletes them by itself; tick them and press Clear to take them off a relay.")
            .arg(extras));
    }
}

void PushWindow::tickAll()
{
    for (int row = 0; row < this->fileTable->rowCount(); row++)
        this->fileTable->item(row, 0)->setCheckState(Qt::Checked);
}

void PushWindow::tickNone()
{
    for (int row = 0; row < this->fileTable->rowCount(); row++)
        this->fileTable->item(row, 0)->setCheckState(Qt::Unchecked);
}

// ---- push ----

void PushWindow::startPush()
{
    if (this->busy)
        return;

    if (this->results.isEmpty())
    {
        log("Nothing to push: press Compare first, so what would change can be seen before it does.");
        return;
    }

    this->sendQueue.clear();
    for (int row = 0; row < this->fileTable->rowCount() && row < this->results.count(); row++)
    {
        // An extra has no local file behind it, so a tick on one means "clear it off
        // the relay" and is the Clear button's business, never this one's.
        if (this->results.at(row).state == PushJob::Extra)
            continue;

        if (this->fileTable->item(row, 0)->checkState() == Qt::Checked)
            this->sendQueue.append(row);
    }

    if (this->sendQueue.isEmpty())
    {
        log("Nothing ticked.");
        return;
    }

    this->sent = 0;
    this->failed = 0;
    log(QString("Pushing %1 file(s)").arg(this->sendQueue.count()));

    setBusy(true);
    nextFile();
}

// A refusal is an answer and will be the same answer next time. A dropped
// connection, a gateway hiccup or a host under load is none of those, and over the
// internet it is the common case rather than the interesting one.
bool PushWindow::worthRetrying(int httpStatus)
{
    // 0 is no HTTP answer at all: the connection went away.
    return httpStatus == 0 || httpStatus >= 500;
}

void PushWindow::nextFile()
{
    if (this->sendQueue.isEmpty())
    {
        log(QString("Done. %1 sent, %2 failed.").arg(this->sent).arg(this->failed));
        setBusy(false);
        return;
    }

    sendOne(this->sendQueue.takeFirst());
}

void PushWindow::sendOne(int index)
{
    PushJob job = this->results.at(index);

    QFile file(job.absolutePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        log(QString("  cannot read %1").arg(job.relativePath));
        this->failed++;
        nextFile();
        return;
    }

    QByteArray payload = file.readAll();
    file.close();

    QNetworkRequest request((QUrl(job.target.uploadUrl(job.pack, job.relativePath))));
    job.target.authorise(request);

    QNetworkReply* reply = nullptr;

    if (job.target.github)
    {
        // A commit, not a file write. The sha names the copy being replaced; leaving
        // it out on an existing file is how GitHub is told this is a new one, and
        // sending the wrong one is refused rather than overwriting somebody's work.
        QJsonObject commit;
        commit.insert("message", QString("%1 %2/%3")
            .arg(job.state == PushJob::New ? "Add" : "Update", job.pack, job.relativePath));
        commit.insert("content", QString::fromLatin1(payload.toBase64()));

        if (!job.remoteId.isEmpty())
            commit.insert("sha", job.remoteId);
        if (!job.target.branch.isEmpty())
            commit.insert("branch", job.target.branch);

        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        reply = this->network->put(request, QJsonDocument(commit).toJson(QJsonDocument::Compact));
    }
    else
    {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");

        // What was read off this disk, so the far end can refuse anything else. Both
        // a client and a relay check it, and neither stores a file that fails.
        request.setRawHeader("X-Content-Sha1",
            QCryptographicHash::hash(payload, QCryptographicHash::Sha1).toHex());

        // A client takes a PUT at a path; the relay takes a POST with the path in
        // the query. Same bytes, and only the far end's taste differs.
        reply = job.target.relay ? this->network->post(request, payload)
                                 : this->network->put(request, payload);
    }
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, index, job]() {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // GitHub answers 201 for a file it created and 200 for one it replaced.
        bool ok = (status == 200 || status == 201);

        if (!ok && worthRetrying(status) && index < this->results.count()
            && this->results.at(index).attempts < MAXIMUM_ATTEMPTS)
        {
            // Backing off rather than hammering: a host that just refused a
            // connection is not helped by three more in the same second.
            int attempt = ++this->results[index].attempts;
            int delay = attempt * 2000;

            log(QString("  %1 / %2 \xE2\x80\x94 %3, trying again in %4s (%5 of %6)")
                .arg(job.target.label(), job.relativePath,
                     status == 0 ? reply->errorString() : QString("HTTP %1").arg(status))
                .arg(delay / 1000).arg(attempt).arg(MAXIMUM_ATTEMPTS));

            QTimer::singleShot(delay, this, [this, index]() { sendOne(index); });
            return;
        }

        if (ok)
        {
            this->sent++;
        }
        else
        {
            this->failed++;

            QJsonObject failure = QJsonDocument::fromJson(reply->readAll()).object();

            // A relay says "error"; GitHub says "message". A 409 from GitHub means
            // the file moved under us, which a fresh Compare fixes.
            QString reason = failure.value("error").toString();
            if (reason.isEmpty())
                reason = failure.value("message").toString();
            if (status == 409 && job.target.github)
                reason = "the repository changed since Compare; run Compare again";
            if (status == 413 && reason.isEmpty())
                reason = "the far end would not take a file this size";

            log(QString("  refused %1 / %2 \xE2\x80\x94 %3").arg(job.target.label(), job.relativePath,
                reason.isEmpty() ? reply->errorString() : reason));
        }

        // The row says what happened to it, and a sent file unticks itself so a
        // second press cannot send it twice.
        if (index < this->results.count())
        {
            this->results[index].state = ok ? PushJob::Sent : PushJob::Failed;
            if (index < this->fileTable->rowCount())
            {
                this->fileTable->item(index, 4)->setText(this->results.at(index).stateText());
                this->fileTable->item(index, 4)->setForeground(
                    QBrush(ok ? QColor(110, 170, 110) : QColor(215, 110, 110)));
                if (ok)
                    this->fileTable->item(index, 0)->setCheckState(Qt::Unchecked);
            }
        }

        nextFile();
    });
}

// ---- identify ----

// One request per client, all at once: this is a handful of small reads and the
// answers name themselves, so ordering buys nothing here.
void PushWindow::identifyTargets()
{
    if (this->busy)
        return;

    saveSettings();

    QList<PushTarget> targets = checkedTargets();
    if (targets.isEmpty())
    {
        log("No clients ticked.");
        return;
    }

    this->logView->clear();
    log(QString("Asking %1 client(s) who they are").arg(targets.count()));

    foreach (const PushTarget& target, targets)
    {
        QNetworkRequest request((QUrl(target.infoUrl())));
        target.authorise(request);

        QNetworkReply* reply = this->network->get(request);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, target]() {
            reply->deleteLater();

            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 200)
            {
                QJsonObject info = QJsonDocument::fromJson(reply->readAll()).object();

                if (target.github)
                {
                    // Saying so loudly: templates in a public repository are readable
                    // by anyone, and that is not usually what was meant.
                    log(QString("  %1 \xE2\x86\x92 %2, default branch %3, %4")
                        .arg(target.label(), info.value("full_name").toString(),
                             info.value("default_branch").toString(),
                             info.value("private").toBool()
                                ? "private"
                                : "PUBLIC \xE2\x80\x94 anyone can read these templates"));
                    return;
                }

                if (target.relay)
                {
                    // A relay that answers but will not take an upload is the mistake
                    // worth naming: it means the download token was pasted here.
                    bool canUpload = info.value("canUpload").toBool();
                    log(QString("  %1 \xE2\x86\x92 \"%2\", %3 pack(s), upload %4")
                        .arg(target.label(), info.value("relay").toString())
                        .arg(info.value("packs").toInt())
                        .arg(canUpload ? "allowed" : "REFUSED (this looks like the download token)"));

                    if (canUpload)
                        describeRelayClients(target);

                    return;
                }

                log(QString("  %1 \xE2\x86\x92 %2 on %3, %4 pack(s) in %5")
                    .arg(target.label(), info.value("host").toString(), info.value("os").toString())
                    .arg(info.value("packs").toInt())
                    .arg(info.value("templatesRoot").toString()));
                return;
            }

            // The three an operator will actually hit, named rather than left as a
            // transport error to decipher.
            QString why = (status == 401) ? "wrong or missing token"
                        : (status == 404 && target.github) ? "no such repository, or this token cannot see it"
                        : (status == 403 && target.github) ? "refused by GitHub, usually the rate limit"
                        : (status == 403) ? "template push is switched off on that client"
                        : (status == 429) ? "temporarily refusing this address after repeated wrong tokens"
                        : reply->errorString();
            log(QString("  %1 \xE2\x80\x94 %2").arg(target.label(), why));
        });
    }
}

// ---- clearing what only the far end still has ----

// Deliberately narrow. Only rows Compare marked as existing solely at the far
// end, only ones the operator ticked, and only on a relay. A relay is a staging
// area and its own remove endpoint touches nothing that a client has already
// installed, which is what makes this safe to offer at all.
void PushWindow::removeExtras()
{
    if (this->busy)
        return;

    this->removeQueue.clear();
    int onClients = 0;

    for (int row = 0; row < this->fileTable->rowCount() && row < this->results.count(); row++)
    {
        if (this->results.at(row).state != PushJob::Extra)
            continue;
        if (this->fileTable->item(row, 0)->checkState() != Qt::Checked)
            continue;

        if (!this->results.at(row).target.canRemove())
        {
            onClients++;
            continue;
        }

        this->removeQueue.append(row);
    }

    if (onClients > 0)
    {
        log(QString("%1 ticked file(s) are on clients, not on a relay, and were left alone. "
                    "Remove those on the machine itself.").arg(onClients));
    }

    if (this->removeQueue.isEmpty())
    {
        log("Nothing ticked that this can clear.");
        return;
    }

    // Named and counted before it happens. This is the one button here that
    // destroys something rather than adding to it.
    QMessageBox::StandardButton answer = QMessageBox::question(this, "Clear from the relay",
        QString("Take %1 file(s) off the relay?\n\n"
                "Clients keep whatever they already installed. Only the copy here goes, "
                "so no client will fetch these again.").arg(this->removeQueue.count()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer != QMessageBox::Yes)
    {
        log("Left alone.");
        this->removeQueue.clear();
        return;
    }

    this->sent = 0;
    this->failed = 0;
    log(QString("Clearing %1 file(s) from the relay").arg(this->removeQueue.count()));

    setBusy(true);
    nextRemoval();
}

void PushWindow::nextRemoval()
{
    if (this->removeQueue.isEmpty())
    {
        log(QString("Done. %1 cleared, %2 failed.").arg(this->sent).arg(this->failed));
        setBusy(false);
        return;
    }

    int index = this->removeQueue.takeFirst();
    PushJob job = this->results.at(index);

    QNetworkRequest request((QUrl(job.target.removeUrl(job.pack, job.relativePath))));
    job.target.authorise(request);

    QNetworkReply* reply = nullptr;

    if (job.target.github)
    {
        // Also a commit, and it needs the sha of exactly the copy being removed. The
        // file stays in the repository's history either way, which is the part that
        // makes this the least alarming of the three targets to take things off.
        QJsonObject commit;
        commit.insert("message", QString("Remove %1/%2").arg(job.pack, job.relativePath));
        commit.insert("sha", job.remoteId);
        if (!job.target.branch.isEmpty())
            commit.insert("branch", job.target.branch);

        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        reply = this->network->sendCustomRequest(request, "DELETE",
                                                 QJsonDocument(commit).toJson(QJsonDocument::Compact));
    }
    else
    {
        reply = this->network->post(request, QByteArray());
    }
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, index, job]() {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // 404 means it is already gone, which is the state that was wanted.
        bool ok = (status == 200 || status == 404);

        if (ok)
        {
            this->sent++;
        }
        else
        {
            this->failed++;
            QString reason = QJsonDocument::fromJson(reply->readAll()).object().value("error").toString();
            log(QString("  could not clear %1 \xE2\x80\x94 %2").arg(job.relativePath,
                reason.isEmpty() ? reply->errorString() : reason));
        }

        if (index < this->results.count())
        {
            this->results[index].state = ok ? PushJob::Removed : PushJob::Failed;
            if (index < this->fileTable->rowCount())
            {
                this->fileTable->item(index, 4)->setText(this->results.at(index).stateText());
                this->fileTable->item(index, 4)->setForeground(
                    QBrush(ok ? QColor(140, 140, 140) : QColor(215, 110, 110)));
                if (ok)
                    this->fileTable->item(index, 0)->setCheckState(Qt::Unchecked);
            }
        }

        // Once nothing is marked Extra any more there is nothing left to clear, and
        // the button should say so rather than inviting a second press.
        this->hasExtras = false;
        foreach (const PushJob& remaining, this->results)
        {
            if (remaining.state == PushJob::Extra)
            {
                this->hasExtras = true;
                break;
            }
        }

        nextRemoval();
    });
}

// ---- who has actually picked things up ----

// Pushing to a relay tells you the relay took the file. It does not tell you any
// venue came and got it, and before a show that is the only part worth knowing.
// Clients report what they hold after each poll, and this reads that back.
void PushWindow::describeRelayClients(const PushTarget& target)
{
    QNetworkRequest request((QUrl(relayQuery(target.host, "clients"))));
    target.authorise(request);

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, target]() {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200)
        {
            // An older relay has no such action. Not worth making noise about.
            if (status == 400)
                log("    (this relay does not track check-ins)");

            return;
        }

        QJsonArray clients = QJsonDocument::fromJson(reply->readAll()).object().value("clients").toArray();
        if (clients.isEmpty())
        {
            log("    no client has checked in yet");
            return;
        }

        foreach (const QJsonValue& value, clients)
        {
            QJsonObject client = value.toObject();

            QString ago;
            QDateTime seen = QDateTime::fromString(client.value("seenAt").toString(), Qt::ISODate);
            if (seen.isValid())
            {
                qint64 seconds = seen.secsTo(QDateTime::currentDateTimeUtc());
                ago = (seconds < 60) ? QString("just now")
                    : (seconds < 3600) ? QString("%1m ago").arg(seconds / 60)
                    : (seconds < 86400) ? QString("%1h ago").arg(seconds / 3600)
                    : QString("%1d ago").arg(seconds / 86400);
            }

            // Naming the packs rather than just saying "behind": which pack is
            // stale decides whether it matters before this particular show.
            QStringList behind;
            foreach (const QJsonValue& name, client.value("behind").toArray())
                behind.append(name.toString());

            // Columns, because this is read as a list of machines rather than
            // as sentences.
            int failed = client.value("failed").toInt();

            // Failures first. A client that could install nothing reports no packs,
            // so judging only by the pack list would call it current.
            QString state = (failed > 0) ? QString("FAILING")
                          : behind.isEmpty() ? QString("current")
                          : QString("behind on %1").arg(behind.join(", "));

            // The reason, when there is one worth reading. A venue that is current
            // does not need its last summary quoted back.
            QString result = client.value("result").toString();
            if (!result.isEmpty() && (failed > 0 || !behind.isEmpty()))
                state += QString("  (%1)").arg(result);

            log(QString("    %1  %2  %3")
                .arg(client.value("host").toString(), -22)
                .arg(ago, -10)
                .arg(state));
        }
    });
}
