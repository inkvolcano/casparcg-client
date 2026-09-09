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

        void requestSums();
        void requestAsset();

        void say(const QString& text, bool problem = false);
        void setBusy(bool busy);

        // Beside the application rather than in a temp folder, because a 106 MB
        // download that vanished on reboot before anybody installed it would be a
        // second 106 MB download.
        static QString stagingFolder();
};
