# Template relay

A single PHP file that lets a dev machine update templates on clients it cannot
reach.

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

1. Copy `relay.php` somewhere on a PHP host. PHP 7.4 or newer.
2. Open it and change `UPLOAD_TOKEN` and `DOWNLOAD_TOKEN`. Two different
   long random strings. A relay left on the shipped values is a relay anyone can
   write templates to.
3. Load `relay.php?action=ping` with a token header to check it answers.

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

## What it will not do

- **It never deletes.** A file removed from the relay stays on every client that
  already has it. Removing a template from a machine that may be on air is not a
  decision worth making from the other side of the internet.
- **`project.js` and `extensions.json` never travel.** Each client owns its API
  key, its `local` flag and its Sheets panel buttons. The relay refuses to store
  them and the client refuses to write them, so neither end depends on the other
  being careful.
- **A path can never leave its pack.** Checked on the way in and on the way out,
  including Windows device names such as `con.html`.
- **Bytes are checked against the manifest.** A client hashes what it fetched and
  refuses to install it if that is not what the relay said it would be.

## Endpoints

| | |
|---|---|
| `GET ?action=ping` | is this a relay, how many packs, can this token upload |
| `GET ?action=manifest` | every pack, every file, every digest |
| `GET ?action=manifest&pack=X` | one pack |
| `GET ?action=fetch&pack=X&path=Y` | one file, with `X-Relay-Sha1` |
| `POST ?action=upload&pack=X&path=Y` | body is the file |
| `POST ?action=remove&pack=X&path=Y` | removes it here only |

All of them want `X-Relay-Token`. Wrong or missing is `401`.
