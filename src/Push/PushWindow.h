#pragma once

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>

#include <QtNetwork/QNetworkRequest>

#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableWidget>

class QNetworkAccessManager;

// Somewhere a pack can be sent. One of three: a CasparCG Client that accepts a push,
// a relay that clients pull from, or a private GitHub repository they pull from.
//
// Which one comes from how the address was typed:
//
//   10.0.0.5:3000                  a client
//   https://host/relay/relay.php   a relay
//   github:owner/repo[@branch]     a repository, packs at its root
//
// That keeps the table as it was, so an operator who has only ever pushed directly
// changes nothing.
//
// All three speak different dialects of the same conversation, so every URL and
// every header comes from here rather than from the call sites.
struct PushTarget
{
    QString name;
    QString host;        // host for a client; the whole URL for a relay; the raw address for GitHub
    int port = 3000;
    QString token;
    bool relay = false;
    bool github = false;

    // Only for GitHub. An empty branch means the repository's own default, and an
    // empty api means github.com rather than a GitHub Enterprise Server.
    QString ownerRepo;
    QString branch;
    QString api;

    // Fill relay/github/ownerRepo/branch/host/port from what the operator typed.
    void readAddress(const QString& address);

    QString base() const;
    QString label() const;

    // "Who are you", writing nothing.
    QString infoUrl() const;

    // What is already there, with a digest per file.
    QString manifestUrl(const QString& pack) const;

    // Where one file goes.
    QString uploadUrl(const QString& pack, const QString& relativePath) const;

    // Where a file is taken away again. Relays and repositories only.
    QString removeUrl(const QString& pack, const QString& relativePath) const;

    // Each kind authenticates differently, and deliberately with a different token:
    // a relay's upload token is not any client's push token, and neither is a
    // GitHub personal access token.
    void authorise(QNetworkRequest& request) const;

    // Whether files can be taken off this target at all. A client cannot: removing
    // a template from a machine that may be on air is not a decision to make from
    // another network, so it has no endpoint for it.
    bool canRemove() const { return this->relay || this->github; }
};

// One file, on one client, and what comparing it found.
struct PushJob
{
    // Extra is the far end holding a file this pack no longer has: a rename or a
    // deletion that never travelled, because neither a push nor a pull deletes.
    enum State { New, Changed, Unchanged, Extra, Sent, Failed, Removed };

    PushTarget target;
    QString pack;
    QString relativePath;
    QString absolutePath;
    qint64 bytes = 0;
    State state = New;
    int attempts = 0;

    // What the far end calls the copy it already has. GitHub needs this to replace
    // or delete a file: without it an update would be read as an attempt to create
    // something that is already there.
    QString remoteId;

    QString stateText() const
    {
        switch (this->state)
        {
            case New:       return "new";
            case Changed:   return "changed";
            case Unchanged: return "unchanged";
            case Extra:     return "only there";
            case Sent:      return "sent";
            case Removed:   return "removed";
            default:        return "failed";
        }
    }
};

// Pushing template packs from a dev machine to wherever they need to be.
//
// Two ways out, and the window does not care which. A client on the same network can
// be written to directly. A client anywhere else pulls from a relay, and this uploads
// to that relay instead, which is the only arrangement that works when the machine
// that runs the graphics is behind a firewall nobody is going to open.
//
// Two steps, always in that order. Compare reads every ticked pack on every ticked
// client and lists each file with what it found; Push sends the ones still ticked.
// Nothing is written until the list has been seen, because the far end of this is a
// machine that may be on air.
//
// Comparing is by digest, so a file whose contents match is listed as unchanged and
// left unticked: an unchanged file is never rewritten, and the list is the evidence
// of that rather than a promise about it.
//
// project.js and extensions.json are never offered. They belong to the machine being
// pushed to, which is the one that edits the API key and the Sheets panel buttons.
class PushWindow : public QMainWindow
{
    Q_OBJECT

    public:
        explicit PushWindow(QWidget* parent = nullptr);

    private:
        void buildUi();
        void loadSettings();
        void saveSettings();

        void refreshPacks();
        QList<PushTarget> checkedTargets() const;
        QStringList checkedPacks() const;

        // Every pack in the templates folder, whether ticked or not.
        QStringList allPacks() const;

        // gitStyle asks for Git blob hashes rather than a plain sha1, which is what a
        // GitHub tree listing gives back and therefore what it must be compared to.
        QMap<QString, QString> localFiles(const QString& pack, bool gitStyle) const;

        static QString gitBlobSha(const QByteArray& content);
        static bool isProtected(const QString& relativePath);
        static QString humanBytes(qint64 bytes);

        void startCompare();
        void nextPair();
        void showResults();

        void startPush();
        void nextFile();
        void sendOne(int index);

        // Whether a failure is worth another go. A dropped connection or a host
        // having a moment will succeed on the next try; a refusal will not, and
        // retrying one only wastes the operator's time and hammers the far end.
        static bool worthRetrying(int httpStatus);

        void log(const QString& line);
        void setBusy(bool busy);

        Q_SLOT void addTarget();
        Q_SLOT void removeTarget();
        Q_SLOT void browseForSource();
        Q_SLOT void tickAll();
        Q_SLOT void tickNone();

        // Ask each ticked client who it is. Over a link that may cross networks,
        // "can I reach it and is my token right" is worth answering on its own,
        // before a push is the thing that finds out.
        Q_SLOT void identifyTargets();

        // Ask a relay which clients have checked in and which are behind. This is
        // the question a push to a relay cannot answer by itself: the relay took the
        // file, but did the venue ever come and get it.
        void describeRelayClients(const PushTarget& target);

        // Clear ticked "only there" rows off a relay. Relays only: a client has no
        // delete endpoint on purpose, because taking a template off a machine that
        // may be on air is not a decision to make from another network.
        Q_SLOT void removeExtras();

        // Which machine takes which packs, for a relay or a repository. A client
        // pushed to directly has no assignments: it gets what is sent to it.
        Q_SLOT void showAssignments();

        QLineEdit* sourceEdit = nullptr;
        QListWidget* packList = nullptr;
        QTableWidget* targetTable = nullptr;
        QTableWidget* fileTable = nullptr;
        QPlainTextEdit* logView = nullptr;
        QPushButton* identifyButton = nullptr;
        QPushButton* removeButton = nullptr;
        QPushButton* assignButton = nullptr;
        QPushButton* compareButton = nullptr;
        QPushButton* pushButton = nullptr;

        QNetworkAccessManager* network = nullptr;

        // Everything Compare found, in the order it is shown. The table's rows and
        // this list are the same thing seen twice, so a row's index is its job.
        QList<PushJob> results;

        QList<QPair<PushTarget, QString>> pairQueue;   // (client, pack) still to compare
        QList<int> sendQueue;                          // indexes into results, still to send
        QList<int> removeQueue;                        // indexes into results, still to clear off a relay

        void nextRemoval();

        bool busy = false;

        // Whether the last Compare found anything that exists only at the far end.
        // Held rather than read back off the button, because setBusy disables the
        // button and would otherwise lose the answer the moment a push runs.
        bool hasExtras = false;
        int sent = 0;
        int failed = 0;
};
