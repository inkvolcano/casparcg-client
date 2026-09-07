# The PHP side

Two files. Neither is required to run the client, and each solves a problem the
client cannot solve alone.

Drop them on any PHP host from 7.4 up. They keep their data in folders created
beside them, so nothing else needs setting up.

| | |
|---|---|
| `sheet_cache.php` | the cache templates read from, instead of Google |
| `strain_collector.php` | where every client reports usage, so one place knows the total |

The relay that distributes templates is a separate thing and lives in
[`../relay/`](../relay/README.md).

## sheet_cache.php

A Google Sheets API key has a quota per minute. Twenty templates each reading the
same sheet spend twenty reads on one answer. Pointed here they spend one, and the
rest come off this disk.

```
GET  ?spreadsheetId=X&sheetNumber=N    the stored rows, or 404
POST ?spreadsheetId=X&sheetNumber=N    store rows (body is the JSON)
```

A `404` is not an error. It is the answer a template is built to handle: it goes to
Google instead, and posts what it got back so the next template does not have to.

The client serves this same contract from its own port, so a template can be pointed
at either without being changed. Use the client's when one machine's templates want
a cache; use this when several machines should share one.

Two headers come back with a hit: `X-Sheet-Age` in seconds, and `X-Sheet-Stale`,
which is `true` past an hour. Nothing acts on them. They are there so a caller can
decide for itself.

**If you are replacing an older `local_server.php`, replace it.** That version built
its filename straight from the query, so a request naming a spreadsheet of
`../../something` wrote outside the data folder. No token was needed and it would
create the file. This one checks the id and the sheet number against a pattern
first, and the read contract is otherwise unchanged. The only difference a template
can see is that a POST which creates a file now reports success rather than
`{"error":"Sheet not found"}`.

## strain_collector.php

The quota is spent **per key, not per spreadsheet**. Two clients and a connector all
drawing on the same key can each be comfortably inside the limit and still take the
key past it together. None of them can see that alone.

So they all report here, and anything that needs the real number reads it back.

```
POST /strain_collector.php     a client or connector reporting itself
GET  /strain_collector.php     everyone's reports, and the totals per key
GET  ?format=flat              one line per key
```

Point a client at it with **Settings → Sheets → Report to URL**. It posts every ten
seconds while sheets are being read, and nothing at all when they are not.

### Reporting from a connector

Post the same shape the client posts. Only `source`, `host` and `sheets` are read;
anything else is stored and handed back untouched.

```json
{
  "source": "salvo-connector",
  "host": "SALVO-1",
  "sheets": [
    {
      "spreadsheetId": "1a2b3c",
      "project": "SEVILLE",
      "keyId": "k7f2",
      "total": 40,
      "clientReads": 40,
      "templateReads": 0
    }
  ]
}
```

`keyId` is what makes the totals mean anything: it groups everything drawing on one
budget. The client publishes a **digest of the API key, never the key**, so two
applications sharing a key report the same `keyId` without either of them telling
this file what the key is. A connector should do the same. Any stable string works
as long as everything on that key uses the identical one.

`total` is what that reporter spent on that sheet in the last minute.

### Reading it back

```json
{
  "keys": [
    { "keyId": "k7f2", "readsLastMinute": 95,
      "sheets": ["1a2b3c", "xyz"],
      "reporters": ["SALVO-1", "STUDIO-A", "TRUCK-2"] }
  ],
  "sheets":    [ ... per spreadsheet ... ],
  "reporters": [ ... each report as it arrived, with ageSeconds and current ... ]
}
```

`keys` is the answer to the only question that matters: how close is this key to its
limit right now. A report older than two minutes is not current usage, so it is left
out of the totals while still appearing under `reporters`, where its `current` says
`false`.

`?format=flat` gives one line per key as `keyId reads sheets reporters`, for
something that would rather not parse a tree.

## What these do not do

**Neither asks for a token.** They hold cached spreadsheet rows and usage counts,
not credentials, and the client posts to them constantly from templates that cannot
easily carry a secret. Put them somewhere only your own machines can reach. If they
must be public, put them behind HTTPS and whatever your host offers for access
control, and understand that anyone who reaches them can read your sheet data.

That is a deliberate difference from the relay, which does require a token, because
the relay accepts HTML that CasparCG will execute.

**Neither deletes anything on a schedule.** The cache keeps sheets until you remove
them; the client has a button for that under Settings → Sheets. The collector drops
a reporter that has been silent for a day.

**A limit worth knowing.** PHP discards a request body larger than `post_max_size`
rather than truncating it, and its default is 8 MB. Both files detect a body that
did not arrive whole and say so rather than storing nothing and reporting success.
