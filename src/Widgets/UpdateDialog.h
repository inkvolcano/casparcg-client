#pragma once

#include "Shared.h"

#include <QtCore/QString>

#include <QtWidgets/QDialog>

QT_BEGIN_NAMESPACE
class QLabel;
class QNetworkAccessManager;
class QProgressBar;
class QPushButton;
class QTextEdit;
QT_END_NAMESPACE

// Is there a newer build of this client, and fetch it if there is.
//
// Nothing here happens on its own. There is no poll timer, no startup check and
// no automatic install: the operator opens this, presses Check, and presses
// Download if they want to. That is deliberate on a playout machine - the client
// is not something to have quietly replace itself between shows - and it is why
// this is a dialog rather than a service.
//
// It also stops short of installing. The package is downloaded, verified against
// the release's own checksums file, and left in a folder with the folder opened.
// Swapping a running program is the one step this will not take on its own:
// Windows will not let a running executable be overwritten anyway, and on a
// machine that may be on air the person doing it should be the one who chose the
// moment.
//
// The arithmetic - what a tag means, whether it is newer, which asset belongs
// here, what the checksums file says - is in Common/ClientRelease.h, tested
// without a network.
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
        QLabel* labelRunning = nullptr;
        QLabel* labelSource = nullptr;
        QLabel* labelResult = nullptr;
        QTextEdit* textNotes = nullptr;
        QPushButton* buttonCheck = nullptr;
        QPushButton* buttonDownload = nullptr;
        QPushButton* buttonInstall = nullptr;
        QPushButton* buttonReveal = nullptr;
        QProgressBar* progress = nullptr;

        QNetworkAccessManager* network = nullptr;

        // What the last check found, kept so Download knows what to fetch.
        QString foundTag;
        QString assetName;
        QString assetUrl;
        QString sumsUrl;
        QString expectedSha;

        // Where the finished package landed.
        QString downloadedPath;

        void check();
        void download();

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

        void requestSums();
        void requestAsset();

        void say(const QString& text, bool problem = false);
        void setBusy(bool busy);

        // Beside the application rather than in a temp folder, because a 106 MB
        // download that vanished on reboot before anybody installed it would be a
        // second 106 MB download.
        static QString stagingFolder();
};
