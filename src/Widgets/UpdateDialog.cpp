#include "UpdateDialog.h"

#include "ClientRelease.h"
#include "DatabaseManager.h"
#include "Version.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QUrl>
#include <QtCore/QProcess>
#include <QtCore/QSignalBlocker>

#include <algorithm>

#include <QtGui/QDesktopServices>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <QtWidgets/QComboBox>
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

    // Enough to show every build a venue could plausibly want back.
    const int RELEASES_LISTED = 50;
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

QString UpdateDialog::databaseFile()
{
    // Where Main.cpp opens it. A copy goes beside the kept build at every install,
    // because an older build put back later opens whatever a newer one left.
    return QString("%1/.CasparCG/Client/Database.s3db").arg(QDir::homePath());
}

UpdateDialog::UpdateDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Check for Updates");
    setMinimumWidth(600);

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

    // Every published build, newest first. Empty and disabled until a check has
    // filled it, so there is never a list of builds nobody has asked GitHub about.
    grid->addWidget(new QLabel("Build:", this), 2, 0);
    this->comboVersion = new QComboBox(this);
    this->comboVersion->setEnabled(false);
    this->comboVersion->setPlaceholderText("Press Check Now to list the published builds");
    this->comboVersion->setToolTip("Every published build. Choose an older one to roll back to it:\n"
                                   "it is downloaded, verified and installed the same way.");
    grid->addWidget(this->comboVersion, 2, 1);

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
    this->buttonPrevious = buttons->addButton("Put Back Previous Build", QDialogButtonBox::ActionRole);
    this->buttonReveal = buttons->addButton("Show Download", QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);

    this->buttonDownload->setEnabled(false);
    this->buttonInstall->setEnabled(false);
    this->buttonReveal->setEnabled(false);

    this->buttonInstall->setToolTip(
        "Close the client, copy the downloaded build over this installation, and start it again.\n\n"
        "Only available once a download has been verified against the checksum the\n"
        "release published. The build you are running is kept so it can be put back.");

    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(this->buttonCheck, &QPushButton::clicked, this, [this]() { check(); });
    QObject::connect(this->buttonDownload, &QPushButton::clicked, this, [this]() { download(); });
    QObject::connect(this->buttonInstall, &QPushButton::clicked, this, [this]() { installAndRestart(); });
    QObject::connect(this->buttonPrevious, &QPushButton::clicked, this, [this]() { restorePrevious(); });
    QObject::connect(this->buttonReveal, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(stagingFolder()));
    });
    QObject::connect(this->comboVersion, qOverload<int>(&QComboBox::currentIndexChanged),
                     this, [this](int index) { choose(index); });

    outer->addWidget(buttons);

    // Fixed, and shown rather than offered: this is where builds come from, not a
    // preference. A build is a program that runs on this machine, so the list of
    // places it may come from belongs in the binary rather than in a text field
    // anybody with the Settings dialog open could edit.
    this->labelSource->setText(gitHubOwnerRepo());
    this->labelSource->setToolTip("Fixed in this build. A build is a program that runs on this"
                                  + QString(" machine, so where one may come from is not a setting."));

    refreshPrevious();

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

    // After the greeting, so a download waiting to be installed is what it says.
    restoreDownloaded();
}

void UpdateDialog::say(const QString& text, bool problem)
{
    this->labelResult->setText(text);
    this->labelResult->setStyleSheet(problem ? "color: rgb(230, 140, 140);" : QString());
}

void UpdateDialog::setBusy(bool busy)
{
    this->buttonCheck->setEnabled(!busy && isGitHub() && !source().isEmpty());
    this->buttonDownload->setEnabled(!busy && this->downloadAllowed);
    this->comboVersion->setEnabled(!busy && !this->offers.isEmpty());

    // Only ever after a download that matched its checksum. An install button that
    // could act on an unverified file would undo the point of verifying.
    this->buttonInstall->setEnabled(!busy && !this->downloadedPath.isEmpty());
    this->buttonPrevious->setEnabled(!busy && hasPreviousBuild());
    this->progress->setVisible(busy);

    if (!busy)
        this->progress->setValue(0);
}

// ---- what was already here -------------------------------------------------

void UpdateDialog::restoreDownloaded()
{
    const QString folder = stagingFolder();

    // A download this window never finished - closed mid-way - leaves only a part
    // file, which is never installed and would otherwise sit there at full size.
    foreach (const QString& part, QDir(folder).entryList(QStringList("*.part"), QDir::Files))
        QFile::remove(QDir(folder).absoluteFilePath(part));

    QFile file(QDir(folder).absoluteFilePath(ClientRelease::recordFileName()));
    if (!file.open(QIODevice::ReadOnly))
        return;

    const ClientRelease::DownloadRecord record = ClientRelease::parseRecord(QString::fromUtf8(file.readAll()));
    file.close();

    if (!record.valid)
        return;

    const QString path = QDir(folder).absoluteFilePath(record.asset);
    const QFileInfo info(path);
    if (!info.exists() || info.size() != record.size)
        return;

    const ClientRelease::Version version = ClientRelease::parse(record.tag);
    const ClientRelease::Version running = ClientRelease::parse(runningVersionText());
    const ClientRelease::Direction direction = ClientRelease::directionOf(version, running);

    this->buttonReveal->setEnabled(true);

    if (direction == ClientRelease::Direction::Same)
    {
        say(QString("The build downloaded earlier, %1, is the one running now.")
            .arg(ClientRelease::describe(version)));
        return;
    }

    this->downloadedPath = path;
    this->downloadedTag = record.tag;
    this->downloadedSha = record.sha256;
    this->buttonInstall->setEnabled(true);

    say(QString("%1 was downloaded and verified%2, and has not been installed. "
                "Install and Restart puts it in place%3; it is checked again first.")
        .arg(ClientRelease::describe(version),
             record.when.isEmpty() ? QString() : QString(" on %1").arg(record.when),
             direction == ClientRelease::Direction::Rollback ? QString(" - an older build than this one") : QString()));
}

bool UpdateDialog::verifyDownloaded(QString* problem) const
{
    QFile file(this->downloadedPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (problem != nullptr)
            *problem = "The downloaded package is gone. Download it again.";

        return false;
    }

    // In pieces, so a 106 MB package is never in memory whole.
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
        hash.addData(file.read(4 * 1024 * 1024));

    const QString actual = QString::fromLatin1(hash.result().toHex()).toLower();
    if (actual != this->downloadedSha)
    {
        if (problem != nullptr)
            *problem = "The downloaded package has changed since it was verified, so it will not be "
                       "installed. Download it again.";

        return false;
    }

    return true;
}

// ---- which builds are there ------------------------------------------------

void UpdateDialog::check()
{
    this->offers.clear();
    this->foundTag.clear();
    this->assetName.clear();
    this->assetUrl.clear();
    this->sumsUrl.clear();
    this->expectedSha.clear();
    this->downloadAllowed = false;
    this->textNotes->setVisible(false);

    {
        QSignalBlocker blocker(this->comboVersion);
        this->comboVersion->clear();
    }

    setBusy(true);
    this->progress->setRange(0, 0);   // indeterminate: this is one small request
    say("Asking " + gitHubOwnerRepo() + "...");

    QNetworkRequest request((QUrl(QString("%1/repos/%2/releases?per_page=%3")
        .arg(apiBase(), gitHubOwnerRepo()).arg(RELEASES_LISTED))));

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
        int withoutPackage = 0;

        foreach (const QJsonValue& value, releases)
        {
            const QJsonObject release = value.toObject();

            // A draft is not published and a pre-release is not something to offer
            // a venue, in either direction.
            if (release.value("draft").toBool() || release.value("prerelease").toBool())
                continue;

            Offer offer;
            offer.tag = release.value("tag_name").toString();
            offer.version = ClientRelease::parse(offer.tag);
            if (!offer.version.valid)
                continue;

            QStringList names;
            const QJsonArray assets = release.value("assets").toArray();
            foreach (const QJsonValue& assetValue, assets)
                names.append(assetValue.toObject().value("name").toString());

            const QString wanted = ClientRelease::assetFor(ClientRelease::platformKey(), names);
            if (wanted.isEmpty())
            {
                withoutPackage++;
                continue;
            }

            foreach (const QJsonValue& assetValue, assets)
            {
                const QJsonObject asset = assetValue.toObject();
                const QString name = asset.value("name").toString();

                if (name == wanted)
                {
                    offer.assetName = name;
                    offer.assetUrl = asset.value("url").toString();
                }

                // What makes the download verifiable. Without it the package is
                // refused rather than installed on trust.
                if (name.compare("SHA256SUMS.txt", Qt::CaseInsensitive) == 0)
                    offer.sumsUrl = asset.value("url").toString();
            }

            offer.notes = release.value("body").toString();
            this->offers.append(offer);
        }

        // Newest first, whatever order the API used.
        std::stable_sort(this->offers.begin(), this->offers.end(), [](const Offer& a, const Offer& b) {
            return ClientRelease::isNewer(a.version, b.version);
        });

        if (this->offers.isEmpty())
        {
            say(withoutPackage > 0
                ? QString("Nothing published there has a package for %1.").arg(ClientRelease::platformKey())
                : QString("No release there has a version this understands."), true);
            return;
        }

        {
            QSignalBlocker blocker(this->comboVersion);
            for (int i = 0; i < this->offers.count(); i++)
                this->comboVersion->addItem(ClientRelease::choiceLabel(this->offers.at(i).version, running, i == 0));
        }

        this->comboVersion->setEnabled(true);
        this->comboVersion->setCurrentIndex(0);
        choose(0);
    });
}

void UpdateDialog::choose(int index)
{
    this->foundTag.clear();
    this->assetName.clear();
    this->assetUrl.clear();
    this->sumsUrl.clear();
    this->expectedSha.clear();
    this->downloadAllowed = false;
    this->textNotes->setVisible(false);
    this->buttonDownload->setText("Download");

    if (index < 0 || index >= this->offers.count())
    {
        setBusy(false);
        return;
    }

    const Offer& offer = this->offers.at(index);
    const ClientRelease::Version running = ClientRelease::parse(runningVersionText());
    const ClientRelease::Direction direction = ClientRelease::directionOf(offer.version, running);

    this->foundTag = offer.tag;
    this->assetName = offer.assetName;
    this->assetUrl = offer.assetUrl;
    this->sumsUrl = offer.sumsUrl;

    if (!offer.notes.isEmpty())
    {
        this->textNotes->setPlainText(offer.notes);
        this->textNotes->setVisible(true);
    }

    const bool newest = index == 0;

    switch (direction)
    {
        case ClientRelease::Direction::Same:
            say(newest
                ? QString("Up to date. %1 is the newest published build. Choose an older one in the "
                          "list to roll back.").arg(ClientRelease::describe(running))
                : QString("%1 is the build running now.").arg(ClientRelease::describe(running)));
            break;

        case ClientRelease::Direction::Upgrade:
            this->downloadAllowed = true;
            say(QString("%1 is available. You are on %2.")
                .arg(ClientRelease::describe(offer.version), ClientRelease::describe(running)));
            break;

        case ClientRelease::Direction::Rollback:
            this->downloadAllowed = true;
            this->buttonDownload->setText("Download to Roll Back");
            say(QString("%1 is older than the build running now, %2. Installing it rolls this machine "
                        "back; settings added since stay in the database and the older build ignores "
                        "them.")
                .arg(ClientRelease::describe(offer.version), ClientRelease::describe(running)));
            break;

        default:
            say("That build's version is not understood, so it is not offered.", true);
            break;
    }

    setBusy(false);
}

// ---- put it in place -------------------------------------------------------

QString UpdateDialog::writeUpdaterScript(QString* problem) const
{
    const QString installDir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    const QString zip = QDir::toNativeSeparators(this->downloadedPath);
    const QString folder = QDir::toNativeSeparators(stagingFolder());
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString runningVersion = runningVersionText();
    const QString databaseFile = QDir::toNativeSeparators(UpdateDialog::databaseFile());

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

    // Named, so Put Back Previous Build can say which build it would put back. In
    // parentheses so the version's last digit cannot be read as a stream number.
    text += "(echo " + runningVersion + ")>\"%BACKUP%\\update-backup-of.txt\"\r\n";

    // The settings database as it was before this build ever opened it. Not put
    // back by anything automatically - settings changed since would be lost - but
    // there for a person when an older build needs the database it knew.
    text += "if exist \"" + databaseFile + "\" copy /y \"" + databaseFile + "\" \"%BACKUP%\\Database-at-update.s3db\" >nul\r\n";
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
    text += "robocopy \"%BACKUP%\" \"%INSTALL%\" /E /XF update-backup-of.txt Database-at-update.s3db /NFL /NDL /NP /NJH /NJS /R:3 /W:2 >nul\r\n";
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

QString UpdateDialog::writeRestoreScript(QString* problem) const
{
    const QString installDir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    const QString folder = QDir::toNativeSeparators(stagingFolder());
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString runningVersion = runningVersionText();

    const QString scriptPath = QDir(stagingFolder()).absoluteFilePath("restore-previous.cmd");

    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (problem != nullptr)
            *problem = QString("Could not write the restore script to %1").arg(folder);

        return QString();
    }

    // The same shape as the updater, and for the same reasons: literal paths, a
    // wait for the client to be gone, and every failure saying what changed.
    QString text;
    text += "@echo off\r\n";
    text += "setlocal\r\n";
    text += "title CasparCG Client - put back the previous build\r\n";
    text += "echo Putting back the previous CasparCG Client build.\r\n";
    text += "echo.\r\n";
    text += "\r\n";
    text += "set \"INSTALL=" + installDir + "\"\r\n";
    text += "set \"BACKUP=" + folder + "\\previous\"\r\n";
    text += "set \"SWAP=" + folder + "\\replaced\"\r\n";
    text += "\r\n";

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
    text += "if not exist \"%BACKUP%\\casparcg-client.exe\" goto :nobackup\r\n";
    text += "\r\n";

    // 1. Keep the build being replaced, named, so the two trade places and this
    //    can be undone the same way.
    text += "echo Keeping the build being replaced...\r\n";
    text += "if exist \"%SWAP%\" rmdir /s /q \"%SWAP%\"\r\n";
    text += "robocopy \"%INSTALL%\" \"%SWAP%\" /E /XD \"" + folder + "\""
            " /NFL /NDL /NP /NJH /NJS /R:1 /W:1 >nul\r\n";
    text += "if errorlevel 8 goto :keepfailed\r\n";
    text += "(echo " + runningVersion + ")>\"%SWAP%\\update-backup-of.txt\"\r\n";
    text += "\r\n";

    // 2. Put the kept build back. Its name note and database copy stay with it.
    text += "echo Putting the previous build back...\r\n";
    text += "robocopy \"%BACKUP%\" \"%INSTALL%\" /E /XF update-backup-of.txt Database-at-update.s3db /NFL /NDL /NP /NJH /NJS /R:3 /W:2 >nul\r\n";
    text += "if errorlevel 8 goto :copyfailed\r\n";
    text += "\r\n";

    // 3. Trade places: what was running becomes the kept build.
    text += "rmdir /s /q \"%BACKUP%\"\r\n";
    text += "move \"%SWAP%\" \"%BACKUP%\" >nul\r\n";
    text += "if errorlevel 1 echo The replaced build could not be renamed and is in %SWAP%.\r\n";
    text += "\r\n";

    text += "echo Done. Starting the previous build.\r\n";
    text += "start \"\" \"" + exe + "\"\r\n";
    text += "exit /b 0\r\n";
    text += "\r\n";

    text += ":nobackup\r\n";
    text += "echo There is no previous build kept in %BACKUP%. Nothing has been changed.\r\n";
    text += "pause\r\n";
    text += "exit /b 1\r\n";
    text += "\r\n";
    text += ":keepfailed\r\n";
    text += "echo Could not keep a copy of the build being replaced, so nothing was changed.\r\n";
    text += "pause\r\n";
    text += "exit /b 1\r\n";
    text += "\r\n";
    text += ":copyfailed\r\n";
    text += "echo The copy failed. Putting the build that was running back...\r\n";
    text += "robocopy \"%SWAP%\" \"%INSTALL%\" /E /XF update-backup-of.txt /NFL /NDL /NP /NJH /NJS /R:3 /W:2 >nul\r\n";
    text += "if errorlevel 8 (\r\n";
    text += "  echo That ALSO failed. The build that was running is in:\r\n";
    text += "  echo   %SWAP%\r\n";
    text += ") else (\r\n";
    text += "  echo The build that was running is back. Nothing has changed.\r\n";
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

    // Checked again, because the file has been sitting in a folder since it was
    // verified - possibly since another day.
    QString problem;
    if (!verifyDownloaded(&problem))
    {
        say(problem, true);
        this->downloadedPath.clear();
        setBusy(false);
        return;
    }

    const QString target = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    const ClientRelease::Version version = ClientRelease::parse(this->downloadedTag);
    const bool rollback = ClientRelease::directionOf(version, ClientRelease::parse(runningVersionText()))
        == ClientRelease::Direction::Rollback;

    // Asked plainly, because this closes the program. On a playout machine that is
    // not a click to make without reading it.
    QMessageBox confirm(this);
    confirm.setWindowTitle(rollback ? "Roll back and restart" : "Install and restart");
    confirm.setIcon(QMessageBox::Question);
    confirm.setText(QString(rollback ? "Close the client and roll back to %1?" : "Close the client and install %1?")
                    .arg(ClientRelease::describe(version)));
    confirm.setInformativeText(
        QString("The client will close, %1 will be copied over\n%2\n"
                "and the client will start again.\n\n"
                "The build you are running now is kept, so Put Back Previous Build\n"
                "can return to it, and so can a copy that fails.%3\n\n"
                "Do not do this during a show.")
            .arg(ClientRelease::describe(version), target,
                 rollback ? QString("\n\nThe older build opens the same settings. Settings added by newer\n"
                                    "builds stay in the database and are ignored by it; a copy of the\n"
                                    "database as it is now is kept beside the replaced build.")
                          : QString()));
    confirm.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
    confirm.setDefaultButton(QMessageBox::Cancel);
    confirm.button(QMessageBox::Ok)->setText(rollback ? "Close and roll back" : "Close and install");

    if (confirm.exec() != QMessageBox::Ok)
        return;

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

    // Installed, or about to be: the record no longer describes something waiting.
    QFile::remove(QDir(stagingFolder()).absoluteFilePath(ClientRelease::recordFileName()));

    // And now get out of its way.
    QCoreApplication::quit();
}

// ---- the build the last install replaced ------------------------------------

bool UpdateDialog::hasPreviousBuild()
{
    return QFileInfo::exists(QDir(stagingFolder()).absoluteFilePath("previous/casparcg-client.exe"));
}

QString UpdateDialog::previousBuildText()
{
    if (!hasPreviousBuild())
        return QString();

    QFile marker(QDir(stagingFolder()).absoluteFilePath(QString("previous/") + ClientRelease::backupMarkerName()));
    if (marker.open(QIODevice::ReadOnly))
    {
        const ClientRelease::Version version = ClientRelease::parse(QString::fromUtf8(marker.readAll()).trimmed());
        if (version.valid)
            return ClientRelease::describe(version);
    }

    // Kept by an install older than the note, which did not say what it kept.
    return QString("an earlier build (its version was not recorded)");
}

void UpdateDialog::refreshPrevious()
{
    const QString previous = previousBuildText();

    this->buttonPrevious->setEnabled(!previous.isEmpty());
    this->buttonPrevious->setToolTip(previous.isEmpty()
        ? QString("Nothing to put back yet. Every install keeps the build it replaces, and this\n"
                  "puts that one back without a download.")
        : QString("Put back %1, kept by the last install, without a download.\n"
                  "The build running now is kept in its place, so this can be undone the same way.")
              .arg(previous));
}

void UpdateDialog::restorePrevious()
{
    const QString previous = previousBuildText();
    if (previous.isEmpty())
        return;

    QMessageBox confirm(this);
    confirm.setWindowTitle("Put back the previous build");
    confirm.setIcon(QMessageBox::Question);
    confirm.setText(QString("Close the client and put back %1?").arg(previous));
    confirm.setInformativeText(
        QString("The client will close, the build the last install replaced will be copied\n"
                "back over\n%1\nand the client will start again. Nothing is downloaded.\n\n"
                "The build running now, %2, is kept in its place, so this can be\n"
                "undone the same way. Settings stay as they are.\n\n"
                "Do not do this during a show.")
            .arg(QDir::toNativeSeparators(QCoreApplication::applicationDirPath()), runningVersionText()));
    confirm.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
    confirm.setDefaultButton(QMessageBox::Cancel);
    confirm.button(QMessageBox::Ok)->setText("Close and put back");

    if (confirm.exec() != QMessageBox::Ok)
        return;

    QString problem;
    const QString script = writeRestoreScript(&problem);
    if (script.isEmpty())
    {
        say(problem, true);
        return;
    }

    if (!QProcess::startDetached("cmd.exe", QStringList() << "/c" << QDir::toNativeSeparators(script)))
    {
        say("Could not start the restore script. Nothing has been changed.", true);
        return;
    }

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
    // A file name from the API is used as a file name here, so it is held to the
    // same rule as a record read back from disk.
    if (!ClientRelease::isPlainFileName(this->assetName))
    {
        setBusy(false);
        say("The release names its package in a way that is not a plain file name, so it "
            "is not downloaded.", true);
        return;
    }

    const QString finalPath = QDir(stagingFolder()).absoluteFilePath(this->assetName);

    // Into a part file, hashed as it arrives. Holding the whole package in memory
    // until the end cost its full size again on top of the running client, and
    // the part file never carries the package's own name, so nothing that has not
    // matched its checksum can be mistaken for something to install.
    this->partFile.close();
    this->partFile.setFileName(finalPath + ".part");
    if (!this->partFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        setBusy(false);
        say("Could not write to " + stagingFolder(), true);
        return;
    }

    this->partHash.reset();

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

    // GitHub answers an asset request with a redirect to its storage host, which
    // Qt follows by itself and only the final body reaches here. The status check
    // keeps a redirect's own body out of the package should one ever be delivered.
    QObject::connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 300 && status < 400)
            return;

        const QByteArray chunk = reply->readAll();
        this->partHash.addData(chunk);

        if (this->partFile.write(chunk) != chunk.size())
            reply->abort();
    });

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, finalPath]() {
        reply->deleteLater();

        const QString partPath = this->partFile.fileName();

        if (reply->error() == QNetworkReply::NoError)
        {
            const QByteArray rest = reply->readAll();
            this->partHash.addData(rest);
            this->partFile.write(rest);
        }

        const bool written = this->partFile.error() == QFileDevice::NoError;
        this->partFile.close();
        setBusy(false);

        if (reply->error() != QNetworkReply::NoError || !written)
        {
            QFile::remove(partPath);
            say(written ? "The download failed: " + reply->errorString()
                        : "The download could not be written to " + stagingFolder(), true);
            return;
        }

        const QString actual = QString::fromLatin1(this->partHash.result().toHex()).toLower();
        if (actual != this->expectedSha)
        {
            QFile::remove(partPath);
            say("The download does not match the checksum the release published, so "
                "it has been thrown away. Nothing was kept.", true);
            return;
        }

        QFile::remove(finalPath);
        if (!QFile::rename(partPath, finalPath))
        {
            QFile::remove(partPath);
            say("Verified, but could not be put in place in " + stagingFolder(), true);
            return;
        }

        // Remembered, so closing this window does not lose it.
        ClientRelease::DownloadRecord record;
        record.tag = this->foundTag;
        record.asset = this->assetName;
        record.sha256 = actual;
        record.size = QFileInfo(finalPath).size();
        record.when = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");

        QFile recordFile(QDir(stagingFolder()).absoluteFilePath(ClientRelease::recordFileName()));
        if (recordFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            recordFile.write(ClientRelease::recordText(record).toUtf8());
            recordFile.close();
        }

        this->downloadedPath = finalPath;
        this->downloadedTag = this->foundTag;
        this->downloadedSha = actual;
        this->buttonReveal->setEnabled(true);
        this->buttonInstall->setEnabled(true);

        // And it stops here, on purpose. Windows will not overwrite a running
        // executable, and on a machine that may be on air the moment to replace the
        // client belongs to whoever is standing in front of it.
        say(QString("Downloaded and verified.\n\nInstall and Restart will close the client, "
                    "put it in place and start it again. It stays ready if this window is closed. "
                    "Or unpack it yourself from %1 - the contents of the folder inside the zip, "
                    "not the folder.")
            .arg(stagingFolder()));
    });
}
