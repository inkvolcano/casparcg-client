#include "AssignmentsDialog.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QUrl>

#include <QtGui/QBrush>
#include <QtGui/QColor>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QVBoxLayout>

namespace
{
    // The row that answers for every machine the file does not name.
    const char* ANY = "*";

    QString encoded(const QString& value, const QByteArray& keep = QByteArray())
    {
        return QString::fromUtf8(QUrl::toPercentEncoding(value, keep));
    }

    QString relayQuery(const QString& url, const QString& action)
    {
        return url + (url.contains('?') ? "&action=" : "?action=") + action;
    }
}

AssignmentsDialog::AssignmentsDialog(const PushTarget& target, const QStringList& localPacks,
                                     QWidget* parent)
    : QDialog(parent), target(target), localPacks(localPacks)
{
    this->network = new QNetworkAccessManager(this);
    this->network->setTransferTimeout(20000);

    setWindowTitle(QString("Assignments \xE2\x80\x94 %1").arg(target.label()));
    resize(760, 460);

    buildUi();
    load();
}

void AssignmentsDialog::buildUi()
{
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(8);

    QLabel* explain = new QLabel(
        "A tick means that machine takes that pack. The * row is what a machine gets\n"
        "when it is not listed here. A machine listed with nothing ticked takes nothing,\n"
        "which is not the same as leaving it off the list entirely.", this);
    explain->setStyleSheet("color: rgba(150, 150, 150, 220);");
    outer->addWidget(explain);

    this->grid = new QTableWidget(0, 0, this);
    this->grid->verticalHeader()->setVisible(false);
    this->grid->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->grid->setSelectionBehavior(QAbstractItemView::SelectRows);
    outer->addWidget(this->grid, 1);

    this->status = new QLabel(this);
    this->status->setWordWrap(true);

    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(this->status, 1);

    this->addButton = new QPushButton("Add machine...", this);
    this->addButton->setToolTip(
        "For a machine that has not checked in yet, or one you are setting up before\n"
        "it exists. The name is its machine name, exactly as Windows reports it.");
    QObject::connect(this->addButton, &QPushButton::clicked, this, &AssignmentsDialog::addMachine);
    buttons->addWidget(this->addButton);

    this->removeButton = new QPushButton("Remove", this);
    this->removeButton->setToolTip(
        "Take the selected machine off the list. It then follows the * row, or its own\n"
        "local setting if there is no * row. It does not stop that machine updating.");
    QObject::connect(this->removeButton, &QPushButton::clicked, this, &AssignmentsDialog::removeMachine);
    buttons->addWidget(this->removeButton);

    this->loadButton = new QPushButton("Reload", this);
    this->loadButton->setToolTip("Throw away what is on screen and ask the source again.");
    QObject::connect(this->loadButton, &QPushButton::clicked, this, &AssignmentsDialog::load);
    buttons->addWidget(this->loadButton);

    this->saveButton = new QPushButton("Save", this);
    this->saveButton->setToolTip("Write this back. Nothing is written until you press it.");
    QObject::connect(this->saveButton, &QPushButton::clicked, this, &AssignmentsDialog::save);
    buttons->addWidget(this->saveButton);

    QPushButton* close = new QPushButton("Close", this);
    QObject::connect(close, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(close);

    outer->addLayout(buttons);
}

void AssignmentsDialog::say(const QString& line, bool bad)
{
    this->status->setText(line);
    this->status->setStyleSheet(bad ? "color: rgb(215, 110, 110);"
                                    : "color: rgba(170, 170, 170, 220);");
}

void AssignmentsDialog::setBusy(bool value)
{
    this->busy = value;
    this->loadButton->setEnabled(!value);
    this->addButton->setEnabled(!value);
    this->removeButton->setEnabled(!value);

    // Saving before the first load has finished would write an empty grid over
    // whatever is really there.
    this->saveButton->setEnabled(!value && this->loaded);
}

// ---- reading what is there ----

void AssignmentsDialog::load()
{
    if (this->busy)
        return;

    this->assignments = QJsonObject();
    this->machines.clear();
    this->packs.clear();
    this->seenMachines.clear();
    this->gitHubBlobSha.clear();
    this->loaded = false;

    setBusy(true);
    say("Reading...");

    if (this->target.github)
        loadFromGitHub();
    else
        loadFromRelay();
}

void AssignmentsDialog::loadFromRelay()
{
    // The manifest carries the assignments and the packs together, and the client
    // list says which machines are actually out there.
    QNetworkRequest request((QUrl(relayQuery(this->target.host, "manifest"))));
    this->target.authorise(request);

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (http != 200)
        {
            setBusy(false);
            say(http == 401 ? "The relay refused that token."
                            : QString("Could not read the relay: %1").arg(reply->errorString()), true);
            return;
        }

        QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();
        this->assignments = answer.value("assignments").toObject();

        foreach (const QJsonValue& value, answer.value("packs").toArray())
        {
            QString name = value.toObject().value("name").toString();
            if (!name.isEmpty() && !this->packs.contains(name))
                this->packs.append(name);
        }

        // Who has actually reported in. A relay knows; a repository cannot.
        QNetworkRequest clients((QUrl(relayQuery(this->target.host, "clients"))));
        this->target.authorise(clients);

        QNetworkReply* second = this->network->get(clients);
        QObject::connect(second, &QNetworkReply::finished, this, [this, second]() {
            second->deleteLater();

            if (second->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200)
            {
                foreach (const QJsonValue& value,
                         QJsonDocument::fromJson(second->readAll()).object().value("clients").toArray())
                {
                    QString host = value.toObject().value("host").toString();
                    if (!host.isEmpty() && !this->seenMachines.contains(host))
                        this->seenMachines.append(host);
                }
            }

            buildGrid();
        });
    });
}

void AssignmentsDialog::loadFromGitHub()
{
    QNetworkRequest request((QUrl(this->target.manifestUrl(QString()))));
    this->target.authorise(request);

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (http != 200)
        {
            setBusy(false);
            say(http == 404 ? "No such repository, or that token cannot see it."
                            : QString("Could not read the repository: %1").arg(reply->errorString()), true);
            return;
        }

        QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        if (root.value("truncated").toBool())
        {
            setBusy(false);
            say("That repository is too large for one tree listing.", true);
            return;
        }

        // A pack is a folder at the root, which is the same rule a client follows.
        foreach (const QJsonValue& value, root.value("tree").toArray())
        {
            QJsonObject entry = value.toObject();
            QString path = entry.value("path").toString();

            if (entry.value("type").toString() == "blob"
                && path.compare("assignments.json", Qt::CaseInsensitive) == 0)
            {
                this->gitHubBlobSha = entry.value("sha").toString();
                continue;
            }

            if (path.contains('/') || path.startsWith('.'))
                continue;

            if (entry.value("type").toString() == "tree" && !this->packs.contains(path))
                this->packs.append(path);
        }

        if (this->gitHubBlobSha.isEmpty())
        {
            // No file yet. That is a repository nobody has assigned anything in, not
            // a failure, and saving will create it.
            buildGrid();
            return;
        }

        requestGitHubBlob(this->gitHubBlobSha);
    });
}

void AssignmentsDialog::requestGitHubBlob(const QString& sha)
{
    QNetworkRequest request((QUrl(QString("%1/git/blobs/%2").arg(this->target.base(), sha))));
    this->target.authorise(request);
    request.setRawHeader("Accept", "application/vnd.github.raw");

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200)
        {
            QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
            if (document.isObject())
                this->assignments = document.object();
            else
                say("assignments.json is not a JSON object; starting from empty.", true);
        }

        buildGrid();
    });
}

// Everything gathered, turned into a grid.
void AssignmentsDialog::buildGrid()
{
    // Machines from every source that knows any, so a row is never missing just
    // because one of them had not heard of it.
    this->machines.clear();
    this->machines.append(ANY);

    foreach (const QString& key, this->assignments.keys())
    {
        if (key != ANY && !this->machines.contains(key))
            this->machines.append(key);
    }

    foreach (const QString& seen, this->seenMachines)
    {
        if (!this->machines.contains(seen))
            this->machines.append(seen);
    }

    // Packs likewise: the source's, the ones already assigned, and the dev
    // machine's own, so something can be assigned before it is ever uploaded.
    foreach (const QString& key, this->assignments.keys())
    {
        foreach (const QJsonValue& value, this->assignments.value(key).toArray())
        {
            QString pack = value.toString();
            if (!pack.isEmpty() && !this->packs.contains(pack))
                this->packs.append(pack);
        }
    }

    foreach (const QString& local, this->localPacks)
    {
        if (!this->packs.contains(local))
            this->packs.append(local);
    }

    this->packs.sort();

    this->grid->clear();
    this->grid->setRowCount(this->machines.count());
    this->grid->setColumnCount(this->packs.count() + 1);

    QStringList headers;
    headers << "Machine";
    headers += this->packs;
    this->grid->setHorizontalHeaderLabels(headers);

    for (int row = 0; row < this->machines.count(); row++)
    {
        QString machine = this->machines.at(row);

        QTableWidgetItem* name = new QTableWidgetItem(machine);
        name->setFlags(name->flags() & ~Qt::ItemIsEditable);

        if (machine == ANY)
        {
            name->setText("*  (any other machine)");
            name->setForeground(QBrush(QColor(150, 170, 210)));
        }
        else if (!this->seenMachines.contains(machine))
        {
            // Named here but never heard from. Worth showing, because a typo in a
            // machine name looks exactly like a machine that is switched off.
            name->setForeground(QBrush(QColor(160, 160, 160)));
            name->setToolTip("This machine has not checked in, so either it is off, "
                             "it does not pull from here, or the name is wrong.");
        }

        this->grid->setItem(row, 0, name);

        QStringList assigned;
        foreach (const QJsonValue& value, this->assignments.value(machine).toArray())
            assigned.append(value.toString());

        for (int column = 0; column < this->packs.count(); column++)
        {
            QTableWidgetItem* cell = new QTableWidgetItem();
            cell->setFlags((cell->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
            cell->setCheckState(assigned.contains(this->packs.at(column)) ? Qt::Checked : Qt::Unchecked);
            this->grid->setItem(row, column + 1, cell);
        }
    }

    this->grid->resizeColumnsToContents();

    this->loaded = true;
    setBusy(false);

    QString where = this->target.github ? "repository" : "relay";
    say(QString("%1 machine(s), %2 pack(s) on the %3. Nothing is written until you press Save.")
        .arg(this->machines.count() - 1).arg(this->packs.count()).arg(where));
}

// ---- writing it back ----

QJsonObject AssignmentsDialog::collect() const
{
    QJsonObject map;

    for (int row = 0; row < this->grid->rowCount(); row++)
    {
        QString machine = this->machines.value(row);
        if (machine.isEmpty())
            continue;

        QJsonArray assigned;
        for (int column = 0; column < this->packs.count(); column++)
        {
            QTableWidgetItem* cell = this->grid->item(row, column + 1);
            if (cell != nullptr && cell->checkState() == Qt::Checked)
                assigned.append(this->packs.at(column));
        }

        map.insert(machine, assigned);
    }

    return map;
}

void AssignmentsDialog::save()
{
    if (this->busy || !this->loaded)
        return;

    QJsonObject map = collect();

    // A row with nothing ticked means that machine takes nothing. That is a real
    // instruction and worth being sure about, because it is one tick away from a
    // machine that simply stops receiving anything.
    QStringList empties;
    foreach (const QString& key, map.keys())
    {
        if (map.value(key).toArray().isEmpty())
            empties.append(key == ANY ? QString("* (any other machine)") : key);
    }

    if (!empties.isEmpty())
    {
        QMessageBox::StandardButton answer = QMessageBox::question(this, "Machines taking nothing",
            QString("%1 row(s) have nothing ticked, which tells those machines to take "
                    "no packs at all:\n\n    %2\n\nSave anyway?")
                .arg(empties.count()).arg(empties.join("\n    ")),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (answer != QMessageBox::Yes)
        {
            say("Left alone.");
            return;
        }
    }

    setBusy(true);
    say("Saving...");

    QByteArray body = QJsonDocument(map).toJson(QJsonDocument::Indented);

    if (this->target.github)
        saveToGitHub(body);
    else
        saveToRelay(body);
}

void AssignmentsDialog::saveToRelay(const QByteArray& body)
{
    QNetworkRequest request((QUrl(relayQuery(this->target.host, "assignments"))));
    this->target.authorise(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = this->network->post(request, body);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        setBusy(false);

        int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();

        if (http != 200)
        {
            say(http == 401 ? "That token cannot change assignments; the upload token is needed."
                            : QString("The relay refused it: %1").arg(answer.value("error").toString()), true);
            return;
        }

        // The relay says what it would not take. Both of these are silent failures
        // otherwise, so they are worth putting in front of somebody.
        QStringList notes;
        notes << QString("Saved. %1 machine(s).").arg(answer.value("clients").toInt());

        QStringList dropped;
        foreach (const QJsonValue& value, answer.value("dropped").toArray())
            dropped.append(value.toString());
        if (!dropped.isEmpty())
            notes << QString("Refused: %1.").arg(dropped.join(", "));

        QStringList unknown;
        foreach (const QJsonValue& value, answer.value("unknownPacks").toArray())
            unknown.append(value.toString());
        if (!unknown.isEmpty())
            notes << QString("Not on the relay yet: %1.").arg(unknown.join(", "));

        say(notes.join("  "), !dropped.isEmpty());
    });
}

void AssignmentsDialog::saveToGitHub(const QByteArray& body)
{
    QJsonObject commit;
    commit.insert("message", "Update template assignments");
    commit.insert("content", QString::fromLatin1(body.toBase64()));

    // Replacing a file needs the blob being replaced, or GitHub reads it as an
    // attempt to create one that is already there.
    if (!this->gitHubBlobSha.isEmpty())
        commit.insert("sha", this->gitHubBlobSha);
    if (!this->target.branch.isEmpty())
        commit.insert("branch", this->target.branch);

    QNetworkRequest request((QUrl(QString("%1/contents/assignments.json").arg(this->target.base()))));
    this->target.authorise(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = this->network->put(request, QJsonDocument(commit).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        setBusy(false);

        int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();

        if (http != 200 && http != 201)
        {
            QString why = answer.value("message").toString();
            if (http == 409 || http == 422)
                why = "the file changed in the repository since this was loaded; press Reload";
            if (http == 403 || http == 404)
                why = "that token cannot write to this repository";

            say(QString("GitHub refused it: %1").arg(why.isEmpty() ? reply->errorString() : why), true);
            return;
        }

        // The new blob, so a second save in the same sitting does not conflict with
        // the commit this one just made.
        this->gitHubBlobSha = answer.value("content").toObject().value("sha").toString();

        say("Saved. Clients pick it up on their next poll.");
    });
}

// ---- the two buttons that change the shape of the grid ----

void AssignmentsDialog::addMachine()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, "Add machine",
        "Machine name, exactly as that computer reports it:", QLineEdit::Normal, QString(), &ok).trimmed();

    if (!ok || name.isEmpty())
        return;

    if (this->machines.contains(name, Qt::CaseInsensitive))
    {
        say(QString("%1 is already listed.").arg(name), true);
        return;
    }

    this->machines.append(name);

    int row = this->grid->rowCount();
    this->grid->insertRow(row);

    QTableWidgetItem* item = new QTableWidgetItem(name);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setForeground(QBrush(QColor(160, 160, 160)));
    item->setToolTip("Added by hand. It has not checked in.");
    this->grid->setItem(row, 0, item);

    for (int column = 0; column < this->packs.count(); column++)
    {
        QTableWidgetItem* cell = new QTableWidgetItem();
        cell->setFlags((cell->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        cell->setCheckState(Qt::Unchecked);
        this->grid->setItem(row, column + 1, cell);
    }

    say(QString("%1 added. Tick what it should take, then Save.").arg(name));
}

void AssignmentsDialog::removeMachine()
{
    int row = this->grid->currentRow();
    if (row < 0)
    {
        say("Select a machine first.", true);
        return;
    }

    QString machine = this->machines.value(row);
    if (machine == ANY)
    {
        say("The * row cannot be removed. Untick everything in it instead, "
            "which means an unlisted machine takes nothing.", true);
        return;
    }

    this->machines.removeAt(row);
    this->grid->removeRow(row);

    say(QString("%1 removed. It will follow the * row, or its own local setting.").arg(machine));
}
