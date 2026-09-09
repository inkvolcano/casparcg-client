// Where a client reports what it holds.
//
// A venue pulling from a PHP relay checked in to that relay. A venue pulling from
// a private GitHub repository checked in nowhere, and that was deliberate rather
// than an oversight: a client's GitHub token is read-only by design, and writing
// a check-in file into the templates repository would mean every venue holding a
// token that can push templates to every other venue. The report is not worth
// that trade, and this file exists partly so nobody quietly makes it later.
//
// The way out is that "where do templates come from" and "where does this machine
// report" are two questions that only looked like one because a relay answers
// both. Separating them gives a GitHub venue its check-ins back with no token
// upgrade anywhere.

#include "../src/Common/CheckInTarget.h"

#include <QtCore/QString>
#include <QtCore/QTextStream>

static int failures = 0;
static int checks = 0;

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

static void same(const QString& actual, const QString& wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what
                        << "  (wanted \"" << wanted << "\", got \"" << actual << "\")\n";
}

static void aRelayVenueStillReportsToItsRelay()
{
    // Nothing configured beyond the relay: exactly as before this existed.
    CheckInTarget::Decision d = CheckInTarget::decide(
        false, "https://host/relay.php", "dl-token", "", "");

    expectTrue(d.send, "a relay venue reports");
    same(d.url, "https://host/relay.php", "to the relay it pulls from");
    same(d.token, "dl-token", "with the token it pulls with");
}

static void aGitHubVenueReportsNowhereUntilItIsTold()
{
    // The old behaviour, and still the default: no writable token is invented.
    CheckInTarget::Decision d = CheckInTarget::decide(
        true, "https://api.github.com/repos/me/templates", "ghp_readonly", "", "");

    expectTrue(!d.send, "a GitHub venue reports nowhere by default");
    expectTrue(!d.reason.isEmpty(), "and says why");
    expectTrue(d.reason.contains("GitHub"), "naming the reason");

    // The thing that must never happen: a destination being invented for a
    // source whose token is deliberately read-only.
    expectTrue(d.url.isEmpty(), "and no destination is invented");
}

static void aGitHubVenueWithACheckInAddressReports()
{
    // The fix. Templates still come from GitHub with a read-only token; the
    // report goes to a relay that holds no packs.
    CheckInTarget::Decision d = CheckInTarget::decide(
        true, "https://api.github.com/repos/me/templates", "ghp_readonly",
        "https://host/logbook.php", "relay-dl-token");

    expectTrue(d.send, "a GitHub venue with an address reports");
    same(d.url, "https://host/logbook.php", "to that address");
    same(d.token, "relay-dl-token", "with that address's own token");

    // The GitHub token has no business being handed to a third party.
    expectTrue(d.token != QString("ghp_readonly"), "and never with the GitHub token");
}

static void theAddressWinsOverTheRelayToo()
{
    // A relay venue can report somewhere else — one collector for an estate that
    // pulls from several relays.
    CheckInTarget::Decision d = CheckInTarget::decide(
        false, "https://packs.example/relay.php", "pack-token",
        "https://logbook.example/relay.php", "log-token");

    expectTrue(d.send, "an explicit address is used");
    same(d.url, "https://logbook.example/relay.php", "rather than the pull address");
    same(d.token, "log-token", "with its own token");
}

static void theTokenFallsBackRatherThanBeingTypedTwice()
{
    // Pointing a venue at the relay it already pulls from, spelled out. Making
    // somebody retype a token is a way to get it wrong once.
    CheckInTarget::Decision d = CheckInTarget::decide(
        false, "https://host/relay.php", "dl-token", "https://host/relay.php", "");

    expectTrue(d.send, "an address with no token of its own still reports");
    same(d.token, "dl-token", "falling back to the source token");
}

static void aGitHubTokenIsNeverSentToARelay()
{
    // The fallback above is right for a relay reporting to itself and wrong across
    // routes. A GitHub client's source token is a repository credential, and the
    // fallback would have posted it as X-Relay-Token to whatever host was in the
    // check-in field - handing a repository token to a third party because a box
    // was left empty. Read-only or not, that is a credential leaking on a typo.
    CheckInTarget::Decision d = CheckInTarget::decide(
        true, "https://api.github.com/repos/me/templates", "github_pat_secret",
        "https://someone-elses-host/logbook.php", "");

    expectTrue(!d.send, "a GitHub venue with no check-in token of its own sends nothing");
    expectTrue(d.token != "github_pat_secret", "and the GitHub token is not the one it would have sent");
    expectTrue(d.reason.contains("GitHub token is not used here"),
               "the reason says the GitHub token is not the answer");

    // Spelled out, it reports, and with its own token rather than the other one.
    CheckInTarget::Decision told = CheckInTarget::decide(
        true, "https://api.github.com/repos/me/templates", "github_pat_secret",
        "https://host/logbook.php", "relay-token");

    expectTrue(told.send, "given the relay's own token it reports");
    same(told.token, "relay-token", "and sends that one");
}

static void anAddressWithNoTokenAnywhereIsRefused()
{
    // A GitHub venue has no relay token to fall back on. Posting with no
    // credential would be refused by the relay anyway; saying so here is more use
    // than a silent failure at poll time.
    CheckInTarget::Decision d = CheckInTarget::decide(
        true, "https://api.github.com/repos/me/templates", "",
        "https://host/logbook.php", "");

    expectTrue(!d.send, "no token anywhere means no report");
    expectTrue(d.reason.contains("token"), "and the reason names the token");
}

static void nothingConfiguredReportsNothing()
{
    CheckInTarget::Decision d = CheckInTarget::decide(false, "", "", "", "");

    expectTrue(!d.send, "a client with no source at all reports nothing");
    expectTrue(!d.reason.isEmpty(), "and says so");

    expectTrue(!CheckInTarget::decide(false, "https://host/relay.php", "", "", "").send,
               "a relay with no token reports nothing");
}

static void surroundingSpaceDoesNotChangeTheAnswer()
{
    // These come out of text fields somebody pasted into.
    CheckInTarget::Decision d = CheckInTarget::decide(
        true, " https://api.github.com/x ", " ghp ", "  https://host/logbook.php  ", "  log-token  ");

    expectTrue(d.send, "a pasted address still works");
    same(d.url, "https://host/logbook.php", "and is trimmed");
    same(d.token, "log-token", "and so is the token");

    // A field holding only spaces is empty, not an address.
    expectTrue(!CheckInTarget::decide(true, "https://api.github.com/x", "ghp", "   ", "").send,
               "an address of only spaces is no address");
}

static void anUnchangedVenueStopsRepeatingItself()
{
    // The point of the whole change. A machine that is up to date said the same
    // few hundred bytes four times an hour forever, and the body carries no
    // timestamp, so every one of those reports was byte-identical to the last.
    expectTrue(CheckInTarget::reportFor("abc", "abc", 60, 720) == CheckInTarget::Report::Nothing,
               "nothing changed and the heartbeat is not due, so nothing is said");

    expectTrue(CheckInTarget::reportFor("def", "abc", 60, 720) == CheckInTarget::Report::Changed,
               "a different state reports at once");
}

static void aFailureIsAChangeAndReportsImmediately()
{
    // result and failed are in the body, so a venue that starts failing has a
    // different digest and does not wait for the heartbeat to say so. This is the
    // property that makes reporting-on-change safe rather than merely quieter.
    expectTrue(CheckInTarget::reportFor("ok-2-installed", "ok-idle", 5, 720)
                   == CheckInTarget::Report::Changed,
               "a venue that just started failing is heard on the next poll");
}

static void theFirstReportOfARunAlwaysGoesOut()
{
    // Nothing has been accepted yet, so there is no record to be consistent with.
    // A restarted client and a collector redeployed underneath one look the same
    // from here, and both want the record put back rather than assumed.
    expectTrue(CheckInTarget::reportFor("abc", "", -1, 720) == CheckInTarget::Report::Changed,
               "a client that has sent nothing yet reports");
    expectTrue(CheckInTarget::reportFor("abc", "", 90000, 720) == CheckInTarget::Report::Changed,
               "an empty last digest reports even if the clock says otherwise");
    expectTrue(CheckInTarget::reportFor("abc", "abc", -1, 720) == CheckInTarget::Report::Changed,
               "and so does a matching digest that was never actually accepted");
}

static void theHeartbeatKeepsTheTimestampMeaningful()
{
    // Without this a venue silent for a fortnight is either fine or dead and there
    // is no way to tell, which is the question worth answering before a show.
    expectTrue(CheckInTarget::reportFor("abc", "abc", 719 * 60, 720) == CheckInTarget::Report::Nothing,
               "a minute short of due says nothing");
    expectTrue(CheckInTarget::reportFor("abc", "abc", 720 * 60, 720) == CheckInTarget::Report::Heartbeat,
               "exactly due sends the heartbeat");
    expectTrue(CheckInTarget::reportFor("abc", "abc", 5000 * 60, 720) == CheckInTarget::Report::Heartbeat,
               "and long overdue still only sends one");
}

static void theHeartbeatCanBeTurnedOff()
{
    // A legitimate choice for somebody who would rather have the silence, so it is
    // off rather than clamped to some minimum nobody asked for.
    expectTrue(CheckInTarget::reportFor("abc", "abc", 99999, 0) == CheckInTarget::Report::Nothing,
               "zero minutes means changes only");
    expectTrue(CheckInTarget::reportFor("abc", "abc", 99999, -5) == CheckInTarget::Report::Nothing,
               "and so does a negative, rather than meaning always");

    // Turning the heartbeat off must never silence an actual change.
    expectTrue(CheckInTarget::reportFor("def", "abc", 1, 0) == CheckInTarget::Report::Changed,
               "a change still reports with no heartbeat");
}

static void theEndpointKeepsTheRelayShape()
{
    same(CheckInTarget::endpointFor("https://host/relay.php"),
         "https://host/relay.php?action=checkin", "a plain URL gets its query");

    // A relay behind a rewrite may already carry one.
    same(CheckInTarget::endpointFor("https://host/relay.php?venue=north"),
         "https://host/relay.php?venue=north&action=checkin", "an existing query is appended to");

    same(CheckInTarget::endpointFor("  https://host/relay.php  "),
         "https://host/relay.php?action=checkin", "and it is trimmed first");

    expectTrue(CheckInTarget::endpointFor("").isEmpty(), "nothing makes nothing");
    expectTrue(CheckInTarget::endpointFor("   ").isEmpty(), "and so does only space");
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "Check-in target\n";

    aRelayVenueStillReportsToItsRelay();
    aGitHubVenueReportsNowhereUntilItIsTold();
    aGitHubVenueWithACheckInAddressReports();
    theAddressWinsOverTheRelayToo();
    theTokenFallsBackRatherThanBeingTypedTwice();
    aGitHubTokenIsNeverSentToARelay();
    anAddressWithNoTokenAnywhereIsRefused();
    nothingConfiguredReportsNothing();
    surroundingSpaceDoesNotChangeTheAnswer();

    anUnchangedVenueStopsRepeatingItself();
    aFailureIsAChangeAndReportsImmediately();
    theFirstReportOfARunAlwaysGoesOut();
    theHeartbeatKeepsTheTimestampMeaningful();
    theHeartbeatCanBeTurnedOff();

    theEndpointKeepsTheRelayShape();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}
