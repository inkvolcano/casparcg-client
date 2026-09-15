# Performance and memory

A working list from a read-only audit of the whole client (2026-09-15), worked
through in rounds. The goal: faster and snappier, no memory leaks or hogs, and
no feature changes. Each item is checked against the code before it is changed;
the audit's own claims are not taken on trust.

Status: **done** (build), **next**, **open**, **checked: not worth it**.

Already fixed before this list: template reads (QTextStream::readAll, 234), the
template scan gate and cache (230), reconnect guard and cached lookup (229).

## Round 1 - build 237

| # | Item | Status |
|---|------|--------|
| P8 | Settings read from the database on every call (210 call sites: firing, selection, every rundown row built) | **done 237** - answered from memory, kept in step by updateConfiguration, cleared by migrations. `tools/test-settingscache` runs the real manager on SQLite |
| P3 | Every library refresh rebuilt all Library trees 2 s later via an empty thumbnail pass | **done 237** |
| P6 | Every log line opened, appended and closed the log file, from several threads, unlocked | **done 237** - kept open behind a mutex, reopened on date change, flushed per line |
| P9 | Rundown rows loaded a thumbnail (query, decode, pixmap, base64 tooltip) into a label that is never shown | **done 237** |
| P7 | No indexes on Configuration / Library; thumbnail query did not join Library to Device | **done 237** - migration 260; query now matches the item's own server |
| - | Update download held the whole package in memory before hashing | **done 236** - streamed to a part file |

## Round 2 - build 238

| # | Item | Status |
|---|------|--------|
| P13 | Dialogs created with `new` on every open and never freed (Settings stayed connected to singletons; the URL dialog kept an app-wide event filter) | **done 238** - 16 sites freed on return via QScopedPointerDeleteLater, after every getter is read |
| P16 | HTTP items made a network manager per request; a second send before the first answered cancelled the second and logged the wrong URL | **done 238** - one manager per item, each reply logs its own URL |
| P21 | ThumbnailWorker connected its reply slot on every tick; a removed or shadow server's entry was retried every 2 s forever | **done 238** |
| P20 | AMCP/RRUP reply parsing removes each line from the front of the buffer | **checked: not worth it** - measured 40,000 lines at 41 ms old vs 20 ms offset-based; Qt 6 trims a string's front cheaply, so the predicted quadratic cost is not there |

## Round 3 - build 239

| # | Item | Status |
|---|------|--------|
| P2 | Every selection refilled the Inspector target list from a full library query, one name at a time | **done 239** - measured on 10,000 files / 2,500 movies: query 26-29 ms + addItem 31-65 ms per fill, up to 3 fills per click. Now a per-server, per-kind name cache (cleared by media/template events) added in one batch: 2.4 ms |
| P1 | One click sends the selection event up to three times | **measuring 239** - the log now records any selection whose listeners take 40 ms or more. With P2 removed, the remaining cost decides whether coalescing (medium risk) is worth it |

## Round 4 - build 240

| # | Item | Status |
|---|------|--------|
| P4 | Library lists filled one row at a time into the live tree, a new QIcon per row, a copy per model; sort comparator rebuilt keys and parsed timecodes per comparison | **done 240** - measured at 10,000 clips: fill 2,340-2,674 ms -> 53-56 ms (rows built detached, one addTopLevelItems, updates off, one icon per kind); sort by length 407 ms -> 26 ms with keys built once, same order verified. Media, templates and stored data. OGraf folder walk not yet changed |

## Round 5 - build 241

| # | Item | Status |
|---|------|--------|
| P10 | Row style sheets at build time and during the active flash | **done 241 (the part that measured)** - under the client's real application style sheet, restyling a row costs 2.35 ms; every row did it twice on open (default colour, then the saved colour, usually identical). All 49 row setColor calls now skip a sheet the row already has: about half the row-styling cost of opening a rundown. The active flash measured 0.11 ms per frame (~2.4 ms per flash) on its leaf label: **checked: not worth it** |

## Round 6 - build 242

| # | Item | Status |
|---|------|--------|
| P14 | Opening a rundown (file or URL) and inserting a preset went through the system clipboard | **done 242** - pasteXml takes the XML directly. Justified on correctness rather than time: the rundown landed in Windows clipboard history, a clipboard held by another program meant the wrong content was pasted, and a preset insert overwrote the operator's clipboard for good. The file is read with one readAll in text mode, so the change-detection hash is unchanged. Duplicate and Paste as Linked Clones still use the clipboard, by design |

## Round 7 - build 243

| # | Item | Status |
|---|------|--------|
| P15 | Undo kept every step's before and after rundown as UTF-16 text, 50 steps per tab | **done 243** - measured on the real rundowns: 1,017 KB held per copy for the largest (520 KB on disk), about 100 MB of undo for one tab; qCompress level 1 stores it in 14 KB (2.35 ms to compress, 1.04 ms to restore), about 1.4 MB for the same history. Round trip exact, tested (`tools/test-undosnapshot`). The double serialisation per edit is unchanged |

## Round 8 - build 244

| # | Item | Status |
|---|------|--------|
| P17 | A channel spin-box tick rebuilt every group summary in every open tab, each followed by a synchronous layout | **done 244** - a tab that is not visible marks itself stale and refreshes on show; the group channel badge sets font and style sheet only when they differ. Output identical. Not timed in the running client: the change removes work for hidden tabs outright and re-polishes only on a real change |

## Round 9 - build 245

| # | Item | Status |
|---|------|--------|
| P19 | Sheets panel cleared and rebuilt every row and row button on every poll, unchanged or not, resetting scroll and selection | **done 245** - a poll that returns the same headers and rows for the same project and tab is not drawn again. Polling itself is kept: it also warms the sheet cache templates and the Inspector read. Toggles, column visibility and action edits still redraw directly |

## Round 10 - build 246

| # | Item | Status |
|---|------|--------|
| P22a | The NDI panel loaded the NDI runtime and started a network discovery thread in its constructor, which the main window runs whether or not the layout places the panel | **done 246** - started on the panel's first showEvent. A layout without NDI never loads it; a layout with it starts it as the window appears, as before. Nothing outside the panel uses NdiManager (checked) |
| P22b | All 44 Inspector sections built eagerly at startup | **open** - medium risk, not started |

## Round 11 - build 247

| # | Item | Status |
|---|------|--------|
| P23 | Missing-media check stat'ed files on the GUI thread even for items the Library already has | **done 247** - MediaCheck::diskDecides: the disk is only asked when the Library does not have the item. test-mediacheck proves over every evidence combination and item type that, with the item in the Library, the verdict and its explanation do not depend on the disk. Up to 13 lookups per extension-less clip saved per item, per rundown open. Not timed on a network share (none here) |

## Round 12 - build 248

| # | Item | Status |
|---|------|--------|
| P27a | Audio meter fall timer (40 ms) ran for every meter while hidden | **done 248** - stopped in hideEvent, caught up and restarted in showEvent. The fall is computed from elapsed wall time (MeterBallistics::advanceTo), so a meter shown again lands where it would have been |
| P25 | Simple Mode restyled every key's tally on every fire | **done 248** - setStyleSheetIfChanged, so only the key that lit and the one that went dark are restyled |

## Round 13 - build 249

| # | Item | Status |
|---|------|--------|
| P11a | NDI receiver posted a deep-copied frame (~8 MB at 1080p) to the GUI per frame with no bound: a busy GUI thread let them pile up | **done 249** - an atomic pending flag; the receiver drops a frame (before copying it) while the last one has not been drawn. At most one frame waits per viewer. Scaling on the GUI thread is unchanged (P11b, open) |
| P27b | Sheet row cache never trimmed | **changed in 250, by decision** - the cache is meant to cover internet outages, so 249's ten-second trim was too short. Each tab's last copy is now kept for 24 hours (trimmed on the 10 s tick), and 250 serves it, marked cached with its real age, when every network read for the tab fails |

## Round 14 - build 251

| # | Item | Status |
|---|------|--------|
| P24 | Activity rows faded out over 25 s through a QGraphicsOpacityEffect at the display rate | **done 251** - measured 0.33 ms per row per frame (1.76 ms for six) under the client's style sheet, ~60 frames a second. The long fade is now a linear QTimeLine stepped every 200 ms with the same key values (0.3 at 80 %, 0 at the end): about twelve times fewer re-renders, steps under 0.02 opacity. The 300 ms fade-in on a loop restart stays a smooth animation |

## Checked, not worth changing

| # | Item | Measurement |
|---|------|-------------|
| P20 | AMCP/RRUP line parsing | 40,000 lines: 41 ms old vs 20 ms offset-based. Qt 6 trims a string's front cheaply |
| P26 | OSC subscription registry "linear per item" | Already a hash of path to subscriber list; unsubscribe walks only the subscribers of one exact path |
| P27c | OSC dispatch for unsubscribed paths | 3,000 distinct paths per batch against 60 subscriptions: 0.88 ms per batch. Batches run every 200 ms (OscRefreshRate default), and messages are merged per path between batches, so about 4.4 ms a second at that load |
| P10b | Active-item flash setStyleSheet per frame | 0.11 ms per frame on a leaf label, ~2.4 ms per flash |

## Next

| # | Item | Where | Risk |
|---|------|-------|------|

## Open - later rounds

| # | Item | Where | Risk |
|---|------|-------|------|
| P4b | OGraf folder walk on every template change or filter press (only with OGraf on) | Library/LibraryWidget.cpp appendOgrafGraphics | low |
| P5 | Library vs server list comparison: nested loops, copies (the media loop already breaks on a match; re-measure templates/data/thumbnails) | Core/LibraryManager.cpp | low |
| P11b | NDI viewer scales each full frame on the GUI thread | NdiViewerWidget.cpp | low-medium |
| P12 | Preview (non-legacy only): per-frame map + toImage, full-size still decode, folder listing per selection | PreviewWidget.cpp, PreviewContentWidget.cpp | low-medium |
| P18 | Each selection change walks the whole tree to build a set | RundownTreeWidget.cpp itemSelectionChanged | medium: guards a known dangling-pointer crash |
