#pragma once

#include "Shared.h"

#include <QtCore/QDateTime>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>

class QNetworkAccessManager;
class QNetworkRequest;
class QTimer;

// Pulling template packs from a relay, or from a private GitHub repository.
//
// The other half of the push story, and the half that works across the internet.
// Instead of a dev machine reaching in to this client, this client reaches out to a
// small PHP relay and asks what is there. Nothing inbound is opened at the venue,
// nobody needs to know this machine's address, and a client behind any amount of
// NAT is reachable in the only direction that matters.
//
// What it does on each poll:
//
//   1. asks for a manifest: every pack, every file, every digest
//   2. keeps the packs this client is meant to follow
//   3. compares each file's digest against the copy already on disk
//   4. fetches only what differs, checks the bytes against the promised digest,
//      and installs through TemplateInstaller
//
// Two kinds of source, and only the first two steps differ between them.
//
//   relay      an address, and the PHP in tools/php/relay
//   github:owner/repo[@branch]     a private repository, packs at its root
//
// GitHub costs nothing to run, authenticates for you, and keeps the history of
// every template as a side effect. One request lists the whole tree with a digest
// per file, and those digests are Git blob hashes, so a pack on disk can be
// compared against the repository without downloading anything.
//
// It never deletes. A file that vanished from the relay stays on this machine,
// because removing a template from a box that may be on air is not a decision worth
// taking from the other side of the internet without someone watching.
//
// project.js and extensions.json are skipped here and refused by the installer as
// well: this machine owns its API key, its local flag and its Sheets panel buttons.
class WIDGETS_EXPORT RelayClient : public QObject
{
    Q_OBJECT

    public:
        static RelayClient& getInstance();

        // Starts the poll timer if the relay is configured; safe to call again, and
        // the way a settings change is picked up.
        void start();
        void stop();

        static bool isEnabled();
        static QString url();
        static QString token();
        static int pollMinutes();

        // A "github:owner/repo" or "github:owner/repo@branch" address rather than
        // the address of a relay.
        static bool isGitHub();
        static QString gitHubOwnerRepo();

        // Empty means the repository's own default branch, which is what HEAD gets.
        static QString gitHubBranch();

        // Where the GitHub API lives. github.com unless a GitHub Enterprise Server
        // is configured, which has its own and is otherwise unreachable from here.
        static QString gitHubApi();

        // What to call this source in a status line, without leaking the token.
        static QString sourceLabel();

        // Which packs this client follows, empty meaning all of them. A relay can
        // carry every venue's packs while each client takes only its own.
        static QStringList packFilter();

        bool isBusy() const { return this->busy; }
        QDateTime lastRun() const { return this->ranAt; }
        QString lastSummary() const { return this->summary; }

        // Whether the last poll got what it went for. What a status light reads.
        bool lastCheckOk() const { return this->ok; }

        // A poll now, whatever the timer thinks. Returns false when one is already
        // running or the relay is not configured.
        bool checkNow();

        // Reach the relay and say what it is, writing nothing. The answer to "is the
        // URL right and is my token the right one", asked on its own rather than
        // discovered during a pull.
        Q_SLOT void ping();

    Q_SIGNALS:
        // Every line this would put in a log, so a dialog can show the same thing.
        void progress(const QString& line);

        // A poll ended. Installed and failed are file counts.
        void finished(int installed, int failed, const QString& summary);

    private:
        explicit RelayClient();

        void requestManifest();

        // Tell the relay what this machine now holds, so the dev machine can find
        // out who has caught up without being able to reach any of them.
        //
        // Relays only. A GitHub client's token is read-only on purpose, and that is
        // worth more than the report would be.
        void sendCheckIn();

        void planFrom(const QByteArray& manifestJson);
        void planFromGitHubTree(const QByteArray& treeJson);

        // The token and the headers each kind of source wants. GitHub needs an
        // Authorization header, an API version and a user agent; a relay needs one
        // header of its own.
        void authorise(QNetworkRequest& request) const;
        void fetchNext();
        void done(const QString& note);
        void say(const QString& line);

        // <url>?action=X, whichever way the operator wrote the URL.
        static QString endpoint(const QString& action);

        struct Wanted
        {
            QString pack;
            QString relativePath;
            QString sha1;      // what the manifest promised; the bytes must match it
            qint64 bytes = 0;
            int attempts = 0;
        };

        void fetchOne(const Wanted& wanted);

        // A dropped connection is worth another go; a refusal is an answer.
        static bool worthRetrying(int httpStatus);

        QNetworkAccessManager* network = nullptr;
        QTimer* timer = nullptr;

        QList<Wanted> queue;
        int installed = 0;
        int failed = 0;
        bool busy = false;

        // The version each followed pack had in the last listing, and the packs that
        // had a file fail. A pack with a failure is not reported as held, because a
        // half-installed pack is exactly what the dev machine must not be told is
        // current.
        QMap<QString, QString> versionByPack;
        QSet<QString> packsWithFailures;

        QDateTime ranAt;
        QString summary;
        bool ok = true;
        int manifestAttempts = 0;
};
