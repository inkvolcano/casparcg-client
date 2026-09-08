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

## Seeing which clients have caught up

A GitHub repository has nowhere for a client to report to. Writing a check-in
file back into it would mean every venue machine holding a token that can push
templates to every other machine, which is a bad trade for a status line — so
clients on this route never write to the repository, and they never will.

That is a limit of the repository, not of the route. **Where templates come from
and where a machine reports are two separate questions**, and only looked like
one because a relay answers both. Give a client a check-in address and it reports
there while still pulling from GitHub with a read-only token:

**Settings → Templates → Report to** — the address of a relay.
**Report token** — that relay's `DOWNLOAD_TOKEN`. Left empty it reuses the token
above, which is wrong for this route, so fill it in.

The relay you point at **does not need to hold any packs**. Deploy `relay.php`
as normal, set both tokens, and leave its storage empty: it is a logbook. Every
client that reports there shows up in **Assignments...** in the push tool and in
`?action=clients`, whether it pulled from that relay or from GitHub. The record
carries a `source` field saying which.

Nothing about the pull changes. The GitHub token stays read-only, the repository
is still never written to, and a client with no check-in address set behaves
exactly as before — it simply reports nowhere.

### If you would rather not host anything at all

Then check a client from its own Server Status panel, where the relay row shows
when it last checked and what happened. That is one machine at a time, which is
the trade for having nothing to deploy.

## GitHub Enterprise Server

A self-hosted GitHub answers the same API at its own address, usually
`https://github.example.com/api/v3`. Point the client at it with a database
setting, `RelayGitHubApi`. There is no field for it in Settings on purpose:
almost nobody needs one, and left empty it means github.com.

In the push tool, write the whole thing into the address instead:

```
github:https://github.example.com/api/v3/owner/repo
github:https://github.example.com/api/v3/owner/repo@branch
```

## Limits worth knowing

- An authenticated token gets 5000 API requests an hour. A poll costs one request
  plus one per changed file, so this is not a limit you will meet.
- A repository too large for a single tree listing is refused rather than
  guessed at, because a truncated listing looks exactly like missing files.
- Files over 100 MB will not go into a Git repository at all. Templates are not
  that, but video is.
