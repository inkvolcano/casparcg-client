#pragma once

#include "Shared.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonObject>
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
        // Where this machine reports, when that is not where it pulls from. Set
        // these to give a GitHub-route venue check-ins without a writable token.
        static QString checkInUrl();
        static QString checkInToken();

        // How long a venue may stay silent before it says so anyway. Reporting only
        // on change answers "what does this venue hold" and cannot answer "is it
        // still reachable", and a machine quiet for a fortnight is either fine or
        // dead. Zero turns it off, leaving changes only.
        static int heartbeatMinutes();

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
        //
        // This is the local answer. The source can override it per machine, which is
        // what lets one person decide the whole estate from one place instead of
        // visiting every venue.
        static QStringList packFilter();

        // What the source says this machine should have, or empty when it says
        // nothing about it. Named by machine name, the same one a check-in uses.
        static QStringList assignedPacks(const QJsonObject& assignments);

        // The list actually used for a poll: the source's answer when it has one,
        // and the local setting when it does not. Never silently empty, because an
        // empty list means "every pack" and that is not a safe way to be wrong.
        static QStringList packsForThisMachine(const QJsonObject& assignments);

        // Whether this machine ignores what it is assigned and uses its own list.
        // Off by default: the point of assignments is that one person decides.
        static bool packsDecidedLocally();

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

        // A repository keeps its assignments in a file at its root, so the tree has
        // to be held while that file is fetched and the planning happens after.
        void fetchGitHubAssignments(const QByteArray& treeJson, const QString& blobSha);

        // What the repository's assignments.json said this poll, empty when it has
        // none or could not be read.
        QJsonObject gitHubAssignments;

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

        // What was last accepted by the collector, and when. A check-in used to go
        // out on every poll; it now goes out when this digest changes, or when the
        // heartbeat falls due, so an idle venue stops repeating itself.
        //
        // Both are recorded on success only. A report that never arrived is not a
        // report, and leaving the digest alone means the next poll retries it
        // without any retry machinery existing.
        QString lastReportDigest;
        QDateTime lastReportAt;
        int manifestAttempts = 0;
};
