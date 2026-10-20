#include "ServerDownloadDialog.h"

#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "ServerProcessControl.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFileInfo>
#include <QtCore/QMap>
#include <QtCore/QProcess>
#include <QtCore/QSignalBlocker>
#include <QtCore/QStorageInfo>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <QtGui/QDesktopServices>
#include <QtGui/QFont>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include <memory>

namespace
{
    // The list's data role: an index into `builds`, or -1 for a heading.
    const int BUILD_ROLE = Qt::UserRole + 1;

    QNetworkRequest listingRequest(const QString& url, bool json)
    {
        QNetworkRequest request((QUrl(url)));
        request.setRawHeader("User-Agent", "CasparCG-Client");
        if (json)
        {
            request.setRawHeader("Accept", "application/vnd.github+json");
            request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
        }
        request.setTransferTimeout(30000);
        return request;
    }
}

void ServerDownloadDialog::openFor(const QString& deviceName, QWidget* parent)
{
    static QPointer<ServerDownloadDialog> open;

    if (open != nullptr)
    {
        open->show();
        open->raise();
        open->activateWindow();
        return;
    }

    open = new ServerDownloadDialog(deviceName, parent);
    open->setAttribute(Qt::WA_DeleteOnClose);
    open->show();
}

ServerDownloadDialog::ServerDownloadDialog(const QString& deviceName, QWidget* parent)
    : QDialog(parent), deviceName(deviceName)
{
    setWindowTitle(QString("Download CasparCG Server - %1").arg(deviceName));
    resize(760, 640);

    const DeviceModel model = DatabaseManager::getInstance().getDeviceByName(deviceName);
    const QString configured = model.getServerPath().trimmed();
    this->configuredAsFolder = !configured.isEmpty() && QFileInfo(configured).isDir();
    this->currentExecutable = ServerProcessControl::serverExecutableFor(configured);
    if (!this->currentExecutable.isEmpty())
        this->currentFolder = QFileInfo(this->currentExecutable).absolutePath();
    this->runningVersion = model.getVersion().trimmed();

    this->network = new QNetworkAccessManager(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    this->labelCurrent = new QLabel(this);
    this->labelCurrent->setWordWrap(true);
    this->labelCurrent->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QString current = this->currentExecutable.isEmpty()
        ? QString("No server folder is set for '%1' (Settings \xe2\x86\x92 Servers). A build can still be downloaded and installed, "
                  "and this server pointed at it.").arg(deviceName)
        : QString("'%1' runs from %2").arg(deviceName, QDir::toNativeSeparators(this->currentFolder));
    if (!this->runningVersion.isEmpty())
        current += QString(" and reports version %1").arg(this->runningVersion);
    this->labelCurrent->setText(current + ".");
    layout->addWidget(this->labelCurrent);

    this->treeBuilds = new QTreeWidget(this);
    this->treeBuilds->setColumnCount(3);
    this->treeBuilds->setHeaderLabels(QStringList() << "Build" << "Published" << "Size");
    this->treeBuilds->setRootIsDecorated(true);
    this->treeBuilds->setUniformRowHeights(true);
    this->treeBuilds->header()->setStretchLastSection(false);
    this->treeBuilds->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    this->treeBuilds->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    this->treeBuilds->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    QObject::connect(this->treeBuilds, &QTreeWidget::currentItemChanged, this, [this]() { choose(); });
    layout->addWidget(this->treeBuilds, 3);

    this->checkPreReleases = new QCheckBox("Show pre-releases (release candidates and betas)", this);
    QObject::connect(this->checkPreReleases, &QCheckBox::toggled, this, [this]() { fillList(); });
    layout->addWidget(this->checkPreReleases);

    this->textNotes = new QTextEdit(this);
    this->textNotes->setReadOnly(true);
    layout->addWidget(this->textNotes, 2);

    QGridLayout* options = new QGridLayout();
    options->setHorizontalSpacing(8);
    options->setVerticalSpacing(6);

    options->addWidget(new QLabel("Install into", this), 0, 0);
    this->editParent = new QLineEdit(this);
    QString parentFolder;
    if (!this->currentFolder.isEmpty())
        parentFolder = QFileInfo(this->currentFolder).absolutePath();
    else if (QFileInfo("C:/CasparCG").isDir())
        parentFolder = "C:/CasparCG";
    else
        parentFolder = QDir::homePath();
    this->editParent->setText(QDir::toNativeSeparators(parentFolder));
    this->editParent->setToolTip("Each build gets a new folder of its own inside this one. Nothing in it is replaced.");
    options->addWidget(this->editParent, 0, 1);
    this->buttonChooseParent = new QPushButton("Choose...", this);
    QObject::connect(this->buttonChooseParent, &QPushButton::clicked, this, [this]() {
        const QString folder = QFileDialog::getExistingDirectory(this, "Install Into", this->editParent->text());
        if (!folder.isEmpty())
            this->editParent->setText(QDir::toNativeSeparators(folder));
    });
    options->addWidget(this->buttonChooseParent, 0, 2);

    const bool hasConfig = !this->currentFolder.isEmpty() && QFileInfo(QDir(this->currentFolder).filePath("casparcg.config")).isFile();
    this->checkKeepConfig = new QCheckBox("Keep this server's casparcg.config (a dated backup is written beside it first)", this);
    this->checkKeepConfig->setChecked(hasConfig);
    this->checkKeepConfig->setEnabled(hasConfig);
    this->checkKeepConfig->setToolTip(hasConfig
        ? "The new build's own config is kept beside it as casparcg.config.shipped, to look up new options."
        : "There is no casparcg.config in the current server folder to keep.");
    options->addWidget(this->checkKeepConfig, 1, 1, 1, 2);

    this->checkPointServer = new QCheckBox(QString("Point '%1' at the new folder when it is installed").arg(deviceName), this);
    this->checkPointServer->setChecked(true);
    this->checkPointServer->setToolTip("Changes the server folder in Settings \xe2\x86\x92 Servers. The running server is not stopped: that is asked separately.");
    options->addWidget(this->checkPointServer, 2, 1, 1, 2);
    options->setColumnStretch(1, 1);
    layout->addLayout(options);

    this->progress = new QProgressBar(this);
    this->progress->setVisible(false);
    layout->addWidget(this->progress);

    this->labelResult = new QLabel(this);
    this->labelResult->setWordWrap(true);
    this->labelResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(this->labelResult);

    QHBoxLayout* buttons = new QHBoxLayout();
    this->buttonCheck = new QPushButton("Refresh List", this);
    QObject::connect(this->buttonCheck, &QPushButton::clicked, this, [this]() { check(); });
    buttons->addWidget(this->buttonCheck);
    this->buttonReveal = new QPushButton("Open Folder", this);
    this->buttonReveal->setEnabled(false);
    QObject::connect(this->buttonReveal, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(this->installedFolder));
    });
    buttons->addWidget(this->buttonReveal);
    buttons->addStretch(1);
    this->buttonInstall = new QPushButton("Download and Install", this);
    this->buttonInstall->setEnabled(false);
    this->buttonInstall->setDefault(true);
    QObject::connect(this->buttonInstall, &QPushButton::clicked, this, [this]() { install(); });
    buttons->addWidget(this->buttonInstall);
    QPushButton* buttonClose = new QPushButton("Close", this);
    QObject::connect(buttonClose, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(buttonClose);
    layout->addLayout(buttons);

    check();
}

ServerDownloadDialog::~ServerDownloadDialog()
{
    // The client closing mid-download: the reply and the unpacker are children
    // and would report back into a window already half taken down.
    if (this->download != nullptr)
    {
        this->download->disconnect(this);
        this->download->abort();
        this->partFile.close();
        QFile::remove(this->zipPath + ".part");
    }

    if (this->unpacker != nullptr)
    {
        this->unpacker->disconnect(this);
        this->unpacker->kill();
        this->unpacker->waitForFinished(3000);
    }
}

void ServerDownloadDialog::reject()
{
    if (this->unpacker != nullptr)
    {
        say("Unpacking - this window closes once it is done.");
        return;
    }

    if (this->download != nullptr)
    {
        if (QMessageBox::question(this, "Stop Download", "Stop the download? What has arrived so far is thrown away.",
                                  QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;

        // The finished handler cleans up after an abort.
        if (this->download != nullptr)
            this->download->abort();
    }

    QDialog::reject();
}

QString ServerDownloadDialog::sizeText(qint64 bytes)
{
    if (bytes <= 0)
        return QString();
    if (bytes >= 1024LL * 1024 * 1024)
        return QString("%1 GB").arg(double(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 MB").arg(qRound64(double(bytes) / (1024.0 * 1024.0)));
}

QString ServerDownloadDialog::tarExecutable()
{
    // Part of Windows since Windows 10 1803. Named in full so a tar.exe that
    // happens to be earlier on the PATH (Git's, which cannot read zips) is not used.
    return QDir(qEnvironmentVariable("SystemRoot", "C:/Windows")).filePath("System32/tar.exe");
}

void ServerDownloadDialog::say(const QString& text, bool problem)
{
    this->labelResult->setText(text);
    this->labelResult->setStyleSheet(problem ? "color: rgb(230, 140, 140);" : QString());
}

void ServerDownloadDialog::setBusy(bool busy)
{
    this->busy = busy;
    this->buttonCheck->setEnabled(!busy && this->listingsPending == 0);
    this->buttonInstall->setEnabled(!busy && chosenIndex() >= 0);
    this->treeBuilds->setEnabled(!busy);
    this->editParent->setEnabled(!busy);
    this->buttonChooseParent->setEnabled(!busy);
    this->checkKeepConfig->setEnabled(!busy && !this->currentFolder.isEmpty()
                                      && QFileInfo(QDir(this->currentFolder).filePath("casparcg.config")).isFile());
    this->checkPointServer->setEnabled(!busy);
    this->progress->setVisible(busy);
    if (!busy)
        this->progress->setValue(0);
}

void ServerDownloadDialog::check()
{
    using namespace ServerReleases;

    this->builds.clear();
    this->listingProblems.clear();
    this->treeBuilds->clear();
    this->textNotes->clear();
    this->listingsPending = 2;
    this->buttonCheck->setEnabled(false);
    this->buttonInstall->setEnabled(false);
    say("Listing builds...");

    // Releases, from GitHub. Anonymous: a token kept for another purpose is not
    // sent to a repository it was not given for.
    QNetworkReply* releases = this->network->get(listingRequest(GITHUB_API, true));
    QObject::connect(releases, &QNetworkReply::finished, this, [this, releases]() {
        releases->deleteLater();
        const int status = releases->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (releases->error() != QNetworkReply::NoError)
            this->listingProblems.append(status == 403
                ? QString("GitHub refused to list releases (its hourly limit for this address may be used up).")
                : QString("Releases could not be listed: %1").arg(releases->errorString()));
        else
            this->builds.append(parseGitHubReleases(releases->readAll()));
        listingDone();
    });

    // Nightlies: the branch folders, then each branch's listing.
    QNetworkReply* root = this->network->get(listingRequest(NIGHTLY_ROOT, false));
    QObject::connect(root, &QNetworkReply::finished, this, [this, root]() {
        root->deleteLater();
        if (root->error() != QNetworkReply::NoError)
        {
            this->listingProblems.append(QString("Nightly builds could not be listed: %1").arg(root->errorString()));
            listingDone();
            return;
        }

        const QStringList branches = parseNightlyBranches(QString::fromUtf8(root->readAll()));
        this->listingsPending += branches.count();
        for (const QString& branch : branches)
        {
            // Newest first, so the newest builds are at the top of the listing.
            const QString branchUrl = QString(NIGHTLY_ROOT) + QString::fromLatin1(QUrl::toPercentEncoding(branch)) + "/";
            QNetworkReply* listing = this->network->get(listingRequest(branchUrl + "?C=M;O=D", false));
            QObject::connect(listing, &QNetworkReply::finished, this, [this, listing, branch, branchUrl]() {
                listing->deleteLater();
                if (listing->error() != QNetworkReply::NoError)
                    this->listingProblems.append(QString("Nightly builds of %1 could not be listed: %2").arg(branch, listing->errorString()));
                else
                    this->builds.append(parseNightlyIndex(QString::fromUtf8(listing->readAll()), branch, branchUrl));
                listingDone();
            });
        }
        listingDone();
    });
}

void ServerDownloadDialog::listingDone()
{
    this->listingsPending--;
    fillList();

    if (this->listingsPending > 0)
        return;

    this->buttonCheck->setEnabled(!this->busy);
    if (this->listingProblems.isEmpty())
        say(this->builds.isEmpty() ? QString("No Windows builds were found.") : QString("Choose a build."));
    else
        say(this->listingProblems.join("\n"), true);
}

int ServerDownloadDialog::chosenIndex() const
{
    const QTreeWidgetItem* item = this->treeBuilds->currentItem();
    if (item == nullptr)
        return -1;
    const int index = item->data(0, BUILD_ROLE).toInt();
    return index >= 0 && index < this->builds.count() ? index : -1;
}

void ServerDownloadDialog::fillList()
{
    using namespace ServerReleases;

    // Kept across a refill (each listing that arrives refills it), by address.
    QString selectedUrl;
    if (chosenIndex() >= 0)
        selectedUrl = this->builds.at(chosenIndex()).url;

    QSignalBlocker blocker(this->treeBuilds);
    this->treeBuilds->clear();

    auto heading = [this](const QString& text) {
        QTreeWidgetItem* item = new QTreeWidgetItem(this->treeBuilds, QStringList() << text);
        item->setData(0, BUILD_ROLE, -1);
        item->setFlags(Qt::ItemIsEnabled);
        QFont font = item->font(0);
        font.setBold(true);
        item->setFont(0, font);
        item->setExpanded(true);
        return item;
    };

    QTreeWidgetItem* selected = nullptr;
    auto add = [this, &selected, &selectedUrl](QTreeWidgetItem* parent, int index) {
        const Build& build = this->builds.at(index);
        QString title = build.kind == Kind::Nightly ? build.title : QString("%1  (%2)").arg(build.title, kindText(build.kind));
        if (build.kind != Kind::Nightly && !this->runningVersion.isEmpty()
            && (this->runningVersion == build.version || this->runningVersion.startsWith(build.version + " ")))
            title += "  - running now";

        QTreeWidgetItem* item = new QTreeWidgetItem(parent, QStringList()
            << title << build.date.toLocalTime().toString("yyyy-MM-dd HH:mm") << sizeText(build.size));
        item->setData(0, BUILD_ROLE, index);
        item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        if (build.url == selectedUrl)
            selected = item;
    };

    QList<int> releases;
    QMap<QString, QList<int>> nightlies;
    for (int i = 0; i < this->builds.count(); i++)
    {
        const Build& build = this->builds.at(i);
        if (build.kind == Kind::Nightly)
            nightlies[build.branch].append(i);
        else if (build.kind == Kind::Stable || this->checkPreReleases->isChecked())
            releases.append(i);
    }

    std::sort(releases.begin(), releases.end(), [this](int a, int b) { return this->builds.at(a).date > this->builds.at(b).date; });

    if (!releases.isEmpty())
    {
        QTreeWidgetItem* group = heading("Releases");
        for (int index : releases)
            add(group, index);
    }

    for (const QString& branch : orderBranches(nightlies.keys()))
    {
        QTreeWidgetItem* group = heading(QString("Nightly builds: %1").arg(branch));
        for (int index : nightlies.value(branch))
            add(group, index);
    }

    this->treeBuilds->expandAll();
    if (selected != nullptr)
        this->treeBuilds->setCurrentItem(selected);

    blocker.unblock();
    choose();
}

void ServerDownloadDialog::choose()
{
    using namespace ServerReleases;

    const int index = chosenIndex();
    this->buttonInstall->setEnabled(!this->busy && index >= 0);
    if (index < 0)
    {
        this->textNotes->clear();
        return;
    }

    const Build& build = this->builds.at(index);
    const QString folder = folderName(build);

    QString text;
    if (build.kind == Kind::Nightly)
    {
        text = QString("# %1\n\nBuilt automatically from the **%2** branch on %3, commit [%4](https://github.com/CasparCG/server/commit/%4).\n\n"
                       "A nightly build is not a tested release. Try it on a machine that is not on air first.\n\n"
                       "CasparCG publishes no checksum for nightlies, so the download is checked for its full size and for being a zip.\n\n")
                   .arg(build.title, build.branch, build.date.toString("yyyy-MM-dd HH:mm"), build.commit);
    }
    else
    {
        text = QString("# %1\n\n").arg(build.title);
        if (build.kind == Kind::PreRelease)
            text += "A pre-release: a candidate for the next release, not the release itself.\n\n";
        text += build.sha256.isEmpty()
            ? QString("GitHub publishes no checksum for this download, so it is checked for its full size.\n\n")
            : QString("Checked against the SHA-256 GitHub publishes for it.\n\n");
    }
    text += QString("Installs into a new folder, **%1**.\n\n").arg(folder);
    if (!build.notes.trimmed().isEmpty())
        text += "---\n\n" + build.notes;

    this->textNotes->setMarkdown(text);
}

void ServerDownloadDialog::install()
{
    using namespace ServerReleases;

#if !defined(Q_OS_WIN)
    say("Installing a server build is only done on Windows. Download the zip from the CasparCG website instead.", true);
    return;
#endif

    const int index = chosenIndex();
    if (index < 0 || this->busy)
        return;
    this->chosen = this->builds.at(index);

    const QString parent = QDir::fromNativeSeparators(this->editParent->text().trimmed());
    if (parent.isEmpty() || !QFileInfo(parent).isDir() || QDir::isRelativePath(parent))
    {
        say("Choose an existing folder to install into.", true);
        return;
    }

    if (!QFileInfo(tarExecutable()).isFile())
    {
        say(QString("Windows' own tar.exe is not on this machine (%1), so the zip cannot be unpacked here.")
                .arg(QDir::toNativeSeparators(tarExecutable())), true);
        return;
    }

    this->targetFolder = uniqueFolder(parent, folderName(this->chosen), [](const QString& path) { return QFileInfo::exists(path); });
    if (this->targetFolder.isEmpty())
    {
        say("Could not find a free folder name in " + QDir::toNativeSeparators(parent), true);
        return;
    }

    // The zip, then everything in it, then a little room.
    const qint64 needed = this->chosen.size * 4;
    const QStorageInfo storage(parent);
    if (storage.isValid() && this->chosen.size > 0 && storage.bytesAvailable() < needed)
    {
        say(QString("Not enough free space: about %1 is needed in %2, and %3 is free.")
                .arg(sizeText(needed), QDir::toNativeSeparators(storage.rootPath()), sizeText(storage.bytesAvailable())), true);
        return;
    }

    QStringList steps;
    steps << QString("Download %1%2.").arg(this->chosen.title,
                                           this->chosen.size > 0 ? QString(" (%1)").arg(sizeText(this->chosen.size)) : QString());
    steps << QString("Unpack it into %1").arg(QDir::toNativeSeparators(this->targetFolder));
    if (this->checkKeepConfig->isChecked() && this->checkKeepConfig->isEnabled())
        steps << "Copy this server's casparcg.config into it, after a dated backup beside the original.";
    if (this->checkPointServer->isChecked())
        steps << QString("Point '%1' at the new folder.").arg(this->deviceName);

    if (QMessageBox::question(this, "Download and Install",
            steps.join("\n\n") + "\n\nThe running server is not stopped or changed.",
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    this->zipPath = this->targetFolder + ".zip";
    this->partFile.close();
    this->partFile.setFileName(this->zipPath + ".part");
    if (!this->partFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        say("Could not write to " + QDir::toNativeSeparators(parent), true);
        return;
    }
    this->partHash.reset();
    this->installedFolder.clear();
    this->buttonReveal->setEnabled(false);

    setBusy(true);
    this->progress->setRange(0, 100);
    say(QString("Downloading %1...").arg(this->chosen.title));

    QNetworkRequest request((QUrl(this->chosen.url)));
    request.setRawHeader("User-Agent", "CasparCG-Client");
    request.setRawHeader("Accept", "application/octet-stream");
    // An inactivity limit: a download that is still arriving runs as long as it takes.
    request.setTransferTimeout(60000);

    QNetworkReply* reply = this->network->get(request);
    this->download = reply;

    QObject::connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total <= 0)
            total = this->chosen.size;
        if (total > 0)
        {
            this->progress->setValue(int((received * 100) / total));
            say(QString("Downloading %1... %2 of %3").arg(this->chosen.title, sizeText(received), sizeText(total)));
        }
    });

    // A release download is a redirect to GitHub's storage, followed by Qt; only
    // the final body is written.
    QObject::connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 300 && status < 400)
            return;

        const QByteArray chunk = reply->readAll();
        this->partHash.addData(chunk);
        if (this->partFile.write(chunk) != chunk.size())
            reply->abort();
    });

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { downloaded(reply); });
}

void ServerDownloadDialog::cleanUp(bool removeFolder)
{
    this->partFile.close();
    QFile::remove(this->zipPath + ".part");
    QFile::remove(this->zipPath);

    // Only a folder this install made: uniqueFolder chose a name nobody had.
    if (removeFolder && !this->targetFolder.isEmpty() && QFileInfo(this->targetFolder).isDir())
        QDir(this->targetFolder).removeRecursively();
}

void ServerDownloadDialog::downloaded(QNetworkReply* reply)
{
    reply->deleteLater();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const qint64 announced = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();

    if (reply->error() == QNetworkReply::NoError)
    {
        const QByteArray rest = reply->readAll();
        this->partHash.addData(rest);
        this->partFile.write(rest);
    }

    const bool written = this->partFile.error() == QFileDevice::NoError;
    const qint64 size = this->partFile.size();
    this->partFile.close();

    if (reply->error() != QNetworkReply::NoError || !written || status >= 400)
    {
        const bool stopped = reply->error() == QNetworkReply::OperationCanceledError;
        cleanUp(false);
        setBusy(false);
        say(stopped ? QString("The download was stopped. Nothing was kept.")
                    : !written ? QString("The download could not be written to disk. Nothing was kept.")
                               : QString("The download failed: %1").arg(reply->errorString()), !stopped);
        return;
    }

    QString problem;
    if (!this->chosen.sha256.isEmpty())
    {
        if (QString::fromLatin1(this->partHash.result().toHex()).toLower() != this->chosen.sha256)
            problem = "The download does not match the checksum GitHub published for it.";
    }
    else if (announced > 0 && size != announced)
    {
        problem = QString("The download stopped short: %1 of %2 bytes arrived.").arg(size).arg(announced);
    }

    if (problem.isEmpty())
    {
        QFile check(this->zipPath + ".part");
        if (!check.open(QIODevice::ReadOnly) || check.read(2) != "PK")
            problem = "What arrived is not a zip file.";
    }

    if (!problem.isEmpty())
    {
        cleanUp(false);
        setBusy(false);
        say(problem + " It has been thrown away; nothing was installed.", true);
        return;
    }

    QFile::remove(this->zipPath);
    if (!QFile::rename(this->zipPath + ".part", this->zipPath))
    {
        cleanUp(false);
        setBusy(false);
        say("Downloaded and checked, but the file could not be put in place.", true);
        return;
    }

    unpack();
}

void ServerDownloadDialog::unpack()
{
    if (!QDir().mkpath(this->targetFolder))
    {
        cleanUp(false);
        setBusy(false);
        say("Could not create " + QDir::toNativeSeparators(this->targetFolder), true);
        return;
    }

    say(QString("Unpacking into %1...").arg(QDir::toNativeSeparators(this->targetFolder)));
    this->progress->setRange(0, 0);

    QProcess* process = new QProcess(this);
    this->unpacker = process;
    process->setProgram(tarExecutable());
    process->setArguments(QStringList() << "-xf" << QDir::toNativeSeparators(this->zipPath)
                                        << "-C" << QDir::toNativeSeparators(this->targetFolder));
    process->setProcessChannelMode(QProcess::MergedChannels);

    QObject::connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        process->deleteLater();
        unpacked(exitStatus == QProcess::NormalExit ? exitCode : -1);
    });
    QObject::connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        unpacked(-1);
    });

    process->start();
}

void ServerDownloadDialog::unpacked(int exitCode)
{
    QString output;
    if (this->unpacker != nullptr)
        output = QString::fromLocal8Bit(this->unpacker->readAll()).trimmed();
    this->unpacker = nullptr;
    this->progress->setRange(0, 100);

    // The zips unpack flat, but a later one with a folder inside is followed down
    // one level rather than refused.
    QString serverFolder;
    if (exitCode == 0)
    {
        if (QFileInfo(QDir(this->targetFolder).filePath("casparcg.exe")).isFile())
        {
            serverFolder = this->targetFolder;
        }
        else
        {
            const QStringList inside = QDir(this->targetFolder).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            if (inside.count() == 1 && QFileInfo(QDir(this->targetFolder).filePath(inside.first() + "/casparcg.exe")).isFile())
                serverFolder = QDir(this->targetFolder).filePath(inside.first());
        }
    }

    if (serverFolder.isEmpty())
    {
        cleanUp(true);
        setBusy(false);
        say(exitCode == 0 ? QString("The zip holds no casparcg.exe, so nothing was installed.")
                          : QString("The zip could not be unpacked%1. Nothing was installed.")
                                .arg(output.isEmpty() ? QString() : ": " + output.left(300)), true);
        return;
    }

    QFile::remove(this->zipPath);

    QStringList lines;
    QStringList problems;
    lines << QString("Installed %1 in %2.").arg(this->chosen.title, QDir::toNativeSeparators(serverFolder));

    if (this->checkKeepConfig->isChecked() && this->checkKeepConfig->isEnabled())
    {
        const QString line = carryConfig(serverFolder, problems);
        if (!line.isEmpty())
            lines << line;
    }

    const QString oldExecutable = this->currentExecutable;
    const QString newExecutable = QDir(serverFolder).filePath("casparcg.exe");
    bool pointed = false;
    if (this->checkPointServer->isChecked())
    {
        const QString line = pointServer(serverFolder, problems);
        pointed = !line.isEmpty();
        if (pointed)
            lines << line;
    }

    this->installedFolder = serverFolder;
    this->buttonReveal->setEnabled(true);
    setBusy(false);

    say(lines.join("\n") + (problems.isEmpty() ? QString() : "\n" + problems.join("\n")), !problems.isEmpty());

    // Asked once this handler has returned: the question is a dialog of its own.
    if (pointed && !oldExecutable.isEmpty() && ServerProcessControl::supported()
        && !ServerProcessControl::runningFrom(oldExecutable).isEmpty())
    {
        QTimer::singleShot(0, this, [this, oldExecutable, newExecutable]() { offerSwitch(oldExecutable, newExecutable); });
    }
}

QString ServerDownloadDialog::carryConfig(const QString& serverFolder, QStringList& problems)
{
    const QString oldConfig = QDir(this->currentFolder).filePath("casparcg.config");
    QFile source(oldConfig);
    if (!source.open(QIODevice::ReadOnly))
    {
        problems << "The current casparcg.config could not be read, so the new build keeps its own.";
        return QString();
    }
    const QString xml = QString::fromUtf8(source.readAll());
    source.close();

    // The backup first, beside the original.
    const QString backupName = ServerReleases::configBackupName(QDateTime::currentDateTime(), [this](const QString& name) {
        return QFileInfo::exists(QDir(this->currentFolder).filePath(name));
    });
    if (!QFile::copy(oldConfig, QDir(this->currentFolder).filePath(backupName)))
    {
        problems << "The backup of casparcg.config could not be written, so the config was not carried over; the new build keeps its own.";
        return QString();
    }

    const QString shipped = QDir(serverFolder).filePath("casparcg.config");
    if (QFileInfo(shipped).isFile())
        QFile::rename(shipped, QDir(serverFolder).filePath(ServerReleases::shippedConfigName()));

    const ServerReleases::CarriedConfig carried = ServerReleases::carryConfig(xml, this->currentFolder);
    QFile target(shipped);
    if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate) || target.write(carried.xml.toUtf8()) < 0)
    {
        problems << "casparcg.config could not be written into the new folder.";
        return QString();
    }
    target.close();

    QString line = QString("casparcg.config carried over (backup: %1; the new build's own is casparcg.config.shipped).")
                       .arg(backupName);
    if (!carried.changed.isEmpty())
        line += "\nThese relative paths now point at the old folder, where their files are:\n  " + carried.changed.join("\n  ");
    return line;
}

QString ServerDownloadDialog::pointServer(const QString& serverFolder, QStringList& problems)
{
    const DeviceModel model = DatabaseManager::getInstance().getDeviceByName(this->deviceName);
    if (model.getName() != this->deviceName)
    {
        problems << QString("'%1' is no longer in Settings, so no server was pointed at the new folder.").arg(this->deviceName);
        return QString();
    }

    // Written the way it was set: a folder stays a folder, an executable an executable.
    const QString path = QDir::toNativeSeparators(this->configuredAsFolder || model.getServerPath().trimmed().isEmpty()
        ? serverFolder
        : QDir(serverFolder).filePath("casparcg.exe"));

    DatabaseManager::getInstance().updateDevice(DeviceModel(model.getId(), model.getName(), model.getAddress(), model.getPort(),
                                                            model.getUsername(), model.getPassword(), model.getDescription(),
                                                            model.getVersion(), model.getShadow(), model.getChannels(),
                                                            model.getChannelFormats(), model.getPreviewChannel(),
                                                            model.getLockedChannel(), model.getTemplatePath(),
                                                            model.getMediaPath(), path));
    DeviceManager::getInstance().refresh();

    return QString("'%1' now points at %2. Pointing it back at %3 in Settings \xe2\x86\x92 Servers goes back.")
        .arg(this->deviceName, path, this->currentFolder.isEmpty() ? QString("its old folder") : QDir::toNativeSeparators(this->currentFolder));
}

void ServerDownloadDialog::offerSwitch(const QString& oldExecutable, const QString& newExecutable)
{
    if (QMessageBox::question(this, "Switch Server",
            QString("The server is still running from the old folder.\n\nStop it and its scanner, and start the new ones now?\n\n"
                    "Every output of '%1' stops until the new server is up.").arg(this->deviceName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    switchServer(oldExecutable, newExecutable);
}

void ServerDownloadDialog::switchServer(const QString& oldExecutable, const QString& newExecutable)
{
    using namespace ServerProcessControl;

    QList<quint32> stopping;
    QList<quint32> refused;
    for (quint32 pid : runningFrom(oldExecutable) + runningFrom(scannerPathFor(oldExecutable)))
    {
        if (terminate(pid))
            stopping.append(pid);
        else
            refused.append(pid);
    }

    if (!refused.isEmpty())
    {
        say("The old server or scanner did not stop. It may run as administrator or as a Windows service, "
            "which a client running as a normal user cannot stop. Nothing was started.", true);
        return;
    }

    setBusy(true);
    this->progress->setRange(0, 0);
    say("Stopping the old server...");

    QTimer* poll = new QTimer(this);
    poll->setInterval(200);
    auto started = std::make_shared<QElapsedTimer>();
    started->start();

    QObject::connect(poll, &QTimer::timeout, this, [this, poll, started, stopping, newExecutable]() {
        bool alive = false;
        for (quint32 pid : stopping)
            alive = alive || isRunning(pid);
        if (alive && started->elapsed() < 10000)
            return;

        poll->stop();
        poll->deleteLater();
        this->progress->setRange(0, 100);
        setBusy(false);

        if (alive)
        {
            say("The old server did not stop within ten seconds. Nothing was started.", true);
            return;
        }

        QStringList problems;
        const QString scanner = scannerPathFor(newExecutable);
        if (QFileInfo(scanner).isFile() && !startDetached(scanner))
            problems << "scanner.exe did not start: " + QDir::toNativeSeparators(scanner);
        if (!startDetached(newExecutable))
            problems << "casparcg.exe did not start: " + QDir::toNativeSeparators(newExecutable);

        // A server that is not connected is retried by the client every few
        // seconds; this only saves the first wait.
        auto device = DeviceManager::getInstance().getDeviceByName(this->deviceName);
        if (device != nullptr && !device->isConnected())
            device->connectDevice();

        say(problems.isEmpty() ? QString("Switched: '%1' now runs from %2.")
                                     .arg(this->deviceName, QDir::toNativeSeparators(QFileInfo(newExecutable).absolutePath()))
                               : problems.join("\n"),
            !problems.isEmpty());
    });
    poll->start();
}
