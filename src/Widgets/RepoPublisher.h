#pragma once

#include "Shared.h"

#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtNetwork/QNetworkAccessManager>

#include <functional>

// Pushes a master client's own template edits to the GitHub source.
//
// Everything a venue plays comes from the source, so an edit made at a venue is
// lost on the next poll unless it goes back. This is the way back, and it is
// deliberately narrow: only from a button in Settings, never on its own; only
// with a second token that can write, which the pull token cannot; never a
// deletion; never project.js or extensions.json; only the packs this client
// follows; and only after showing what will go and taking a commit message.
//
// The mechanics are the Git Data API, one commit: the changed files become
// blobs, the blobs a tree on top of the current one, the tree a commit on top
// of the current head, and the branch is moved to it. No git on the machine.
class WIDGETS_EXPORT RepoPublisher : public QObject
{
    Q_OBJECT

    public:
        static RepoPublisher& getInstance();

        // Whether this client is allowed to push at all. Off by default.
        static bool isMaster();

        // The token that can write. Separate from the pull token on purpose: a
        // venue that only pulls never holds one.
        static QString pushToken();

        // One file that would go.
        struct Change
        {
            QString pack;
            QString path;        // relative to the pack, forward slashes
            QString sha;         // git blob sha of the local bytes
            bool isNew = false;  // not in the repository at all

            QString repoPath() const { return pack + "/" + path; }
        };

        // What differs, for one pack: files here that are new or changed there.
        // Never a deletion, never a protected file. A file the repository has and
        // this machine does not is not a change - it is a sign this machine has
        // not pulled, and is reported in missingLocally so the push can refuse.
        // Pure, so it can be tested on maps.
        static QList<Change> plan(const QString& pack, const QMap<QString, QString>& local,
                                  const QMap<QString, QString>& remote, QStringList* missingLocally);

        bool isBusy() const { return this->busy; }

        // Step one: read the repository and work out what would go. Answers on
        // planned(). Sends nothing.
        void prepare();

        // Step two: send what prepare found, as one commit with this message.
        // Answers on pushed(). Refused if prepare has not run or found nothing.
        void push(const QString& message);

        // What prepare found, as pack/path.
        QStringList plannedFiles() const;

    Q_SIGNALS:
        void progress(const QString& line);

        // files is what would go. problem, when set, is why nothing can: no write
        // token, not a GitHub source, behind the repository.
        void planned(const QStringList& files, const QString& problem);

        void pushed(bool ok, const QString& summary);

    private:
        explicit RepoPublisher();

        typedef std::function<void(int status, const QJsonObject& body)> Answer;

        // One call to the API with the write token. verb is GET, POST or PATCH;
        // path is what follows /repos/{owner}/{repo}.
        void call(const QString& verb, const QString& path, const QJsonObject& body, Answer answer);

        void readTree();
        void planFromTree(const QJsonObject& tree);

        void fail(const QString& why);
        void say(const QString& line);

        // The push, one step at a time.
        void uploadNextBlob();
        void createTree();
        void createCommit(const QString& treeSha);
        void moveBranch(const QString& commitSha);

        QNetworkAccessManager* network = nullptr;
        bool busy = false;
        bool pushing = false;   // which of the two signals a failure ends on

        // What prepare found, and what push works through.
        QString branch;
        QString headSha;
        QString baseTreeSha;
        QList<Change> changes;
        int nextBlob = 0;
        QString message;
        QList<QJsonObject> treeEntries;
};
