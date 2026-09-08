# The PHP side

Everything here is uploaded to a web host rather than built. None of it is required
to run the client, and each part solves a problem the client cannot solve alone.

PHP 7.4 or newer. Each keeps its data in a folder created beside it, so there is
nothing else to set up.

**New to this? [SETUP.md](SETUP.md) walks it through from nothing**, picks a route for
you, and says what you should see at each step.

| | |
|---|---|
| [`SETUP.md`](SETUP.md) | start here: a walkthrough for somebody who has not done this |
| [`relay/`](relay/README.md) | getting templates onto clients you cannot reach |
| [`sheets/`](sheets/README.md) | a shared Google Sheets cache, and where usage is totalled |

## Which one you want

**`relay/`** if you need to update templates on a playout machine at a venue you do
not control. The dev machine uploads packs to it and each client pulls what it does
not already have, so nothing inbound is opened at the venue. A private GitHub
repository does the same job with nothing to host, and
[`relay/GITHUB.md`](relay/GITHUB.md) covers that route instead.

**`sheets/`** if several machines read the same Google Sheets and you are running
into the API quota. The client can host a cache on its own port for one machine;
this is the version several machines share. `strain_collector.php` beside it is
where every client and connector reports what it is spending, because the quota is
spent per key rather than per spreadsheet and no one machine can see the total.

They are independent. Take one, both, or neither.

## The one difference that matters

**The relay requires a token. The sheets pair does not.**

That is deliberate on both counts, and it follows from what each holds.

The relay accepts HTML that CasparCG will execute. An open one is a way to run code
on a playout machine, so it takes two tokens, refuses an unset one, blocks an
address that keeps guessing, and has a self-test that tells you when it is exposed.

The sheets pair holds cached spreadsheet rows and usage counts. Templates post to
them constantly and cannot easily carry a secret, so asking for a token would mean
putting one in every template. Instead: **put them where only your own machines can
reach them.** If they must be public, use HTTPS and whatever access control your
host offers, and know that anyone who reaches them can read your sheet data.

## Deploying

Copy the folder you want. Two things people get wrong:

- **The relay's dotfiles travel with it.** `.user.ini` and `.htaccess` turn off PHP's
  display errors, and a PHP warning printed in front of a JSON answer breaks every
  client for a reason the symptom does not hint at. Some upload tools hide dotfiles.
- **Put the relay behind HTTPS.** Its tokens travel in a header. That is the one part
  the PHP cannot do for itself.

Then ask the relay how it feels about where you put it:

```bash
curl -H "X-Relay-Token: YOUR-UPLOAD-TOKEN" "https://example.com/relay/relay.php?action=selftest"
```

It checks the things that fail quietly: default or matching tokens, storage sitting
somewhere the web can read, plain HTTP, an upload limit smaller than it advertises.
`200` when it is happy, `500` when it is not, so it can go in a monitor.

## A limit worth knowing

PHP discards a request body larger than `post_max_size` rather than truncating it,
and the default is 8 MB. Every file here notices a body that did not arrive whole
and says so, rather than storing nothing and reporting success.
