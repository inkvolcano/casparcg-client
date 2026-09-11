# Bughunt — all findings, by priority

**30 findings from eight passes on 2026-09-11, against build 219. The four P1s
are fixed in build 220. The other 26 are still open and unchanged.** Every entry
names the file and line it rests on and the shape of a fix, so each can be picked
up on its own.

Evidence came from the tree at build 219, the live `Database.s3db`, and the
client log of 2026-09-10 (2,901 lines, spanning the first start after the
migrations landed). The client was not started on the 11th, so nothing here is
from a run of 219 itself.

## Fixed in build 220

| | What was done | Checked by |
|---|---|---|
| **F28** | `ProcessPacket` overridden in both OSC listeners to catch `osc::Exception`, plus a `catch (...)` backstop in `OscThread::run()` | 6 cases in `src/Tests/OscPacketGuardTest.cpp` — one byte, empty, junk, both listeners, and that a valid message still dispatches |
| **F7** | `SimpleModeWidget::showEvent` now rebuilds every time instead of only when a structure change arrived while hidden; `applyRepositoryChanges` fires the structure event for the one path that deletes while the grid is visible | no automated cover — widget code the harnesses cannot reach |
| **F23** | `parseRundownXml()` guards all six parse sites; the paste's item loop is wrapped so a wrong-typed value cannot escape; `readProperties` returning null is skipped at all four call sites; `openRundown` reports the reason | no automated cover — same reason |
| **F16** | The 500-file cap is a batch, not a refusal: it installs the batch, marks packs it could not finish, and says what is left. Over 5000 files is still refused | `test-github`, 8 assertions across two polls, including that a half-installed pack stays out of the check-in |

**Verified by breaking it:** removing the batching made `test-github` fail four
assertions — the summary, the count installed, the check-in exclusion and the
second poll. F28's test cannot be break-verified here: without the guard the
runner is terminated rather than failed, and this machine does not build the Qt
test target.

**Two of the four have no test**, and that is worth saying plainly. F7 and F23
are widget code; the suites here are header-only harnesses that cannot open a
window. They were checked by reading and by type-check, which is weaker than the
rest of this list.

Full suite after the changes: 1583 assertions, 0 failed, network suites included.

## Priority

| | Meaning |
|---|---|
| **done** | Fixed in build 220. Kept here with what changed. |
| **P1** | Takes a machine down, or stops a new one being set up. Fix before the next venue. |
| **P2** | Lets someone do what they should not, or turns one slip into lost or leaked data. |
| **P3** | Wrong answers, fragility, or data problems that are latent today. |
| **P4** | Noise, growth, dev-machine-only exposure, cosmetics. Real and cheap. |
| **?** | Not a finding yet — a path that has never run for real. |

Marked **mine** where the fault is in code written during this fork's recent
sessions rather than inherited.

## Index

| Pri | ID | Finding | Area | Size of fix |
|---|---|---|---|---|
| done | F28 | One malformed UDP datagram terminates the client | OSC | fixed in 220 |
| done | F7 | Simple Mode keys point at items a drag has deleted | Simple Mode | fixed in 220 |
| done | F23 | Opening a malformed rundown file crashes the client | Rundown | fixed in 220 |
| done | F16 | A fresh two-pack venue can never complete its first pull | Template pull | fixed in 220 |
| P2 | F30 | The OSC WebSocket accepts connections from any web page | OSC | origin check |
| P2 | F20 | `/bypass` flips a persisted setting for anyone on the LAN | Local HTTP | loopback or token check |
| P2 | F26 | Settings Test saves the field, so Cancel does not cancel — **mine** | Settings | snapshot/revert |
| P2 | F18 | Declining autosave recovery deletes every autosave | Autosave | change No |
| P2 | F24 | A Library refresh commits its DELETE after failed INSERTs | Database | rollback ×3 |
| P3 | F21 | Two client instances silently drop each other's writes | Database | `QLockFile` |
| P3 | F1 | `Configuration` holds duplicate rows for 31 settings — partly **mine** | Database | migration + index |
| P3 | F22 | Venues with the same hostname are one venue to the relay | Check-in | client + relay |
| P3 | F17 | Missing-media marks are computed once, at open | Rundown | re-sweep hooks |
| P3 | F13 | The server-refusal banner never goes away — **mine** | Server Status | 3 lines |
| P3 | F8 | The relay's path rule is the containment and is untested | Relay (PHP) | new test suite |
| P3 | F4 | AMCP header parser indexes `tokens.at(1)` unguarded | AMCP | 1 line |
| P3 | F5 | `THUMBNAIL LIST` parsing splits and indexes blind | AMCP | 2 guards |
| P4 | F29 | The preview gives HTML templates the operator's files; wrong comment | Preview | comment + flag |
| P4 | F3 | Eleven getters read a row without checking; 1,833 warnings/day | Database | 11 one-liners + seed |
| P4 | F14 | `Logs/` grows forever — 409 MB since 2022 | Logging | startup prune |
| P4 | F15 | `listPacks()` with no template path lists the working directory | Installer | 1 line + test |
| P4 | F19 | Every install produces two check-ins | Check-in | digest change |
| P4 | F25 | The Http Log panel grows for the life of the process | Http Log | 1 line |
| P4 | F9 | The push tool shows and stores every token in clear | Push tool | mask column |
| P4 | F10 | Updater scripts install the *last* folder in the zip | Updater | count and refuse |
| P4 | F11 | Update staging is unwritable under Program Files | Updater | fallback path |
| P4 | F27 | A group can be moved into a shotbox inside itself (undoable) | Simple Mode | ancestor walk |
| P4 | F2 | `RundownWidget.ui` wires two slots that do not exist | Rundown | delete .ui block |
| ? | F6 | Install and Restart's quit-from-modal; two-instance wait | Updater | run it once |
| ? | F12 | `publish-build.ps1`'s refuse / `-Force` branch | Publishing | run it once |

---

## P1 — fix before the next venue

### F28. One malformed UDP datagram terminates the client

**FIXED in build 220.** Both listeners override `ProcessPacket` and catch `osc::Exception`; `OscThread::run()` has a `catch (...)` backstop. Covered by `OscPacketGuardTest`.

**OSC · crash · no user action needed**

Both OSC listeners bind every interface — `OscControlListener.cpp:36` and
`OscMonitorListener.cpp:36`, `IpEndpointName("0.0.0.0", port)` — on **3250**
(control) and **6250** (monitor), and both are on by default
(`Schema.sql:73-74`, `:80-81`). Neither overrides `ProcessPacket`, so oscpack's
runs (`lib/oscpack/osc/OscPacketListener.h:66-73`) and constructs
`ReceivedMessage` with no `try`. That constructor throws on malformed input —
49 `throw` sites in `OscReceivedElements.cpp`, the cheapest being *"message size
must be multiple of four"* (`:594`). Nothing above catches: the receive loop
calls `ProcessPacket` bare (`ip/win32/UdpSocket.cpp:456`), and
`OscThread::run()` (`Osc/OscThread.cpp:9-11`) is one line,
`multiplexer->Run()`. An exception escaping `QThread::run()` is
`std::terminate`. The `try` at `OscControlListener.cpp:29` covers socket setup
only, which is why it looks guarded.

**One datagram of any length not divisible by four — a single byte — to either
port ends the process.** A port scanner's UDP probe qualifies. So does a
truncated packet from the CasparCG server, because 6250 is where its own OSC
stream arrives: this is a robustness gap against the one peer the client is
built to trust, not only an attack surface.

**Fix.** Override `ProcessPacket` in both listeners:
`try { OscPacketListener::ProcessPacket(data, size, ep); } catch (const osc::Exception&) { /* count, drop */ }`.
Add `catch (...)` around `multiplexer->Run()` in `OscThread::run()` as a
backstop. Reproduce against a harness built from oscpack — not by sending the
byte to a running client, which kills it.

### F7. Simple Mode keys point at items a drag has deleted

**FIXED in build 220.** `showEvent` rebuilds unconditionally, and `applyRepositoryChanges` fires the structure event. No automated cover.

**Simple Mode · use-after-free · prep in the rundown, then go live**

Simple Mode and the rundown are never on screen together, so this is not "drag,
then click". It is the ordinary show workflow: arrange the rundown, switch to
Simple Mode, press a key.

The grid holds raw `QTreeWidgetItem*` in `cellItems` (`SimpleModeWidget.h:87`).
It is created once (`MainWindow.cpp:116`) and re-shown on every switch. The
switch itself only saves the setting and fires `rebuildLayout`
(`MainWindow.cpp:298-302`), which the grid does not listen to. `showEvent`
rebuilds only when `rebuildPending` is set, and that happens in one place — the
`rundownStructureChanged` handler (`SimpleModeWidget.cpp:829`). These
normal-mode actions delete items without firing it:

| Where | Action |
|---|---|
| `RundownTreeBaseWidget.cpp:2054`, `:2094` `dropMimeData` | **any drag** — a reorder, or into or out of a group |
| `RundownTreeBaseWidget.cpp:1199` `ungroupItems` | Ungroup |
| `RundownTreeWidget.cpp:2383` `absorbTransforms` | Absorb Transforms |

A drag looks like a move but **deletes the item and builds a new one from its
XML**. The key's position survives it — `simplemodeslot` is written
(`AbstractCommand.cpp:298-299`) and read back (`:272`) — so the grid looks right,
which is the intended behaviour. But the key still points at the deleted item.
Pressing it calls `setCurrentItem` and fires an execute event on freed memory:
a crash, or, if the allocator has reused that address, the key firing something
else.

One path can do this while Simple Mode is on screen: `repositoryChanged`
(`RundownTreeWidget.cpp:1536`) applies a newsroom repository's changes as soon
as they arrive, and `removeRepositoryItem` (`RundownTreeBaseWidget.cpp:2356`)
deletes retracted stories. That only matters where a repository connection is
set up.

**Struck:** Reload Rundown is safe. The grid listens for the reload event
(`SimpleModeWidget.cpp:145`), and the reload finishes by reopening the rundown.

**Fix.** Because the two modes are exclusive, the cheapest complete fix is one
line: rebuild in `showEvent` every time, not only when `rebuildPending`. Every
normal-mode edit is then picked up on the way into Simple Mode, and because slots
survive the rebuild, the keys come back exactly where they were — a reorder
leaves them alone, and a move into or out of a group changes group invokes and
shotbox rows, as intended. For the repository path, also subscribe the grid to
`ClearCurrentPlayingItemEvent(item)`, which every one of these sites already
fires just before its `delete`, and drop any cell whose item matches.

### F23. Opening a malformed rundown file crashes the client

**FIXED in build 220.** All six parse sites go through `parseRundownXml`, the paste loop is wrapped, null `readProperties` results are skipped, and `openRundown` reports the reason. No automated cover.

**Rundown · crash · one bad file**

`RundownTreeWidget::openRundown()` (`:1415`) reads the file, saves the
clipboard, writes the file's XML into it, calls `pasteSelectedItems()`, and
restores the clipboard (`:1433-:1444`). **Opening a rundown is a paste.** That
path runs `boost::property_tree::read_xml` (`RundownTreeBaseWidget.cpp:455`) and
`readProperties()` over **205** `pt.get<T>(L"key", default)` sites in
`Core/Commands/` — a default covers a *missing* key, not
`<simplemodeslot>abc</simplemodeslot>`. The only `try`/`catch` in the file is in
`pasteAsLinkedClones` (`:638-:698`), and `Main.cpp` has no top-level handler.
A file that is not well-formed, or has a wrong-typed value, throws
`ptree_error` out of the event loop. Rundowns here are hand-edited and copied
over flaky links.

Side effects of the same design: the clipboard restore never runs when it
throws, and every open briefly publishes the file to clipboard-watching software.

**Fix.** Catch `boost::property_tree::ptree_error` around the parse in
`pasteSelectedItems` and its siblings (`:394`, `:417`, `:2236`, `:2285`, and
`restoreFromSnapshot`'s `:100`), report "this file could not be read" with the
reason, and restore the clipboard in a scope guard. Longer term, open files
without going through the clipboard.

### F16. A fresh two-pack venue can never complete its first pull

**FIXED in build 220.** The cap batches instead of refusing, and a pack it could not finish is kept out of the check-in. Covered by `test-github`.

**Template pull · blocks setup**

`RelayClient.cpp:27` caps a poll at **500 files**. At `:722` (GitHub) and `:827`
(relay) a larger queue is refused outright — `done()` and return, nothing
installed — with *"the repository offered N files, which is more than one poll
will take."* The two packs total **769** (MARSEILLE 389, SEVILLE 380).

So a machine with an empty templates folder and `Packs` left blank — what
`SETUP.md:104` says to do, and `:118` says will make "your packs appear" — is
refused every fifteen minutes forever. It worked onsite only because the packs
were copied by hand first. **The advice given during setup, that the first poll
fills an empty machine, is wrong for any venue following both packs.** No doc
mentions the cap and no test reaches that branch; the largest test pack is
nine files.

**Until fixed:** seed new machines from `templates broadcast`, or set `Packs` to
a single pack.

**Fix.** The queue is deterministic, so take the first 500 and let the next
poll take the rest — two polls instead of never. Keep a refusal only for an
absurd single list (say 5,000). Add a 501-file test and a line in `GITHUB.md`.

---

## P2 — fix soon

### F30. The OSC WebSocket accepts connections from any web page

**OSC · on-air control from a browser tab**

`OscWebSocketListener.cpp:29` listens on `QHostAddress::Any:4250`, on by
default (`Schema.sql:75`, `:82`). `textMessageReceived` (`:64-90`) dispatches a
`{path, args}` object into `OscSubscriptionRegistry` — the registry holding the
rundown's `/control/<id>/play`, `/stop` and `/playnow` subscriptions
(`RundownAnchorWidget.cpp:487-499`). Nothing checks `origin()`, and
`QWebSocketServer` accepts every origin by default.

Unauthenticated control from the LAN is upstream's design. The escalation is
that browsers do not apply same-origin rules to WebSockets: **any web page open
in any browser that can reach 4250 can fire rundown items** — including a tab
on the playout machine itself, via `ws://127.0.0.1:4250`.

**Fix.** In `newConnection()`, close any socket with a non-empty `http(s)://`
origin — native OSC tools send none, pages always send one. Or bind localhost
unless a setting says otherwise.

### F20. `/bypass` flips a persisted setting for anyone on the LAN

**Local HTTP server · no authentication**

`SheetCacheServer.cpp:174` binds `QHostAddress::Any`. `GET /bypass?on=1`
(`:350-:364`) needs no token, calls `setBypassing()`, which **writes the change
to the database** (`:131`) so it survives restarts, and replies with the cache
directory path. One URL from the guest Wi-Fi turns a venue's sheet cache off for
good. Only `/templates*` checks a token (`:384-:390`).

`/strain` (`:294`) is open too but writes nothing to disk: POSTs and `?tick`
(up to 1,000 per request, `:335`) feed in-memory counters, so a LAN peer can
lie to the strain meters but not fill the drive. It must stay reachable from
localhost, since templates on the same machine report through it.

**Fix.** Refuse `/bypass` and `/strain` unless the peer is loopback, or require
`X-Template-Token` — the check twelve lines down already handles an empty token
correctly.

### F26. Settings Test saves the field, so Cancel does not cancel — mine

**Settings · stores the wrong token**

**Test** and **Check now** call `applyRelayFields()` (`SettingsDialog.cpp:905`,
`:911`); the check-in **Test** added in 219 calls `applyCheckInFields()`
(`:1008`). All three write the typed values to the database before sending.
There is no `reject()` override and nothing snapshots the old values.

The check-in Test invites the worst case: paste a token, press Test, read
*"that is the UPLOAD token — swap it"*, press Cancel — and the venue now holds
the upload token, persisted, after a dialog that said nothing changed.

**Fix.** Snapshot on open and write back in `reject()`; or send the test from
the fields without writing, and save only on OK.

### F18. Declining autosave recovery deletes every autosave

**Autosave · one click, no second chance**

`MainWindow::offerAutoSaveRecovery()` (`:194`) asks once, Yes/No, for *all*
pending autosaves. **No** calls `RundownWidget::clearAutoSaves()` and returns
(`:229-:230`). After a crash with three rundowns open, one reflexive No destroys
all three recovery files, and the dialog does not say it will.

**Fix.** No should leave the files alone — `clearAutoSaves()` already runs on a
clean save — or offer per rundown, or at least say "No deletes them".

### F24. A Library refresh commits its DELETE after failed INSERTs

**Database · empty instead of stale**

`DatabaseManager::updateLibraryMedia()` (`:1405`): `transaction()`, `DELETE`
for the device, INSERTs that `qCritical` and **continue** on failure, then
`commit()` unconditionally (`:1455`). No `rollback()` on the path. Any condition
that fails the inserts leaves the table empty rather than stale — which is
exactly how the migration bug showed on 2026-09-10: 215 rows deleted, 215
inserts failed, commit. A stale Library plays; an empty one does not.

**Fix.** Count failures; on any, `rollback()` and log one line saying the
refresh was refused and why. Same shape in `updateLibraryTemplate` (`:1473`) and
the data variant (`:1525`).

---

## P3 — worth fixing

### F21. Two client instances silently drop each other's writes

**Database · lost settings**

Nothing in `Main.cpp` stops a second `casparcg-client.exe` — no `QLockFile`,
`QSharedMemory` or `QLocalServer` — and both open the same
`~/.CasparCG/Client/Database.s3db` with no `busy_timeout` and the rollback
journal. A write that meets the other instance's lock gets `database is
locked`, which `updateConfiguration` logs with `qCritical` and **continues**
(`:14`, `:24`): the setting the operator pressed OK on is not saved. A preview
instance, or a double-click on a slow machine, is enough.

**Fix.** A `QLockFile` beside the database — "already running", exit. If two
instances are ever wanted: `busy_timeout`, WAL, separate database paths.

### F1. `Configuration` holds duplicate rows for 31 settings — partly mine

**Database · latent**

Two rows each (three for `HeaderLineMaster`) for the relay, check-in, push,
update and OSC settings and all sixteen `PanelSizeMode_*`.
`Configuration.Name` has no UNIQUE constraint (`Schema.sql:2`), and
`updateConfiguration()` INSERTs when its UPDATE touches nothing — so Settings
created rows for keys whose migrations had not run (until 216, none since 229
had). The migrations then added a second. `INSERT OR IGNORE` — used by 20 of 22
seeding scripts — ignores nothing without a unique constraint; 257 and 258 (mine)
omit even that.

Latent because reads take the oldest row and writes hit all of them, so they
agree. It breaks the day anything iterates, counts, or deletes by name.

**Fix.** A migration keeping the lowest `Id` per `Name`, then
`CREATE UNIQUE INDEX ON Configuration(Name)`. Dry-run it against a copy of a
real database first, as the 216 batch was.

### F22. Venues with the same hostname are one venue to the relay

**Check-in · wrong estate view**

`clientId()` in `relay.php` is the sanitised hostname and nothing else; the
client sends `QSysInfo::machineHostName()` (`RelayClient.cpp:998`). Two machines
from one image — both `GFX-04` until someone renames one — overwrite one
`.clients/<host>.json` every poll. The estate view shows one machine flickering
between two, and "behind on SEVILLE" for one is hidden while the other reports
current.

**Fix.** Send `QSysInfo::machineUniqueId()` too and key the relay on host plus
a short id; the estate view can then say "two machines report as GFX-04".

### F17. Missing-media marks are computed once, at open

**Rundown · stale warning**

`checkMissingMedia()` has one caller, `openRundown()`
(`RundownTreeWidget.cpp:1460`). Nothing re-sweeps on a Library refresh, a device
connecting, or a pull landing — so *missing* survives the file arriving, and a
file that disappears after open is never marked.

**Fix.** Re-run on `refreshLibrary` and after a poll that installed anything.
The scanner reads the Library once per sweep, so it is cheap.

### F13. The server-refusal banner never goes away — mine, build 218

**Server Status · a warning that outlives its cause**

`labelServerFailure` is shown by `commandFailed()`
(`ServerStatusPanelWidget.cpp:1032`) and never hidden. After the `_media` fix
and a successful refresh, the panel still says the server cannot scan its media
until the client restarts — and the next real warning gets ignored.

**Fix.** Hide it in the `mediaChanged` slot the panel already connects
(`:618`), which only fires on a successful `200 CLS OK`. Three lines.

### F8. The relay's path rule is the containment, and it is untested

**Relay (PHP) · test gap on a security boundary**

Upload writes `packDir($pack) . '/' . $path` (`relay.php:813`) after
`safeRelativePath()` passes; it cannot `realpath` a file that does not exist
yet, so the pattern rule is the only containment. Download and delete use
`resolveInPack()` with `realpath`, correctly. The C++ twin has `test-paths`; the
PHP copy — which must stay identical, Windows device names included — has no
hostile-input test at all.

**Fix.** A `test-relay-paths` posting `../x`, `%2e%2e/x`, `con.html`, `C:/x`,
`\\server\x` and a dot-prefixed pack, asserting 400 and no file anywhere under
`STORAGE` — the same case table as `test-paths`, so drift shows as one side
failing.

### F4. AMCP header parser indexes `tokens.at(1)` unguarded

**AMCP · crash on a malformed server reply**

`AmcpDevice.cpp:200` calls `translateCommand(tokens.at(1))` for codes 200, 201
and 400. A bare `200` — a truncated read on a dropping connection — has one
token, and `.at(1)` is undefined behaviour. The `default:` branch added in 218
guards; this older path does not.

**Fix.** `tokens.count() > 1 ? translateCommand(tokens.at(1)) : NONE`.

### F5. `THUMBNAIL LIST` parsing splits and indexes blind

**AMCP · crash on a malformed server reply**

`CasparDevice.cpp:795-796` takes `split("\" ").at(1)` and then `.split(" ").at(1)`
with no count checks — the shape the old `CLS` parser had until 204 replaced it
with `MediaListing::parseLine()`. `TLS` (`:724`) and `DATA LIST` (`:763`) use
only `.at(0)`, which is safe.

**Fix.** Route thumbnails through `MediaListing`, or guard both. The
`test-medialisting` suite would then cover it.

---

## P4 — tidy

### F29. The preview gives HTML templates the operator's files, and the comment says otherwise

`Main.cpp:316-317` sets `--allow-file-access-from-files` for the process, and
the preview adds `LocalContentCanAccessFileUrls` (`PreviewWidget.cpp:650`). The
justifying comment says the only local page loaded is one the client writes.
True for OGraf (`:699` loads the client's `.casparcg-ograf-preview.html`,
`:574`) — but **`:351` loads the operator's HTML template directly**, so a
previewed template can read the client database with its tokens, and every
`project.js`. Low because templates are already trusted as code; the comment is
what a future reader will believe.

The flag is also skipped entirely if `QTWEBENGINE_CHROMIUM_FLAGS` is already
set, and then every OGraf graphic fails with a bare CORS error.

**Fix.** Correct the comment; append to an existing value rather than skip it.
To narrow it, load plain HTML through a client-written host page as OGraf
already does.

### F3. Eleven getters read a row without checking it exists

`getConfigurationByName`, `getDeviceByName/ById/ByAddress`, `getFormat`,
`getOscOutputByName/ByAddress`, `getPreset`, `getThumbnailByNameAndDeviceName`,
`getTypeByValue` and `upgradeDatabase` in `DatabaseManager.cpp` do
`sql.first();` then `sql.value(…)` unchecked — **1,833** *"not positioned on a
valid record"* warnings on 2026-09-10, 621 after the schema was fixed. Eighteen
keys the code reads are seeded by nothing, among them `LibrarySortBy`, so the
204 sort choice never persisted. Behaviour is right; the noise buries real
warnings and is most of F14's volume.

**Fix.** `if (!sql.first()) return <empty model>;` in each, and a migration
seeding the eighteen keys with their code defaults.

### F14. `Logs/` grows forever

94 files, **409 MB**, one a day back to 2022-04-14, and nothing prunes. At
~4 MB/day a small venue drive fills in a few years, and then the database cannot
be written either. **Fix.** Delete logs older than 30 days at startup; F3 cuts
the volume by most of it.

### F15. `listPacks()` with no template path lists the working directory

`TemplateInstaller.cpp:153` builds `QDir(root)` without the empty check its
siblings have. On a fresh install `root` is empty, `QDir("")` is the working
directory, and `identify()` (`:358`) — what the push tool's **Identify** shows
as *N pack(s)* — counts the folders beside the exe. The same bug class fixed in
the push tool at 197. **Fix.** `if (root.isEmpty()) return result;` and a
`test-install` case.

### F19. Every install produces two check-ins

`installed` resets each poll (`RelayClient.cpp:362`) and is in the digested body
(`:1024`), so an installing poll reports once and the next idle poll reports
again. **Fix.** Keep `installed`/`failed` out of the digest, or report
cumulative state.

### F25. The Http Log panel grows for the life of the process

Entries land in `HttpResponsePanelWidget`'s `QPlainTextEdit`, which has a manual
`clear()` (`:63`, `:112`) and no cap. **Fix.** `setMaximumBlockCount(2000)`.

### F9. The push tool shows and stores every token in clear

`PushWindow.cpp:380-426` keeps targets in `QSettings("CasparCG", "TemplatePush")`
— the registry, plaintext — and the Token column shows them unmasked, including
the relay upload token and any read/write PAT. Dev machine only; but a
screenshot of that window is a credential leak, and one was taken. **Fix.**
Mask the column with reveal-on-double-click.

### F10. Updater scripts install the last folder in the zip

`install-update.cmd` and the generated `apply-update.cmd` both take the *last*
top-level folder (`for /d … set "SOURCE=…"`). Fine for every release
`publish-build.ps1` makes; a hand-made zip with two folders installs the
alphabetically last silently. **Fix.** Count them; refuse unless exactly one.

### F11. Update staging is unwritable under Program Files

`UpdateDialog::stagingFolder()` is `<app dir>\updates`. Venues here use
`C:\CasparCG`, which is fine; under `Program Files` the verified download cannot
be written. **Fix.** Fall back to `%LOCALAPPDATA%`, or say so in `UPDATES.md`.

### F27. A group can be moved into a shotbox inside itself

`moveCurrentItemInto()` (`RundownTreeBaseWidget.cpp:1488`) guards the source
being the target (`:1494`) and already in it (`:1512`), not the target being
its descendant. Both vanish; one Ctrl+Z restores them. **Fix.** Walk the
target's parents; refuse if one is the source.

### F2. `RundownWidget.ui` wires two slots that do not exist

`RundownWidget.ui:42-75` connects `tabCloseRequested(int)` and
`currentChanged(int)` to form slots `RundownWidget.h` never declares — six
*"No such slot"* warnings per start. Both are handled by lambdas
(`RundownWidget.cpp:270`, `:296`), so nothing is lost. **Fix.** Delete the
`.ui` connections and slot declarations.

---

## ? — never run for real

### F6. The updater's two unobserved paths

- `UpdateDialog::installAndRestart()` calls `QCoreApplication::quit()` from
  inside a modal `exec()`. Qt should unwind it; nobody has seen it do so. The
  first real Install and Restart settles it.
- Both updater scripts wait for *any* `casparcg-client.exe`. A machine running
  two instances waits for both, then refuses after a minute — correct, and
  worth knowing where there is a preview instance (see F21).

### F12. `publish-build.ps1`'s refuse and `-Force` branch

Every real publish so far was a first publish. The "tag already published →
refuse; `-Force` → replace assets" branch has never run. One deliberate run
against a throwaway tag settles it.

---

## Checked and clean

Looked at on purpose and found sound, so they need not be looked at again:

- **Outbound TLS verifies** — no `ignoreSslErrors`, `VerifyNone` or
  `setPeerVerifyMode` anywhere in `src/`.
- **OSC argument typing** checks every `Is*()` before `As*()`; **WebSocket
  input** parses safely — bad JSON gives an empty path and no dispatch.
- **Shell Command items** — `QProcess::splitCommand`, no shell, gate read at
  fire time (`RundownShellCommandWidget.cpp:308`), off by default.
- **Local `/templates*` routes** compare the token before reading the path; an
  empty token matches nothing.
- **The relay's `assignments` POST** — names must equal their own `clientId()`
  form, packs must pass `safeSegment()`, the file is rebuilt from a clean array.
  No body cap before `json_decode`, but `post_max_size` bounds it.
- **No self-deadlock** — `deleteThumbnails` re-enters the mutex, but it is a
  `QRecursiveMutex` (`DatabaseManager.h:115`).
- **Dropdown groups** guard an empty group before `child()`
  (`RundownTreeWidget.cpp:2878`).
- **Undo** is bounded at 50 steps by default (`RundownTreeBaseWidget.cpp:66`);
  each step is a whole-rundown snapshot.
- **The preview's audio decoder** is stopped before each new item
  (`PreviewWidget.cpp:294`) and keeps peaks, not buffers.
- **`RelayClient::start()`** stops the timer before restarting it, so saving
  Settings does not stack polls.
- **`installFile`, `packDigests`, `describePack`** all guard an empty root.
- **The push tool's `sources.first()`** (`PushWindow.cpp:1406`) is guarded by an
  `isEmpty()` check above it — raised in pass 1 and struck.
- **The 216 migration batch** did its job: the live database is at 258, the
  Library has `Size`/`Timestamp`, `LayoutPreset` exists, 215 rows survived.
