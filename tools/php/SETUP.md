# Setting this up for the first time

A walkthrough, start to finish, for updating templates on machines you cannot reach.
Follow it in order. Every step ends with something you should see, so if a step
quietly did not work you find out there rather than five steps later.

Allow about half an hour the first time.

## What you are building

Right now, changing a template on a machine at a venue means getting on their
network. After this, you change it on your own machine and the venue picks it up by
itself, usually within fifteen minutes.

Nothing at the venue has to be opened up. The client there reaches out; nothing
reaches in.

## Five words this uses

| | |
|---|---|
| **pack** | one folder of templates, for example `SEVILLE`. The unit that gets moved |
| **source** | where packs live between your machine and the venues |
| **pull** | a client asking the source what is new, on its own, every few minutes |
| **token** | a long password that lets a machine talk to the source |
| **assignment** | which venue gets which packs |

## Which route

Two ways to do the same job. Pick one.

**Use GitHub if you can.** Nothing to host, nothing to maintain, and you get a
history of every template you have ever shipped and a way to undo a bad one. This is
the right answer for most people, and it is what the rest of this walkthrough uses.

**Use the relay if** you cannot rely on github.com being reachable from a venue, or
you would rather your templates did not sit on someone else's service. It does the
same job and needs a PHP host you control. [Jump to the relay walkthrough](#the-relay-route).

---

# The GitHub route

## Step 1: make the repository

On github.com, make a new repository. **Private.** A public one means anyone can read
your templates.

Put your packs in it, one folder each at the top level:

```
your-templates/
    SEVILLE/
        calendar.html
        css/site.css
    MARSEILLE/
        incidents.html
```

You can do that however you like: the GitHub web page, GitHub Desktop, or git.

> **You should see** your pack folders on the repository's front page, and the word
> **Private** next to its name.

## Step 2: make two tokens

Not passwords. GitHub stopped accepting those for this in 2021.

Go to: your avatar, top right → **Settings** → **Developer settings** (bottom of the
left column) → **Personal access tokens** → **Fine-grained tokens** → **Generate new
token**.

Make **two**, one at a time:

| Call it | Repository access | Permissions | Goes to |
|---|---|---|---|
| `templates-read` | Only select repositories → your templates repo | Contents: **Read-only** | every venue client |
| `templates-write` | the same | Contents: **Read and write** | your machine only |

Set an expiry you will actually notice. When it expires, updates stop.

**Copy each token when it is shown.** GitHub shows it once and never again.

> **You should see** two tokens listed, each starting `github_pat_`.

Why two: a venue machine only ever needs to read. If one goes missing, whoever has it
can read templates they were going to get anyway, and cannot change what every other
venue installs.

## Step 3: point one client at it

Do this on one machine first, not all of them.

On that machine, in CasparCG Client: **Edit → Settings → Templates**.

In **Pull Packs From A Relay Or GitHub**:

| Field | What to put |
|---|---|
| Check for new template packs | tick it |
| Source | `github:youraccount/your-templates` |
| Token | the **read-only** token |
| Check every | 15 min is fine |
| Packs | leave empty for now |

Press **Test**.

> **You should see** the repository name, its default branch, and the word
> **private**.
>
> If it says *no such repository, or this token cannot see it*, one of three things:
> the owner or repository name is misspelt, the token is the wrong one, or the token
> was not given access to this repository. GitHub answers the same way for all three,
> which is why the message names all three.

Now press **Check now**, and press **OK** to close Settings.

> **You should see** your packs appear in the client's templates folder. Not sure
> where that is? Settings → Templates → **Install into** shows it.

That machine is done for templates. It will keep itself up to date from now on.

### Reporting back

Still on the Templates tab, in **Report Back To A Relay**. This is what lets you
see every venue from one place: the push tool's **Identify** list and
**Assignments...** show nothing for a venue that does not report. A GitHub repository has
nowhere for a client to report to, so this goes to a relay - which can hold no
packs at all and just keep the logbook.

| Field | What to put |
|---|---|
| Report to | `https://your-host/path/relay.php` |
| Download token | the relay's `DOWNLOAD_TOKEN`, line 60 of `relay.php` |
| Report at least every | 720 is fine |

Press **Test**.

> **You should see** *Reached "template relay". Download token accepted.* and the
> name this machine will report as.
>
> If it says *that is the UPLOAD token* - swap it. A venue must never hold the
> upload token; that is the one that can write templates to every other venue.
> If it says *refused the token*, it is neither of the two: usually the GitHub
> token pasted in the wrong box.

Press **OK**. The machine reports on its next poll, and appears in the push tool
under **Identify** with its packs and its build number.

## Step 4: prove it works end to end

This is the step people skip, and it is the one that tells you the whole thing works.

1. On your own machine, change something visible in a template. Add a word to some
   text.
2. Commit and push it.
3. On the client, Settings → Templates → **Check now**.

> **You should see** the change in the file on that machine. If you do, everything
> from here is repetition.

## Step 5: the other machines

Repeat step 3 on each one. Same source, same read-only token.

If every venue should have every pack, you are finished.

## Step 6: give each venue only its own packs

Once there is more than one venue, you probably do not want all of them holding all
of it.

You can set this on each machine, in the **Packs** field: `SEVILLE, SHARED` and it
takes those two. But that means visiting a venue to change it, which is the thing you
were trying to avoid.

So set it centrally instead. Open **CasparCG Template Push** (it sits beside the
client), and under **Clients** add a row:

| Name | Host : Port | Token |
|---|---|---|
| templates | `github:youraccount/your-templates` | the **read and write** token |

Tick that row, then press **Assignments...**

> **You should see** a grid: machines down the side, packs across the top.

Tick which machine takes which pack. The `*` row is what a machine gets when it is
not listed, which is how a shared pack reaches everything without naming every box.

A machine that has never checked in will not be there yet. **Add machine...** and
type its name exactly as that computer reports it. On the client machine you can read
it from Settings → Templates, or in Windows under System → About → Device name.

Press **Save**.

> **You should see** *Saved.* If it says some packs are **not on the relay yet** or
> not in the repository, check the spelling. `SEVILE` is a perfectly good name and
> simply is not your pack, so it would deliver nothing to that venue and nothing
> would look wrong.

Each client picks up its assignment on its next poll and takes only what it should.

> **One machine needs to be different?** On that machine, tick **Ignore what this
> machine is assigned** in its own Settings and fill in its Packs field. It then
> ignores the grid.

## Step 7: how you send changes from now on

Either way works, whichever suits you:

- **Just use git.** Commit and push. Every venue picks it up on its next poll.
- **Or use the push tool**, if you would rather see what would change before it does.
  Point *Templates folder* at the folder holding your packs, tick what to send, press
  **Compare** to see what differs, then **Push ticked**.

---

# The relay route

Same walkthrough, with a host of your own instead of GitHub.

## Step 1: put the relay on a host

Copy **the whole `relay` folder** to a PHP host. PHP 7.4 or newer.

**Take the dotfiles with it.** `.user.ini` and `.htaccess` sit beside `relay.php` and
turn off PHP's error display. A PHP warning printed in front of an answer breaks
every client, for a reason nothing about the symptom would suggest. Many upload tools
hide dotfiles by default, so check.

**Put it behind HTTPS.** Its tokens travel in a header. That is the one part the PHP
cannot do for itself.

## Step 2: set two tokens

Open `relay.php` in a text editor. Near the top:

```php
define('UPLOAD_TOKEN',   'change-me-upload');
define('DOWNLOAD_TOKEN', 'change-me-download');
```

Change both to long random strings, different from each other. A relay left on the
shipped values is one anyone can write templates to, and a template is code your
server runs.

The upload token is yours. The download token goes to every venue.

## Step 3: ask it how it feels

```bash
curl -H "X-Relay-Token: YOUR-UPLOAD-TOKEN" "https://example.com/relay/relay.php?action=selftest"
```

> **You should see** `"ok": true`.
>
> If not, it tells you what is wrong in words. The two that matter most: **storage
> under the web root**, which on nginx or IIS means your templates are downloadable
> without a token, and **plain HTTP**, which means the token is readable in transit.

## Step 4: send it your packs

Open **CasparCG Template Push**, set *Templates folder* to the folder holding your
packs, and add a row under Clients:

| Name | Host : Port | Token |
|---|---|---|
| relay | `https://example.com/relay/relay.php` | the **upload** token |

Tick the row and your packs, press **Compare**, then **Push ticked**.

> **You should see** every file listed as *new*, then *sent*.

## Step 5 onwards

Identical to the GitHub route from step 3, with two changes: the **Source** on each
client is the relay's address, and the **Token** is the **download** token.

The relay can do one thing GitHub cannot: press **Identify** in the push tool and it
lists every venue that has checked in, when it last did, and which packs it is behind
on.

---

# When something is wrong

| What you see | What it usually means |
|---|---|
| *no such repository, or this token cannot see it* | name misspelt, wrong token, or the token was not given this repository |
| *wrong or missing token* | the client has the upload token, or the relay's download token was changed |
| *already up to date* but nothing arrived | the client is not assigned that pack. Check the grid, and the machine name in it |
| Nothing at all happens | the Check box is not ticked, or no template folder is configured on that machine |
| *the bytes do not match the digest* | something altered the file in transit. It refused to install it, which is correct |
| *this relay is not set up safely* | run the self-test; it names the problem |
| A pack you assigned never arrives | the pack name in the grid does not match the folder name. Case matters |

Two things that are working as intended and can look like faults:

- **Nothing is ever deleted on a client.** Removing a pack from the source stops it
  being updated; it does not remove it from machines that already have it.
- **`project.js` and `extensions.json` never travel.** Each machine owns its own API
  key and Sheets panel buttons, so those two files are never overwritten by an update.

# Where to read more

- [relay/README.md](relay/README.md) — every relay endpoint, in detail
- [relay/GITHUB.md](relay/GITHUB.md) — the GitHub route, including Enterprise Server
- [sheets/README.md](sheets/README.md) — the separate Google Sheets cache
