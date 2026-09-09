#pragma once

// Where a client reports what it holds.
//
// A venue pulling from a PHP relay checks in to that relay: same host, same
// token, nothing to configure. A venue pulling from a private GitHub repository
// checked in nowhere, and that was on purpose — a client's GitHub token is
// read-only by design, and writing a check-in file into the templates repository
// would mean every venue holding a token that can push templates to every other
// venue. The report is not worth that.
//
// The way out is that the two questions are not actually the same question.
// "Where do templates come from" and "where does this machine report" only
// looked joined because the relay answers both. Separating them costs one
// setting and gives a GitHub venue its check-ins back, with no token upgrade
// anywhere: it reports to a relay — or to anything that accepts the same POST —
// while still pulling its templates from GitHub.
//
// A relay used only for this holds no packs and needs no upload token. It is a
// few lines of PHP acting as a logbook.
//
// Header-only so the rule can be compiled and tested on its own.

#include <QtCore/QString>

namespace CheckInTarget
{
    struct Decision
    {
        bool send = false;
        QString url;    // the base URL to post the check-in to
        QString token;  // the token to send with it
        QString reason; // why nothing is sent, when nothing is
    };

    // sourceUrl / sourceToken are what the client pulls templates from.
    // checkInUrl / checkInToken are the optional override.
    //
    // sourceIsGitHub decides what the default is: a relay is its own logbook, a
    // GitHub repository is not one at all.
    inline Decision decide(bool sourceIsGitHub,
                           const QString& sourceUrl, const QString& sourceToken,
                           const QString& checkInUrl, const QString& checkInToken)
    {
        Decision decision;

        const QString overrideUrl = checkInUrl.trimmed();

        if (!overrideUrl.isEmpty())
        {
            // An explicit destination wins whatever the templates come from. The
            // token falls back to the source's, because a relay venue reporting to
            // a relay has one already and being made to type it twice is a way to
            // get it wrong once.
            //
            // It never falls back across routes. A GitHub client's source token is
            // a GitHub credential, and sending it as X-Relay-Token to whatever host
            // is in the check-in field would hand a repository token to a third
            // party because somebody left a box empty. Read-only or not, that is
            // not ours to leak.
            const QString explicitToken = checkInToken.trimmed();

            decision.token = explicitToken.isEmpty() && !sourceIsGitHub ? sourceToken.trimmed() : explicitToken;

            if (decision.token.isEmpty())
            {
                decision.reason = sourceIsGitHub
                    ? "A check-in address is set but no check-in token. The GitHub token is "
                      "not used here, so this needs the relay's own token."
                    : "A check-in address is set but there is no token to send with it.";
                return decision;
            }

            decision.send = true;
            decision.url = overrideUrl;

            return decision;
        }

        // No override. A GitHub source has nowhere to report that does not cost a
        // writable token, so it reports nowhere and says so.
        if (sourceIsGitHub)
        {
            decision.reason = "Templates come from GitHub, which has no check-in. "
                              "Set a check-in address to report to a relay instead.";
            return decision;
        }

        if (sourceUrl.trimmed().isEmpty() || sourceToken.trimmed().isEmpty())
        {
            decision.reason = "No relay is configured, so there is nothing to report to.";
            return decision;
        }

        decision.send = true;
        decision.url = sourceUrl.trimmed();
        decision.token = sourceToken.trimmed();

        return decision;
    }

    // Whether this poll has anything worth reporting.
    //
    // A check-in used to go out on every poll, which for a machine that is simply
    // up to date is the same few hundred bytes saying the same thing four times an
    // hour forever. The body carries no timestamp - the collector stamps arrival -
    // so two idle polls produce byte-identical reports, and "has this changed"
    // is a digest comparison rather than a schema question.
    //
    // Two questions are being answered, and only one of them changes when an
    // update lands:
    //
    //   what does this venue hold   - changes on install, and on a failure, since
    //                                 the result and failure count are in the body
    //   is this venue reachable     - changes never, and must therefore be said
    //                                 on a timer or it is not being said at all
    //
    // Reporting only on change answers the first perfectly and the second not at
    // all: a venue silent for a fortnight is either fine, or dead, and that is
    // exactly the thing worth knowing before a show. So both - on change, and on a
    // slow heartbeat that keeps the timestamp meaningful.
    enum class Report
    {
        Nothing,
        Changed,    // the state differs from what was last accepted
        Heartbeat   // unchanged, but quiet for long enough to be worth confirming
    };

    // secondsSinceLastSend is negative when nothing has been sent yet this run.
    // heartbeatMinutes of zero or less turns the heartbeat off, leaving changes
    // only - which is a legitimate choice for somebody who does not want the
    // liveness signal and would rather have the silence.
    inline Report reportFor(const QString& digest, const QString& lastDigest,
                            qint64 secondsSinceLastSend, int heartbeatMinutes)
    {
        // Nothing accepted yet, so there is no record to be consistent with. This
        // covers a restarted client and a collector that was redeployed underneath
        // one: both need the record put back rather than assumed.
        if (secondsSinceLastSend < 0 || lastDigest.isEmpty())
            return Report::Changed;

        if (digest != lastDigest)
            return Report::Changed;

        if (heartbeatMinutes <= 0)
            return Report::Nothing;

        return secondsSinceLastSend >= static_cast<qint64>(heartbeatMinutes) * 60
            ? Report::Heartbeat : Report::Nothing;
    }

    // The relay speaks one URL shape, and a check-in collector is a relay whether
    // or not it holds any packs.
    inline QString endpointFor(const QString& base)
    {
        const QString trimmed = base.trimmed();
        if (trimmed.isEmpty())
            return QString();

        return trimmed + (trimmed.contains('?') ? "&action=checkin" : "?action=checkin");
    }
}
