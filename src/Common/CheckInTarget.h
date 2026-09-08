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
            // token falls back to the source's, because pointing a GitHub venue
            // at a relay usually means it has a relay token already and being
            // made to type it twice is a way to get it wrong once.
            decision.token = checkInToken.trimmed().isEmpty() ? sourceToken.trimmed() : checkInToken.trimmed();

            if (decision.token.isEmpty())
            {
                decision.reason = "A check-in address is set but there is no token to send with it.";
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
