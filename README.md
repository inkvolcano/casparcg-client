# CasparCG Client

![platforms](https://img.shields.io/badge/platforms-windows%20%7C%20linux%20%7C%20osx-brightgreen.svg?style=flat-square)
[![help](https://img.shields.io/badge/help-community%20forum-green.svg?style=flat-square)](https://casparcg.com/forum)
[![license](https://img.shields.io/badge/license-GPLv3-blue.svg?style=flat-square)](LICENSE)

<p align="center"><img src="/src/Widgets/Images/Clients.png"></p>

## What this fork adds

A fork of the CasparCG Client, kept close to upstream, with three things built on
top. Everything below is optional: none of it is on until it is switched on, and
the client works exactly as it always did without any of it.

### Getting templates onto machines you cannot reach

Updating templates on a playout machine at someone else's venue normally means
getting on their network. Two ways here that do not.

**A relay you host.** One PHP file on any host. A dev machine uploads packs to it,
each client polls and pulls what it does not already have. Nothing inbound is opened
at the venue. See [tools/relay/README.md](tools/relay/README.md).

**A private GitHub repository.** Same idea with nothing to host, and you get history
and rollback for nothing. Push your templates and clients pick them up on their next
poll. GitHub Enterprise works too. See [tools/relay/GITHUB.md](tools/relay/GITHUB.md).

Both make outbound connections only, so no firewall has to change. A **CasparCG
Template Push** tool sits beside the client for the dev end: it compares a pack
against a client, a relay or a repository and shows what would change before
anything is sent.

Some rules the whole thing keeps, whichever route is used:

- **Nothing is ever deleted on a client.** A file removed at the far end stays on
  every machine that already has it.
- **`project.js` and `extensions.json` never travel.** Each client owns its own API
  key, `local` flag and Sheets panel buttons.
- **Bytes are checked in both directions** against a digest, so a template that
  changed in transit is refused rather than written.
- **A path can never leave its pack**, Windows device names included.

### Google Sheets without spending the quota twice

A Sheets API key has a quota per minute, spent per key rather than per spreadsheet.
Twenty templates reading one sheet spend twenty reads on one answer.

The client can host a cache on its own port that templates read from instead, and
`tools/php/` has the same thing for a host several machines can share, plus a
collector where every client and connector reports what it is spending so one place
knows the real total. See [tools/php/README.md](tools/php/README.md).

### Rundown and panel work

Nested groups, a Simple Mode grid for stream-deck style operation, per-panel
layout with spanning and resizable columns, an NDI monitoring panel, trigger banks,
and a Sheets-bound template inspector. [CHANGES.md](CHANGES.md) has the detail.

### Working on it

```
python tools/check-all.py
```

Type-checks what changed, verifies every source is in the build, and runs the test
suites. A few minutes; `--fast` is under a minute. See
[tools/README.md](tools/README.md).

## Installation

#### Windows
No installation required. Unpack the zip file to a location on your drive and start it. Tested on Windows 10 x64.

#### macOS
No installation required. Open the DMG file and drag the 'CasparCG Client' app to a location on your drive and start it. Tested on macOS 11.

#### Linux
Install the deb file and launch 'CasparCG Client'. Tested on Ubuntu 22.04 64-bit, other distributions may require building from source.

## Development

#### Windows
* Install Qt 6.5 for Windows from [Qt archive](https://www.qt.io/download). You may wish to select a more minimal installation than the full 6.5 tree. At a minimum the additional library *Qt WebSockets* and the *Qt 5 Comaptibility Module* are required.
* Install [Visual Studio Community 2022](https://visualstudio.microsoft.com/vs/community/)
* Install [CMake](https://cmake.org/download/)

* Run cmake with the argument `Qt6_ROOT` with a value of `c:\Qt\6.5.3\msvc2019_64` pointing to your qt installation.
* Open the Visual Studio project file

#### macOS
* Install Qt 6.5 for macOS from [Qt archive](https://www.qt.io/download). You may wish to select a more minimal installation than the full 6.5 tree.
* Download and install Xcode from the App Store.

#### Linux
* Install Qt6, libvlc and boost from your system package manager

## Releases
Complete history of all releases and the changes can be found in the [CHANGELOG](CHANGELOG).

## License
CasparCG Client is distributed under the GNU General Public License GPLv3 or higher, see [LICENSE](LICENSE) for details.
