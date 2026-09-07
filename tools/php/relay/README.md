# Template relay

A single PHP file that lets a dev machine update templates on clients it cannot
reach.

There is a second way to do the same job with no host of your own: a private
GitHub repository. See [GITHUB.md](GITHUB.md).

## Why

Pushing straight from the dev machine to a client works, and it is the better
option on one network. It needs the client to accept an inbound connection on an
endpoint that writes HTML that CasparCG will run. At a venue, on someone else's
network, that is not a port anyone should be opening.

So the direction is turned around. The dev machine uploads a pack here; each
client asks this what is there and fetches what it does not already have. Both
ends make outbound connections only. Neither has to know where the other is.

```
  dev machine  ──upload──▶   relay.php   ◀──poll & fetch──  client
                             (your host)                    client
                                                            client
```

## Install

1. Copy `relay.php` **and the `.user.ini` and `.htaccess` beside it** to a PHP
   host. PHP 7.4 or newer. Those two turn off `display_errors` for the relay: a
   PHP warning printed in front of a JSON answer breaks every client, and some
   warnings happen before `relay.php` runs, so no code inside it can catch them.
   They are dotfiles, so check your upload tool is not hiding them.
2. Open it and change `UPLOAD_TOKEN` and `DOWNLOAD_TOKEN`. Two different
   long random strings. A relay left on the shipped values is a relay anyone can
   write templates to.
3. Load `relay.php?action=selftest` with the **upload** token. It checks the
   things that go wrong quietly: default or matching tokens, storage that is
   reachable over the web, plain HTTP, an upload limit smaller than advertised,
   and errors that would be printed into responses.

```bash
curl -H "X-Relay-Token: YOUR-UPLOAD-TOKEN" "https://example.com/relay/relay.php?action=selftest"
```

It answers `200` when nothing is wrong and `500` when something is, so it can be
put in a monitor.

```bash
curl -H "X-Relay-Token: YOUR-DOWNLOAD-TOKEN" "https://example.com/relay/relay.php?action=ping"
```

**Put it behind HTTPS.** The tokens travel in a header and the templates travel
as plain bytes. Any host with a certificate on it is enough. This is the one
thing that makes the relay safe to expose, and it is not something the PHP file
can do for you.

`relay_data/` is created next to `relay.php` on first use. Move it outside the
web root if the host allows, and set `STORAGE` to the new path. The `.htaccess`
written into it only covers Apache.

## The two tokens

They are separate on purpose.

| Token | Held by | Can |
|---|---|---|
| `UPLOAD_TOKEN` | the dev machine, in the push tool | upload, remove |
| `DOWNLOAD_TOKEN` | every client | list, fetch |

A client that is stolen or resold reads only what it was already going to
install. It cannot put a template on the relay for the other clients to pick up.

## Dev machine

In **CasparCG Template Push**, add a row under Clients and put the full relay
address where a host and port would go. Anything with `://` in it is treated as
a relay.

| Name | Host : Port | Token |
|---|---|---|
| relay | `https://example.com/relay/relay.php` | the **upload** token |

Then work as before. Identify says which relay answered and whether the token
can upload. Compare lists what differs between the local pack and the relay.
Push uploads only the ticked rows.

Direct clients and relays can sit in the table together and be pushed to in one
pass.

## Client

**Settings → Templates → Pull Packs From A Relay.**

| Field | |
|---|---|
| Relay address | the same URL |
| Download token | the **download** token, never the upload one |
| Check every | how often to poll, default 15 minutes |
| Packs | leave empty to follow everything, or list the packs this client owns |

**Test** reaches the relay and writes nothing. **Check now** polls immediately.
The client also checks a few seconds after it starts, because a machine that was
switched off is the one most likely to be behind.

One relay can carry every venue's packs while each client takes only its own,
which is what the Packs field is for.

## Deciding who gets what

One relay can carry every venue. Which packs a machine takes can be set on the
machine, in its Packs field, or centrally here so nobody has to visit a venue to
change it.

```bash
curl -X POST -H "X-Relay-Token: YOUR-UPLOAD-TOKEN" -H "Content-Type: application/json" \
  --data '{"STUDIO-A":["SEVILLE","SHARED"],"TRUCK-2":["MARSEILLE","SHARED"],"*":["SHARED"]}' \
  "https://example.com/relay/relay.php?action=assignments"
```

A name is a machine name, the same one it checks in under. The `*` entry is what a
machine gets when it is not named.

A machine named nowhere, with no `*` to fall back on, keeps whatever it was set to
locally. Adding this file therefore cannot silently stop an existing machine from
updating.

**A machine named with an empty list takes nothing.** That is a real instruction and
is different from not being named at all, and the client keeps the two apart. It has
to: an empty pack filter further down means *every* pack, so confusing them would
hand a venue the whole estate.

The reply says what it would not take:

- `dropped` is names it refused to store at all, such as one that is not a usable
  machine name, or a list that was not a list.
- `unknownPacks` is packs that were assigned but are not on this relay. Those are
  **not** refused, because assigning a pack before uploading it is a reasonable
  order to work in. But `SEVILE` instead of `SEVILLE` looks exactly the same to
  every other check and would otherwise deliver nothing to that venue with no sign
  of why, so it is named here instead.

A single machine can be taken back out of central control. Tick **Ignore what this
machine is assigned** in its own Settings, and its own Packs field wins there.

## What it will not do

- **It never deletes on its own.** A file removed from the relay stays on every
  client that already has it. Removing a template from a machine that may be on
  air is not a decision worth making from the other side of the internet.

  Compare shows those leftovers as **only there**, and the push tool's **Clear**
  button takes ticked ones off the relay after asking. That stops a renamed file
  being served forever. It still never touches a client.
- **`project.js` and `extensions.json` never travel.** Each client owns its API
  key, its `local` flag and its Sheets panel buttons. The relay refuses to store
  them and the client refuses to write them, so neither end depends on the other
  being careful.
- **A path can never leave its pack.** Checked on the way in and on the way out,
  including Windows device names such as `con.html`.
- **Bytes are checked in both directions.** A client hashes what it fetched and
  refuses to install anything that is not what the manifest promised. The relay
  hashes what it receives and refuses to store anything that is not what the
  sender claimed.

## Endpoints

| | |
|---|---|
| `GET ?action=ping` | is this a relay, how many packs, can this token upload |
| `GET ?action=manifest` | every pack, every file, every digest |
| `GET ?action=manifest&pack=X` | one pack |
| `GET ?action=fetch&pack=X&path=Y` | one file, with `X-Relay-Sha1` |
| `POST ?action=upload&pack=X&path=Y` | body is the file; send `X-Content-Sha1` and it is checked before storing |
| `POST ?action=remove&pack=X&path=Y` | removes it here only |
| `POST ?action=checkin` | a client reporting what it now has |
| `GET ?action=clients` | who has checked in, and who is behind (upload token) |
| `GET ?action=selftest` | is this relay set up safely (upload token) |
| `GET ?action=assignments` | which client gets which packs |
| `POST ?action=assignments` | set that (upload token) |

All of them want `X-Relay-Token`. Wrong or missing is `401`.
