# Updating the client itself

Templates got a distribution route years before the thing that plays them did. A
venue ran whatever was copied onto it, the title bar said `build 206` to nobody in
particular, and finding out what an estate was running meant visiting every
machine.

This is the other half. It is deliberately smaller than the template route, and
the difference is the point: **nothing here happens on its own.**

| | Templates | The client |
|---|---|---|
| Checks | every 15 minutes | when somebody presses Check |
| Installs | itself | never |
| Deletes | never | never |

A template is a file the server reads. The client is the program running the show,
on a machine that may be on air, and Windows will not overwrite a running
executable anyway. So the client will fetch and verify a new build and then stop,
and the person standing in front of the machine chooses the moment.

## Publishing a build

`tools/publish-build.ps1`, after your normal build:

```
powershell -ExecutionPolicy Bypass -File tools\publish-build.ps1 -Repo owner/repo
```

It reads the version and build id **out of the tree that was compiled**, not out
of your checkout, so a tag can never claim a build that does not match the binary.
It refuses if `windeployqt` has not run, because a zip without Qt beside the exe
is a binary that starts on no venue machine.

It produces two assets:

- `casparcg-client-v2.3.1-206-windows.zip` — one folder inside, named for the
  build, so unzipping never scatters four hundred DLLs into somebody's Downloads.
- `SHA256SUMS.txt` — what makes the download verifiable. The releases API
  publishes a size and nothing else worth checking, and two builds of this client
  are the same size often enough that a size check would pass one for the other.

**It will not overwrite a published tag** unless you pass `-Force`. A venue may
already be running what is under it. Bump `DEV_BUILD_ID` instead — that is what
the tag is made from.

Use `-WhatIfPublish` to build the zip and the checksum and stop, if you want to
see what would go out.

## Pointing a client at it

Nothing to set. The source is **fixed in the build** and shown, read-only, in
Settings; **Help → Check for Updates** works out of the box.

That is deliberate rather than lazy. A template is a file the server reads; a
build is a program that runs on the playout machine with the operator's
privileges. A text field would let anyone who can open Settings point a venue at
any repository at all, and **the checksum would not catch it** — it verifies the
download against the sums file published beside it, which proves the bytes arrived
intact and says nothing whatever about who built them. Provenance comes from the
list being fixed, not from the file being hashed.

The list is `ClientRelease::allowedSources()`. Adding to it is a one-line change
and a rebuild, which is the point: it takes commit access rather than the Settings
dialog. A stored source that is not on the list is **ignored rather than obeyed**,
and the dialog says so instead of falling back in silence.

The **Update token** is only needed if a listed repository is private; a public one
needs nothing.

## What the client will and will not do

**Will**: ask, compare, download, and verify against the release's own checksums
file. A package that does not match is thrown away without being written, so there
is never a file sitting in the folder that somebody could install by hand later.

**Will not**: check on its own, check at startup, download without being asked, or
install. The verified zip is left in an `updates` folder beside the application and
the folder is opened. Closing the client and unpacking it is a person's job.

## Which build is each venue on

The check-in now carries it. A client that reports to a relay says which build it
runs, and the push tool's estate view shows it:

```
SEVILLE                b205   3m ago      current  (updated 12d ago)
MARSEILLE              b198   2h ago      behind on SEVILLE
NANTES                 -      6d ago      current
```

`-` is a client too old to report it. An estate running four different builds is
worth seeing before somebody spends an afternoon reproducing a bug that was fixed
in one of them.

This works whatever the client pulls templates from, because reporting and pulling
were separated already — see [GITHUB.md](php/relay/GITHUB.md).

## Version numbers

The tag is `v<major>.<minor>.<revision>-<build>`, and the **build is the part that
matters**. Upstream's semantic version has moved three times in as many years while
this fork is on its two hundredth build, so "is there anything newer" is almost
always a question about the trailing number.

These are all understood, so a tag written by hand still works:

```
v2.3.1          upstream's shape, no build
2.3.1
v2.3.1-206      what publish-build.ps1 writes
v2.3.1-build206
build-206       a fork release that moved nothing else
2.3.1 build 206 what the title bar says
```

A tag that cannot be read is treated as **unknown rather than as version zero** —
it is never offered as an update, because offering one on the strength of a tag
nobody understood is how an estate installs something nobody meant to publish.
