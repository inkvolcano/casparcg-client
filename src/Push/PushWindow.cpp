#include "PushWindow.h"

#include <QtCore/QCryptographicHash>
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

    // A request that never answers would otherwise stall the whole queue. Across
    // a local network this never fires; across the internet it is the difference
    // between a slow push and one that appears to have died.
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

QString PushTarget::base() const
{
    return this->relay ? this->host : QString("http://%1:%2").arg(this->host).arg(this->port);
}

QString PushTarget::label() const
{
    if (!this->name.isEmpty())
        return this->name;

    return this->relay ? QString("relay %1").arg(this->host) : base();
}

QString PushTarget::infoUrl() const
{
    return this->relay ? relayQuery(this->host, "ping")
                       : QString("%1/templates/info").arg(base());
}

QString PushTarget::manifestUrl(const QString& pack) const
{
    if (this->relay)
        return QString("%1&pack=%2").arg(relayQuery(this->host, "manifest"), encoded(pack));

    return QString("%1/templates/%2").arg(base(), pack);
}

QString PushTarget::uploadUrl(const QString& pack, const QString& relativePath) const
{
    if (this->relay)
    {
        return QString("%1&pack=%2&path=%3").arg(relayQuery(this->host, "upload"),
                                                 encoded(pack), encoded(relativePath, "/"));
    }

    return QString("%1/templates/%2/%3").arg(base(), pack, encoded(relativePath, "/"));
}

QByteArray PushTarget::tokenHeader() const
{
    return this->relay ? QByteArray("X-Relay-Token") : QByteArray("X-Template-Token");
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
        "A client: host:port, the same port it hosts its sheet cache on, with the token\n"
        "from its Settings -> Templates.\n\n"
        "A relay: the full address of relay.php, with that relay's UPLOAD token. Use this\n"
        "for any client you cannot reach directly. Those clients pull from the relay\n"
        "themselves, so nothing has to reach in to them.");
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
        this->targetTable->setItem(row, 2, new QTableWidgetItem(
            host.contains("://") ? host
                                 : QString("%1:%2").arg(host).arg(entry.value("port").toInt(3000))));
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
        bool relay = address.contains("://");
        int colon = relay ? -1 : address.lastIndexOf(':');

        QJsonObject entry;
        entry.insert("send", this->targetTable->item(row, 0) && this->targetTable->item(row, 0)->checkState() == Qt::Checked);
        entry.insert("name", this->targetTable->item(row, 1) ? this->targetTable->item(row, 1)->text() : QString());
        entry.insert("host", colon > 0 ? address.left(colon) : address);
        entry.insert("port", colon > 0 ? address.mid(colon + 1).toInt() : 3000);
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

        // A scheme means a relay. Splitting a URL on its last colon would otherwise
        // read "https" as the host, which is a confusing way to find out.
        target.relay = address.contains("://");
        if (target.relay)
        {
            target.host = address;
        }
        else
        {
            int colon = address.lastIndexOf(':');
            target.host = colon > 0 ? address.left(colon) : address;
            target.port = colon > 0 ? address.mid(colon + 1).toInt() : 3000;
        }

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

QMap<QString, QString> PushWindow::localFiles(const QString& pack) const
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

        files.insert(relative, QString::fromLatin1(
            QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha1).toHex()));
        file.close();
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
    request.setRawHeader(target.tokenHeader(), target.token.toUtf8());

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
                        : reply->errorString();
            log(QString("  %1 / %2 \xE2\x80\x94 %3").arg(target.label(), pack, why));
            nextPair();
            return;
        }

        QMap<QString, QString> remote;
        foreach (const QJsonValue& value, QJsonDocument::fromJson(body).object().value("files").toArray())
        {
            QJsonObject entry = value.toObject();
            remote.insert(entry.value("path").toString(), entry.value("sha1").toString());
        }

        QMap<QString, QString> local = localFiles(pack);
        if (local.isEmpty())
        {
            log(QString("  %1 / %2 \xE2\x80\x94 no such pack in the templates folder").arg(target.label(), pack));
            nextPair();
            return;
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

            if (!remote.contains(relative))
                job.state = PushJob::New;
            else if (remote.value(relative) != local.value(relative))
                job.state = PushJob::Changed;
            else
                job.state = PushJob::Unchanged;

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
    foreach (const PushJob& job, this->results)
    {
        int row = this->fileTable->rowCount();
        this->fileTable->insertRow(row);

        bool wanted = (job.state == PushJob::New || job.state == PushJob::Changed);
        if (wanted)
            toSend++;

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
    }

    log(QString("%1 file(s) listed, %2 ticked to send, %3 already current.")
        .arg(this->results.count()).arg(toSend).arg(this->results.count() - toSend));
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
        if (this->fileTable->item(row, 0)->checkState() == Qt::Checked)
            this->sendQueue.append(row);

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
    request.setRawHeader(job.target.tokenHeader(), job.target.token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");

    // A client takes a PUT at a path; the relay takes a POST with the path in the
    // query. Same bytes, and the only thing that differs is the far end's taste.
    QNetworkReply* reply = job.target.relay ? this->network->post(request, payload)
                                            : this->network->put(request, payload);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, index, job]() {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool ok = (status == 200);

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
            QString reason = QJsonDocument::fromJson(reply->readAll()).object().value("error").toString();
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
        request.setRawHeader(target.tokenHeader(), target.token.toUtf8());

        QNetworkReply* reply = this->network->get(request);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, target]() {
            reply->deleteLater();

            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 200)
            {
                QJsonObject info = QJsonDocument::fromJson(reply->readAll()).object();

                if (target.relay)
                {
                    // A relay that answers but will not take an upload is the mistake
                    // worth naming: it means the download token was pasted here.
                    log(QString("  %1 \xE2\x86\x92 \"%2\", %3 pack(s), upload %4")
                        .arg(target.label(), info.value("relay").toString())
                        .arg(info.value("packs").toInt())
                        .arg(info.value("canUpload").toBool() ? "allowed"
                                                              : "REFUSED (this looks like the download token)"));
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
                        : (status == 403) ? "template push is switched off on that client"
                        : (status == 429) ? "temporarily refusing this address after repeated wrong tokens"
                        : reply->errorString();
            log(QString("  %1 \xE2\x80\x94 %2").arg(target.label(), why));
        });
    }
}
