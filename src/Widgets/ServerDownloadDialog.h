#pragma once

#include "Shared.h"

#include "ServerReleases.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/QString>

#include <QtWidgets/QDialog>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class QProgressBar;
class QPushButton;
class QTextEdit;
class QTreeWidget;
QT_END_NAMESPACE

// Download a CasparCG Server build - a stable release, a pre-release or a
// nightly of any branch - and install it beside the one this server runs now.
//
// Opened from a server's menu in Server Status. Nothing is replaced: every build
// goes into a new folder of its own ("CasparCG Server 2.5.1"), so the running
// server is untouched until somebody chooses to switch, and going back is a
// matter of pointing the server at the old folder again.
//
// What it does, in order: download to a .part file, check it (the SHA-256 GitHub
// publishes for a release; the size and the zip signature for a nightly, which
// has no checksum), unpack it with Windows' own tar.exe, then - each only when
// ticked - carry this server's casparcg.config over (a dated backup written
// beside the old one first) and point the server at the new folder. Switching the
// running server over is asked for separately, at the end.
//
// Listing, naming and the config carry-over are in Common/ServerReleases.h,
// tested by tools/test-serverreleases.
class WIDGETS_EXPORT ServerDownloadDialog : public QDialog
{
    Q_OBJECT

    public:
        // One window at a time: a second request raises the open one.
        static void openFor(const QString& deviceName, QWidget* parent);

        explicit ServerDownloadDialog(const QString& deviceName, QWidget* parent = nullptr);
        ~ServerDownloadDialog() override;

    protected:
        void reject() override;

    private:
        QString deviceName;

        // The server as it is set up now: where its casparcg.exe is, and what the
        // setting held (a folder or the executable), so a new path is written the
        // same way.
        QString currentExecutable;
        QString currentFolder;
        bool configuredAsFolder = false;
        QString runningVersion;

        QLabel* labelCurrent = nullptr;
        QTreeWidget* treeBuilds = nullptr;
        QCheckBox* checkPreReleases = nullptr;
        QTextEdit* textNotes = nullptr;
        QLineEdit* editParent = nullptr;
        QPushButton* buttonChooseParent = nullptr;
        QCheckBox* checkKeepConfig = nullptr;
        QCheckBox* checkPointServer = nullptr;
        QLabel* labelResult = nullptr;
        QProgressBar* progress = nullptr;
        QPushButton* buttonCheck = nullptr;
        QPushButton* buttonInstall = nullptr;
        QPushButton* buttonReveal = nullptr;

        QNetworkAccessManager* network = nullptr;

        QList<ServerReleases::Build> builds;
        QStringList listingProblems;
        int listingsPending = 0;

        // The build being installed, and where it goes.
        ServerReleases::Build chosen;
        QString targetFolder;
        QString zipPath;
        QString installedFolder;

        QFile partFile;
        QCryptographicHash partHash{QCryptographicHash::Sha256};
        QPointer<QNetworkReply> download;
        QPointer<QProcess> unpacker;
        bool busy = false;

        void check();
        void listingDone();
        void fillList();
        void choose();
        int chosenIndex() const;

        void install();
        void downloaded(QNetworkReply* reply);
        void unpack();
        void unpacked(int exitCode);

        // The steps after unpacking. Each returns a line for the result, or adds
        // a problem.
        QString carryConfig(const QString& serverFolder, QStringList& problems);
        QString pointServer(const QString& serverFolder, QStringList& problems);

        // Stop the server and scanner running from the old folder, then start the
        // ones in the new folder.
        void offerSwitch(const QString& oldExecutable, const QString& newExecutable);
        void switchServer(const QString& oldExecutable, const QString& newExecutable);

        // Remove what an install that did not finish left behind: only its own
        // .part, .zip and the folder it created.
        void cleanUp(bool removeFolder);

        void say(const QString& text, bool problem = false);
        void setBusy(bool busy);

        static QString sizeText(qint64 bytes);
        static QString tarExecutable();
};
