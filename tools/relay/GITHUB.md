# Templates from a private GitHub repository

The other way to reach clients on a network you do not control. Same idea as the
relay: the dev machine puts packs somewhere both ends can reach, and each client
pulls. Nothing inbound is opened at the venue.

Compared to the relay, GitHub costs nothing to run, does the authentication and
the HTTPS for you, and keeps the history of every template you have ever shipped
as a side effect. You can also roll a bad template back with a revert.

The relay is still the better answer if you cannot rely on github.com being
reachable from a venue, or if you would rather not put templates on someone
else's service at all.

## The repository

Make it **private**. A public repository means anyone can read your templates.

One folder at the root per pack. Nothing else about the layout matters.

```
  your-templates/
    SEVILLE/
      calendar.html
      css/site.css
    MARSEILLE/
      incidents.html
    README.md          <- root files are ignored
    .github/           <- dot folders are ignored
```

## Tokens

Use fine-grained personal access tokens, one per job, scoped to this repository
and nothing else.

| Where | Repository access | Permission |
|---|---|---|
| Each client | only this repository | **Contents: read-only** |
| The push tool | only this repository | **Contents: read and write** |

A client token that can only read is the point. A venue machine that goes missing
cannot then change what every other machine installs.

Make them at **Settings → Developer settings → Personal access tokens →
Fine-grained tokens**. Give them an expiry you will actually notice.

## Client

**Settings → Templates → Pull Packs From A Relay Or GitHub.**

| Field | |
|---|---|
| Source | `github:owner/repo`, or `github:owner/repo@branch` |
| Token | that client's read-only token |
| Check every | how often to poll, default 15 minutes |
| Packs | leave empty for every pack, or name the ones this client owns |

Leave the branch off and it follows the repository's default branch.

**Test** reads the repository and reports its name, default branch, and whether
it is private. It writes nothing, and it will tell you loudly if the repository
is public.

## Dev machine

Two ways, and the first one needs no tool at all.

**Just use git.** Clone the repository, copy your pack in, commit, push. Every
client picks it up on its next poll. If you already work this way, there is
nothing else to set up.

**Or use the push tool**, for the same review-before-you-send flow as the relay.
Add a row under Clients:

| Name | Host : Port | Token |
|---|---|---|
| templates | `github:owner/repo` | the read **and write** token |

Identify reports the repository and warns if it is public. Compare lists what
differs. Push commits the ticked files, one commit per file, with a message
naming what changed. Clear removes ticked leftovers, also as commits, so nothing
is ever really lost.

Clients, relays and repositories can sit in the table together and be pushed to
in one pass.

## How a client knows what to fetch

One request lists the whole repository tree with a digest per file. Those digests
are Git blob hashes, so the client hashes its own files the same way and fetches
only the ones that differ. A pack that has not changed costs one request for the
whole estate.

Each file is then fetched **by its digest**, not by its path, so a branch that
moves mid-poll cannot hand a client a different file than the one it was told
about. The bytes are hashed again on arrival and refused if they do not match.

## The same rules as everywhere else

- **Nothing is ever deleted on a client.** Removing a file from the repository
  removes it from future installs, not from machines that already have it.
- **`project.js` and `extensions.json` are never installed.** Each client owns
  its own API key, `local` flag and Sheets panel buttons.
- **A path can never leave its pack**, Windows device names included.
- **Bytes are checked against the digest** before anything is written.

## What the relay has and this does not

**You cannot see which clients have caught up.** A relay knows, because clients
report to it after each poll. Here they cannot: a client's token is read-only,
and writing a report back to the repository would mean giving every venue machine
write access to the templates every other machine installs. That is a bad trade
for a status line.

If knowing which venues are current matters more to you than read-only client
tokens, use the relay. Otherwise check a client from its own Server Status panel,
where the relay row shows when it last checked and what happened.

## Limits worth knowing

- An authenticated token gets 5000 API requests an hour. A poll costs one request
  plus one per changed file, so this is not a limit you will meet.
- A repository too large for a single tree listing is refused rather than
  guessed at, because a truncated listing looks exactly like missing files.
- Files over 100 MB will not go into a Git repository at all. Templates are not
  that, but video is.
