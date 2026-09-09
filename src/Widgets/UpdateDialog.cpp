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
#include <QtCore/QProcess>

#include <QtGui/QDesktopServices>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QMessageBox>
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
    // A stored value is honoured only if it is on the allowlist. Anything else is
    // ignored rather than obeyed - a client whose configuration was edited to point
    // somewhere else goes on checking the place it is supposed to, instead of
    // quietly downloading a program from wherever the row now says.
    const QString stored = DatabaseManager::getInstance()
        .getConfigurationByName("UpdateSource").getValue().trimmed();

    if (!stored.isEmpty() && ClientRelease::isAllowedSource(stored))
        return ClientRelease::normaliseSource(stored);

    return ClientRelease::defaultSource();
}

// Whether the stored value was one this build will use. Only for saying so out
// loud: source() has already fallen back by the time anyone asks.
bool UpdateDialog::sourceWasOverridden()
{
    const QString stored = DatabaseManager::getInstance()
        .getConfigurationByName("UpdateSource").getValue().trimmed();

    return !stored.isEmpty() && !ClientRelease::isAllowedSource(stored);
}

QString UpdateDialog::token()
{
    return DatabaseManager::getInstance().getConfigurationByName("UpdateToken").getValue().trimmed();
}

bool UpdateDialog::isGitHub()
{
    // Always, now that the source is a fixed list rather than something typed.
    // Kept as a question because the answer will not always be yes if a route that
    // is not GitHub is ever added to the list.
    return !source().isEmpty();
}

QString UpdateDialog::gitHubOwnerRepo()
{
    return source();
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
    this->buttonInstall = buttons->addButton("Install and Restart", QDialogButtonBox::ActionRole);
    this->buttonReveal = buttons->addButton("Show Download", QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);

    this->buttonDownload->setEnabled(false);
    this->buttonInstall->setEnabled(false);
    this->buttonReveal->setEnabled(false);

    this->buttonInstall->setToolTip(
        "Close the client, copy the new build over this installation, and start it again.\n\n"
        "Only available once a download has been verified against the checksum the\n"
        "release published. The build you are running is kept so it can be put back.");

    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(this->buttonCheck, &QPushButton::clicked, this, [this]() { check(); });
    QObject::connect(this->buttonDownload, &QPushButton::clicked, this, [this]() { download(); });
    QObject::connect(this->buttonInstall, &QPushButton::clicked, this, [this]() { installAndRestart(); });
    QObject::connect(this->buttonReveal, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(stagingFolder()));
    });

    outer->addWidget(buttons);

    // Fixed, and shown rather than offered: this is where builds come from, not a
    // preference. A build is a program that runs on this machine, so the list of
    // places it may come from belongs in the binary rather than in a text field
    // anybody with the Settings dialog open could edit.
    this->labelSource->setText(gitHubOwnerRepo());
    this->labelSource->setToolTip("Fixed in this build. A build is a program that runs on this"
                                  + QString(" machine, so where one may come from is not a setting."));

    if (sourceWasOverridden())
    {
        // Say so rather than fall back in silence. Somebody put a value there and
        // is entitled to know it is being ignored.
        say("A different update source is configured, and is being ignored: builds "
            "are only accepted from the repository above.", true);
    }
    else
    {
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

    // Only ever after a download that matched its checksum. An install button that
    // could act on an unverified file would undo the point of verifying.
    this->buttonInstall->setEnabled(!busy && !this->downloadedPath.isEmpty());
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

// ---- put it in place -------------------------------------------------------

QString UpdateDialog::writeUpdaterScript(QString* problem) const
{
    const QString installDir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    const QString zip = QDir::toNativeSeparators(this->downloadedPath);
    const QString folder = QDir::toNativeSeparators(stagingFolder());
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

    const QString scriptPath = QDir(stagingFolder()).absoluteFilePath("apply-update.cmd");

    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (problem != nullptr)
            *problem = QString("Could not write the updater to %1").arg(folder);

        return QString();
    }

    // Written rather than shipped so the paths are literal and there is no quoting
    // to get wrong at run time, and so anybody who wants to read what is about to
    // happen to their installation can open it.
    QString text;
    text += "@echo off\r\n";
    text += "setlocal\r\n";
    text += "title CasparCG Client update\r\n";
    text += "echo Updating the CasparCG Client.\r\n";
    text += "echo.\r\n";
    text += "\r\n";
    text += "set \"INSTALL=" + installDir + "\"\r\n";
    text += "set \"ZIP=" + zip + "\"\r\n";
    text += "set \"WORK=" + folder + "\\staged\"\r\n";
    text += "set \"BACKUP=" + folder + "\\previous\"\r\n";
    text += "\r\n";

    // 1. Wait for the client to actually be gone. Overwriting while it is up is
    //    the failure that leaves a mixed installation: the libraries are replaced,
    //    the locked executable is skipped, and the thing starts and misbehaves.
    text += "echo Waiting for the client to close...\r\n";
    text += "set /a TRIES=0\r\n";
    text += ":waitloop\r\n";
    text += "set /a TRIES+=1\r\n";
    text += "if %TRIES% GTR 60 goto :stillrunning\r\n";
    text += "tasklist /fi \"IMAGENAME eq casparcg-client.exe\" 2>nul | find /i \"casparcg-client.exe\" >nul\r\n";
    text += "if errorlevel 1 goto :closed\r\n";
    text += "ping -n 2 127.0.0.1 >nul\r\n";
    text += "goto :waitloop\r\n";
    text += "\r\n";
    text += ":stillrunning\r\n";
    text += "echo The client is still running after a minute. Nothing has been changed.\r\n";
    text += "echo Close it and run this file again:\r\n";
    text += "echo   " + QDir::toNativeSeparators(scriptPath) + "\r\n";
    text += "pause\r\n";
    text += "exit /b 1\r\n";
    text += "\r\n";
    text += ":closed\r\n";

    // 2. Unpack. Expand-Archive is on every Windows 10 and later; tar is not on
    //    all of them, and neither is 7-Zip - this machine has no 7-Zip at all,
    //    which is exactly the sort of assumption worth not making.
    text += "echo Unpacking...\r\n";
    text += "if exist \"%WORK%\" rmdir /s /q \"%WORK%\"\r\n";
    text += "powershell -NoProfile -ExecutionPolicy Bypass -Command "
            "\"Expand-Archive -LiteralPath '%ZIP%' -DestinationPath '%WORK%' -Force\"\r\n";
    text += "if errorlevel 1 goto :unpackfailed\r\n";
    text += "\r\n";

    // 3. The package holds one folder named for the build. Copying that folder
    //    instead of what is inside it is the mistake that leaves the old
    //    executable in place, still running, still reporting the old number.
    text += "set \"SOURCE=%WORK%\"\r\n";
    text += "for /d %%D in (\"%WORK%\\*\") do set \"SOURCE=%%~fD\"\r\n";
    text += "if not exist \"%SOURCE%\\casparcg-client.exe\" goto :noexe\r\n";
    text += "\r\n";

    // 4. Keep what is there. A copy that fails halfway leaves an installation that
    //    is half of each, and without this there is nothing to go back to.
    text += "echo Backing up the current build...\r\n";
    text += "if exist \"%BACKUP%\" rmdir /s /q \"%BACKUP%\"\r\n";

    // Excluding the updates folder, which lives INSIDE the installation. Without
    // that, backing the installation up into a folder within it copies the folder
    // into itself: the package, the unpacked staging and all. Found by running
    // this against a layout that matched a real installation rather than one where
    // the two folders happened to be siblings.
    text += "robocopy \"%INSTALL%\" \"%BACKUP%\" /E /XD \"" + folder + "\""
            " /NFL /NDL /NP /NJH /NJS /R:1 /W:1 >nul\r\n";
    text += "if errorlevel 8 goto :backupfailed\r\n";
    text += "\r\n";

    text += "echo Installing...\r\n";
    text += "robocopy \"%SOURCE%\" \"%INSTALL%\" /E /NFL /NDL /NP /NJH /NJS /R:3 /W:2 >nul\r\n";
    text += "if errorlevel 8 goto :copyfailed\r\n";
    text += "\r\n";

    text += "echo Done. Starting the new build.\r\n";
    text += "start \"\" \"" + exe + "\"\r\n";
    text += "exit /b 0\r\n";
    text += "\r\n";

    // Every failure says what was and was not changed, because "it did not work"
    // on a playout machine is the start of a bad hour.
    text += ":unpackfailed\r\n";
    text += "echo Could not unpack the download. Nothing has been changed.\r\n";
    text += "pause\r\n";
    text += "exit /b 1\r\n";
    text += "\r\n";
    text += ":noexe\r\n";
    text += "echo The package does not contain casparcg-client.exe. Nothing has been changed.\r\n";
    text += "pause\r\n";
    text += "exit /b 1\r\n";
    text += "\r\n";
    text += ":backupfailed\r\n";
    text += "echo Could not back up the current build, so nothing was replaced.\r\n";
    text += "pause\r\n";
    text += "exit /b 1\r\n";
    text += "\r\n";
    text += ":copyfailed\r\n";
    text += "echo The copy failed. Putting the previous build back...\r\n";
    text += "robocopy \"%BACKUP%\" \"%INSTALL%\" /E /NFL /NDL /NP /NJH /NJS /R:3 /W:2 >nul\r\n";
    text += "if errorlevel 8 (\r\n";
    text += "  echo The restore ALSO failed. The previous build is in:\r\n";
    text += "  echo   %BACKUP%\r\n";
    text += ") else (\r\n";
    text += "  echo The previous build is back. Nothing has changed.\r\n";
    text += ")\r\n";
    text += "pause\r\n";
    text += "exit /b 1\r\n";

    script.write(text.toUtf8());
    script.close();

    return scriptPath;
}

void UpdateDialog::installAndRestart()
{
    if (this->downloadedPath.isEmpty())
        return;

    const QString target = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());

    // Asked plainly, because this closes the program. On a playout machine that is
    // not a click to make without reading it.
    QMessageBox confirm(this);
    confirm.setWindowTitle("Install and restart");
    confirm.setIcon(QMessageBox::Question);
    confirm.setText("Close the client and install this build?");
    confirm.setInformativeText(
        QString("The client will close, the new build will be copied over\n%1\n"
                "and the client will start again.\n\n"
                "The build you are running now is kept, so it can be put back if the\n"
                "copy fails.\n\n"
                "Do not do this during a show.").arg(target));
    confirm.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
    confirm.setDefaultButton(QMessageBox::Cancel);
    confirm.button(QMessageBox::Ok)->setText("Close and install");

    if (confirm.exec() != QMessageBox::Ok)
        return;

    QString problem;
    const QString script = writeUpdaterScript(&problem);

    if (script.isEmpty())
    {
        say(problem, true);
        return;
    }

    // Detached on purpose: it has to outlive this process, because this process is
    // what it is waiting for.
    if (!QProcess::startDetached("cmd.exe",
                                 QStringList() << "/c" << QDir::toNativeSeparators(script)))
    {
        say("Could not start the updater. The package is still in " + stagingFolder()
            + " and can be unpacked over the installation by hand.", true);
        return;
    }

    // And now get out of its way.
    QCoreApplication::quit();
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
        this->buttonInstall->setEnabled(true);

        // And it stops here, on purpose. Windows will not overwrite a running
        // executable, and on a machine that may be on air the moment to replace the
        // client belongs to whoever is standing in front of it.
        say(QString("Downloaded and verified.\n\nInstall and Restart will close the client, "
                    "put it in place and start it again. Or unpack it yourself from %1 - "
                    "the contents of the folder inside the zip, not the folder.")
            .arg(stagingFolder()));
    });
}
