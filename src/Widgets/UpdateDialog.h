#pragma once

#include "Shared.h"

#include "ClientRelease.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QList>
#include <QtCore/QString>

#include <QtWidgets/QDialog>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QPushButton;
class QTextEdit;
QT_END_NAMESPACE

// Is there a newer build of this client, fetch it, or go back to an older one.
//
// Nothing here happens on its own. There is no poll timer, no startup check and
// no automatic install: the operator opens this, presses Check, chooses a build
// and presses Download if they want to. That is deliberate on a playout machine -
// the client is not something to have quietly replace itself between shows - and
// it is why this is a dialog rather than a service.
//
// Every published build is offered, not only the newest: going back to an earlier
// one is the same download and the same install, verified the same way. And the
// build an install replaced is kept beside the installation, so the last change
// can be undone without a network at all.
//
// A verified download is remembered on disk. Closing this window after a
// download and opening it again offers the install straight away; the package is
// hashed again before it is installed.
//
// The arithmetic - what a tag means, whether it is newer or older, which asset
// belongs here, what the checksums file says, what a download record holds - is
// in Common/ClientRelease.h, tested without a network. The scripts that swap the
// installation are run for real by tools/test-updater.py.
class WIDGETS_EXPORT UpdateDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit UpdateDialog(QWidget* parent = nullptr);

        // Where releases are published, and the token to read them with. Separate
        // from the template source: the repository holding builds is not usually
        // the one holding templates, and a client may well follow one and not the
        // other.
        static QString source();

        // Whether a stored source was ignored for not being on the allowlist.
        static bool sourceWasOverridden();
        static QString token();

        // "github:owner/repo", the only shape understood so far.
        static bool isGitHub();
        static QString gitHubOwnerRepo();

        // What this build is, in the words a release tag uses.
        static QString runningVersionText();

    private:
        // One published build that can be installed here.
        struct Offer
        {
            QString tag;
            ClientRelease::Version version;
            QString assetName;
            QString assetUrl;
            QString sumsUrl;
            QString notes;
        };

        QLabel* labelRunning = nullptr;
        QLabel* labelSource = nullptr;
        QLabel* labelResult = nullptr;
        QComboBox* comboVersion = nullptr;
        QTextEdit* textNotes = nullptr;
        QPushButton* buttonCheck = nullptr;
        QPushButton* buttonDownload = nullptr;
        QPushButton* buttonInstall = nullptr;
        QPushButton* buttonPrevious = nullptr;
        QPushButton* buttonReveal = nullptr;
        QProgressBar* progress = nullptr;

        QNetworkAccessManager* network = nullptr;

        // What the last check found, newest first.
        QList<Offer> offers;

        // The build chosen in the list, kept so Download knows what to fetch.
        QString foundTag;
        QString assetName;
        QString assetUrl;
        QString sumsUrl;
        QString expectedSha;
        bool downloadAllowed = false;

        // Where the finished, verified package is, and which build it is.
        QString downloadedPath;
        QString downloadedTag;
        QString downloadedSha;

        // A download in progress goes to a .part file and is hashed as it arrives,
        // rather than being held whole in memory until it finishes.
        QFile partFile;
        QCryptographicHash partHash{QCryptographicHash::Sha256};

        void check();
        void choose(int index);
        void download();

        // A package downloaded and verified before this window was opened.
        void restoreDownloaded();

        // Hash the downloaded package again. Returns false with the reason.
        bool verifyDownloaded(QString* problem) const;

        // Write the script that does the swap, start it, and quit so it can.
        //
        // The client cannot install itself while it is running - Windows will not
        // replace a running executable - so the last step has to outlive the
        // process. A small script waits for this one to exit, unpacks, copies over
        // the installation and starts the new build.
        //
        // Two things it does that doing it by hand gets wrong. The package has a
        // folder inside it named for the build, and copying that folder rather than
        // its contents leaves the old executable exactly where it was, running,
        // reporting the old number. And it waits for the process to be gone rather
        // than assuming: an overwrite that begins while the client is still up
        // replaces the libraries, skips the locked executable, and leaves a mixed
        // installation that starts and misbehaves.
        void installAndRestart();

        // Everything the script needs, written next to the download. Returns the
        // path to it, or empty with the reason in `problem`.
        QString writeUpdaterScript(QString* problem) const;

        // Put back the build the last install replaced, from the copy kept beside
        // the installation. No network. The two builds trade places, so doing it
        // again goes forward again.
        void restorePrevious();
        QString writeRestoreScript(QString* problem) const;

        // What the kept build is, from the note the install left with it. Empty
        // when there is no kept build.
        static QString previousBuildText();
        static bool hasPreviousBuild();
        void refreshPrevious();

        void requestSums();
        void requestAsset();

        void say(const QString& text, bool problem = false);
        void setBusy(bool busy);

        // Beside the application rather than in a temp folder, because a 106 MB
        // download that vanished on reboot before anybody installed it would be a
        // second 106 MB download.
        static QString stagingFolder();

        // The settings database, which an install copies beside the kept build.
        static QString databaseFile();
};
