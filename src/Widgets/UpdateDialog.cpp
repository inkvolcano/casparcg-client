#include "UpdateDialog.h"

#include "ClientRelease.h"
#include "DatabaseManager.h"
#include "Version.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QUrl>

#include <QtGui/QDesktopServices>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

namespace
{
    // github.com unless a GitHub Enterprise Server is configured. The same setting
    // the template route uses, because a venue with a self-hosted GitHub has one of
    // them rather than one per feature.
    QString apiBase()
    {
        const QString configured = DatabaseManager::getInstance()
            .getConfigurationByName("RelayGitHubApi").getValue().trimmed();

        return configured.isEmpty() ? QString("https://api.github.com") : configured;
    }
}

QString UpdateDialog::source()
{
    return DatabaseManager::getInstance().getConfigurationByName("UpdateSource").getValue().trimmed();
}

QString UpdateDialog::token()
{
    return DatabaseManager::getInstance().getConfigurationByName("UpdateToken").getValue().trimmed();
}

bool UpdateDialog::isGitHub()
{
    return source().startsWith("github:", Qt::CaseInsensitive);
}

QString UpdateDialog::gitHubOwnerRepo()
{
    if (!isGitHub())
        return QString();

    QString rest = source().mid(QString("github:").length()).trimmed();

    // A branch means nothing to a release, but somebody will paste the template
    // source in here, and quietly asking GitHub for "owner/repo@main" would 404
    // with nothing to explain it.
    return rest.section('@', 0, 0);
}

QString UpdateDialog::runningVersionText()
{
    return QString("%1.%2.%3 build %4")
        .arg(MAJOR_VERSION).arg(MINOR_VERSION).arg(REVISION_VERSION).arg(DEV_BUILD_ID);
}

QString UpdateDialog::stagingFolder()
{
    // Beside the application. A 106 MB download that a temp cleaner removed before
    // anybody installed it is a second 106 MB download.
    QDir dir(QCoreApplication::applicationDirPath());
    dir.mkpath("updates");

    return dir.absoluteFilePath("updates");
}

UpdateDialog::UpdateDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Check for Updates");
    setMinimumWidth(560);

    this->network = new QNetworkAccessManager(this);

    QVBoxLayout* outer = new QVBoxLayout(this);

    QGridLayout* grid = new QGridLayout();
    grid->setColumnStretch(1, 1);

    grid->addWidget(new QLabel("Running:", this), 0, 0);
    this->labelRunning = new QLabel(runningVersionText(), this);
    this->labelRunning->setStyleSheet("font-weight: bold;");
    grid->addWidget(this->labelRunning, 0, 1);

    grid->addWidget(new QLabel("Source:", this), 1, 0);
    this->labelSource = new QLabel(this);
    grid->addWidget(this->labelSource, 1, 1);

    outer->addLayout(grid);

    this->labelResult = new QLabel(this);
    this->labelResult->setWordWrap(true);
    this->labelResult->setMinimumHeight(34);
    outer->addWidget(this->labelResult);

    this->textNotes = new QTextEdit(this);
    this->textNotes->setReadOnly(true);
    this->textNotes->setMinimumHeight(120);
    this->textNotes->setVisible(false);
    outer->addWidget(this->textNotes, 1);

    this->progress = new QProgressBar(this);
    this->progress->setVisible(false);
    outer->addWidget(this->progress);

    QDialogButtonBox* buttons = new QDialogButtonBox(this);

    this->buttonCheck = buttons->addButton("Check Now", QDialogButtonBox::ActionRole);
    this->buttonDownload = buttons->addButton("Download", QDialogButtonBox::ActionRole);
    this->buttonReveal = buttons->addButton("Show Download", QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);

    this->buttonDownload->setEnabled(false);
    this->buttonReveal->setEnabled(false);

    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(this->buttonCheck, &QPushButton::clicked, this, [this]() { check(); });
    QObject::connect(this->buttonDownload, &QPushButton::clicked, this, [this]() { download(); });
    QObject::connect(this->buttonReveal, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(stagingFolder()));
    });

    outer->addWidget(buttons);

    if (source().isEmpty())
    {
        this->labelSource->setText("not set");
        say("No update source is set. Settings → Templates → Update Source, "
            "as github:owner/repo.", true);
        this->buttonCheck->setEnabled(false);
    }
    else if (!isGitHub())
    {
        this->labelSource->setText(source());
        say("Only a github:owner/repo source is understood so far.", true);
        this->buttonCheck->setEnabled(false);
    }
    else
    {
        this->labelSource->setText(gitHubOwnerRepo());
        say("Nothing has been checked yet.");
    }
}

void UpdateDialog::say(const QString& text, bool problem)
{
    this->labelResult->setText(text);
    this->labelResult->setStyleSheet(problem ? "color: rgb(230, 140, 140);" : QString());
}

void UpdateDialog::setBusy(bool busy)
{
    this->buttonCheck->setEnabled(!busy && isGitHub() && !source().isEmpty());
    this->buttonDownload->setEnabled(!busy && !this->assetUrl.isEmpty());
    this->progress->setVisible(busy);

    if (!busy)
        this->progress->setValue(0);
}

// ---- is there anything newer -----------------------------------------------

void UpdateDialog::check()
{
    this->foundTag.clear();
    this->assetName.clear();
    this->assetUrl.clear();
    this->sumsUrl.clear();
    this->expectedSha.clear();
    this->textNotes->setVisible(false);

    setBusy(true);
    this->progress->setRange(0, 0);   // indeterminate: this is one small request
    say("Asking " + gitHubOwnerRepo() + "...");

    QNetworkRequest request((QUrl(QString("%1/repos/%2/releases?per_page=20")
        .arg(apiBase(), gitHubOwnerRepo()))));

    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setRawHeader("User-Agent", "CasparCG-Client");

    // A public repository needs none, which is the ordinary case for builds.
    if (!token().isEmpty())
        request.setRawHeader("Authorization", QByteArray("Bearer ") + token().toUtf8());

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        setBusy(false);

        if (reply->error() != QNetworkReply::NoError)
        {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            if (status == 404)
                say("No such repository, or it is private and needs a token.", true);
            else if (status == 401 || status == 403)
                say("The token was refused. It needs read access to that repository.", true);
            else
                say("Could not reach it: " + reply->errorString(), true);

            return;
        }

        const QJsonArray releases = QJsonDocument::fromJson(reply->readAll()).array();
        if (releases.isEmpty())
        {
            say("That repository has published no releases yet.", true);
            return;
        }

        const ClientRelease::Version running = ClientRelease::parse(runningVersionText());

        // The newest published release, which is not necessarily the first: a draft
        // is not published and a pre-release is not something to offer a venue.
        ClientRelease::Version best;
        QJsonObject bestRelease;

        foreach (const QJsonValue& value, releases)
        {
            const QJsonObject release = value.toObject();
            if (release.value("draft").toBool() || release.value("prerelease").toBool())
                continue;

            const ClientRelease::Version candidate =
                ClientRelease::parse(release.value("tag_name").toString());

            if (!candidate.valid)
                continue;

            if (!best.valid || ClientRelease::isNewer(candidate, best))
            {
                best = candidate;
                bestRelease = release;
            }
        }

        if (!best.valid)
        {
            say("No release there has a version this understands.", true);
            return;
        }

        if (!ClientRelease::isNewer(best, running))
        {
            say(QString("Up to date. The newest published build is %1.")
                .arg(ClientRelease::describe(best)));
            return;
        }

        // Which asset belongs on this machine.
        QStringList names;
        const QJsonArray assets = bestRelease.value("assets").toArray();
        foreach (const QJsonValue& value, assets)
            names.append(value.toObject().value("name").toString());

        const QString wanted = ClientRelease::assetFor(ClientRelease::platformKey(), names);

        this->foundTag = bestRelease.value("tag_name").toString();

        const QString notes = bestRelease.value("body").toString();
        if (!notes.isEmpty())
        {
            this->textNotes->setPlainText(notes);
            this->textNotes->setVisible(true);
        }

        if (wanted.isEmpty())
        {
            say(QString("%1 is available, but it publishes nothing for %2.")
                .arg(ClientRelease::describe(best), ClientRelease::platformKey()), true);
            return;
        }

        foreach (const QJsonValue& value, assets)
        {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value("name").toString();

            if (name == wanted)
            {
                this->assetName = name;
                this->assetUrl = asset.value("url").toString();
            }

            // What makes the download verifiable. Without it the package is refused
            // rather than installed on trust.
            if (name.compare("SHA256SUMS.txt", Qt::CaseInsensitive) == 0)
                this->sumsUrl = asset.value("url").toString();
        }

        say(QString("%1 is available. You are on %2.")
            .arg(ClientRelease::describe(best), ClientRelease::describe(running)));

        this->buttonDownload->setEnabled(true);
    });
}

// ---- fetch it --------------------------------------------------------------

void UpdateDialog::download()
{
    if (this->assetUrl.isEmpty())
        return;

    if (this->sumsUrl.isEmpty())
    {
        say("That release publishes no SHA256SUMS.txt, so the download cannot be "
            "verified. Refusing rather than installing on trust.", true);
        return;
    }

    setBusy(true);
    say("Fetching the checksums...");
    requestSums();
}

void UpdateDialog::requestSums()
{
    QNetworkRequest request((QUrl(this->sumsUrl)));
    request.setRawHeader("Accept", "application/octet-stream");   // the bytes, not JSON
    request.setRawHeader("User-Agent", "CasparCG-Client");

    if (!token().isEmpty())
        request.setRawHeader("Authorization", QByteArray("Bearer ") + token().toUtf8());

    QNetworkReply* reply = this->network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
        {
            setBusy(false);
            say("Could not fetch the checksums: " + reply->errorString(), true);
            return;
        }

        this->expectedSha = ClientRelease::sha256For(this->assetName,
                                                     QString::fromUtf8(reply->readAll()));

        if (this->expectedSha.isEmpty())
        {
            setBusy(false);
            say(QString("The checksums file has no usable entry for %1, so there is "
                        "nothing to verify against.").arg(this->assetName), true);
            return;
        }

        say(QString("Downloading %1...").arg(this->assetName));
        requestAsset();
    });
}

void UpdateDialog::requestAsset()
{
    QNetworkRequest request((QUrl(this->assetUrl)));
    request.setRawHeader("Accept", "application/octet-stream");
    request.setRawHeader("User-Agent", "CasparCG-Client");

    if (!token().isEmpty())
        request.setRawHeader("Authorization", QByteArray("Bearer ") + token().toUtf8());

    QNetworkReply* reply = this->network->get(request);

    this->progress->setRange(0, 100);

    QObject::connect(reply, &QNetworkReply::downloadProgress, this,
                     [this](qint64 received, qint64 total) {
        if (total > 0)
            this->progress->setValue(static_cast<int>((received * 100) / total));
    });

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        setBusy(false);

        if (reply->error() != QNetworkReply::NoError)
        {
            say("The download failed: " + reply->errorString(), true);
            return;
        }

        const QByteArray payload = reply->readAll();

        // Hashed before anything is written. A package that does not match what the
        // release published is not written to disk at all, so there is never a file
        // sitting there that somebody could install by hand later.
        const QString actual = QString::fromLatin1(
            QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex()).toLower();

        if (actual != this->expectedSha)
        {
            say("The download does not match the checksum the release published, so "
                "it has been thrown away. Nothing was written.", true);
            return;
        }

        const QString path = QDir(stagingFolder()).absoluteFilePath(this->assetName);

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
        {
            say("Verified, but could not be written to " + stagingFolder(), true);
            return;
        }

        file.write(payload);
        file.close();

        this->downloadedPath = path;
        this->buttonReveal->setEnabled(true);

        // And it stops here, on purpose. Windows will not overwrite a running
        // executable, and on a machine that may be on air the moment to replace the
        // client belongs to whoever is standing in front of it.
        say(QString("Downloaded and verified.\n\nIt is in %1. Close the client, "
                    "unpack it over the installation, and start the new one.")
            .arg(stagingFolder()));
    });
}
