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

## Round 15 - build 252

| # | Item | Status |
|---|------|--------|
| P5 | Library vs server comparison: nested loops with a copy per inner step, for media (three passes), templates, data and thumbnails, on the GUI thread every refresh | **done 252** - measured at 10,000 clips: 14,289 ms as written, 4 ms with hash lookups, identical deletes, inserts and detail rows. All four comparisons now use QSet/QHash; the media detail pass keeps the first library row of a name, as its early break did |

## Round 16 - build 253

| # | Item | Status |
|---|------|--------|
| P11b | NDI viewer converted and scaled every full frame on the GUI thread | **done 253** - measured: QPixmap::fromImage(1080p) + scaled to a 480x270 tile, 4.68 ms per frame (3.88 fast mode); four viewers at 50 fps is ~940 ms of GUI time a second. The receiver now scales to the label size on its own thread (4.2 ms there) and the GUI converts the small image, 0.05 ms. A frame that does not fit the label (first frames, a resize in flight) is still scaled on the GUI as before |

## Round 17 - build 254

| # | Item | Status |
|---|------|--------|
| P31 | deviceAdded made every row of every type re-check its connection (resetting its time display) and, for media rows, rebuild its OSC subscriptions, for every server added | **done 254** - all 33 handlers return unless the added device is the row's own, compared by device object (by name lookup), which also removes a possible null dereference and tells apart two servers sharing an address |
| - | Found while checking P31: RundownOpacityWidget::deviceConnectionStateChanged connected the signal to itself again on every call, doubling the handlers on each connection change | **fixed 254** - same body as every other row type |

## Round 18 - build 255

| # | Item | Status |
|---|------|--------|
| P36 | Performance panel polled every 2 s (process snapshot, OpenProcess per server, restyles) for the whole session, placed or not | **done 255** - timer runs between showEvent and hideEvent; a reading is taken on show. updateStats has no side effects outside the panel (checked: it only reads quota figures) |
| P45 | Each connectDevice and each disconnect started its own 5 s single-shot chain | **done 255** - AmcpDevice and RrupDevice keep one single-shot QTimer and start it only when not already pending |
| P44 | GPI serial connect retried every 300 ms with an exception each time when no box is attached | **done 255** - retry delay doubles from 300 ms to 5 s while the port will not open, reset to 300 ms when a box connects |

## Round 19 - build 256

| # | Item | Status |
|---|------|--------|
| P28 | Every rundown item looked up its server's frame rate with two uncached queries (device, format) 2-4 times while loading, and every 200 ms while a still with a duration plays | **done 256** - DatabaseManager remembers devices and formats by name (81 and 54 callers). All six Device writers clear the device cache under the same lock; a new database and the change scripts clear both. tools/test-settingscache now writes through every device writer and checks the next read matches the table (16 -> 30) |

## Round 20 - build 257

| # | Item | Status |
|---|------|--------|
| P30 | A server host name that does not resolve was looked up again, blocking the GUI thread, on every call | **done 257** - CasparDevice keeps a failure for 30 s, then tries again; a success is still kept as before. RepositoryDevice not changed |
| P40 | Status bar started a 3 s single-shot per message and never cancelled the earlier ones, so a message could be cleared early by the previous one's timer; restyled on every message | **done 257** - one single-shot timer restarted by the newest message (stopped for a message with no timeout); the line's style set only when it changes |
| P42 | Autosave serialised each rundown twice per tick and rewrote an unchanged recovery copy | **done 257** - one serialisation for both the change check and the write; the write is skipped when the same content was written to the same file and that file still exists |

## Round 21 - build 258

| # | Item | Status |
|---|------|--------|
| P33 | Selecting a sheet-bound template rediscovered every project folder (reading each project.js twice) | **done 258** - the known projects answer first; discover runs only when the template's project is not known yet or Refresh forces a reload |
| P32 | Activity progress bars redrew on every clip frame during playback | **done 258** - measured 0.34 ms per redraw of a playing row at full opacity (the opacity effect is not the cost: Qt skips it at 1.0). The bar now counts in its own pixels, so it redraws when the fill moves, still animated between OSC updates. The permanent opacity effect was left as is |

## Round 22 - build 259

| # | Item | Status |
|---|------|--------|
| P29a | Per row on load: channel badge restyled for channel and again for layer, disabled style and font set to what they already were, device label restyled on every check; timecodeToSeconds compiled a regex per call | **done 259** - badge, disabled label and all 66 device-label restyles go through setStyleSheetIfChanged; the italic font is set only when it changes; the separator regex is static. Output identical; a leaf restyle measured 0.11 ms (P10b), three to four of them per row per open, paste and undo |

## Second audit (2026-09-15) - open

A second read-only sweep of areas the first did not cover. Each item is checked
against the code, and measured where it is a claim about time, before it is changed.

| # | Item | Where | Risk |
|---|------|-------|------|
| P29b | Loading a row: setters emit on unchanged values, so OSC subscriptions are rebuilt ~4 times per row | AbstractCommand.cpp setters, RundownMovieWidget/RundownTemplateWidget | medium: clone sync and change tracking listen to those signals |
| P34 | Every OSC batch reaches every movie row on the layer (fps on the whole channel); name compare allocates | RundownMovieWidget.cpp subscription slots | medium |
| P35 | Linked clones: each property set while loading or editing runs a full XML write/parse sync of the group | AbstractCommand.cpp setCloneGroupId, CloneGroupRegistry | medium |
| P38 | OSC receive thread builds a QVariant list, two formatted strings and a QMap insert per message | OscMonitorListener.cpp | low |
| P39 | Gateway rows rebuild buttons, scan all tabs and lay out the whole tree on every paste/drop and every 30 s | Rundown*GatewayWidget.cpp | low-medium |
| P41 | Thumbnails fetched on a fixed 2 s clock rather than when the last one arrives (1,000 clips = 33 min) | ThumbnailWorker.cpp | medium |

## Checked, not worth changing

| # | Item | Measurement |
|---|------|-------------|
| P20 | AMCP/RRUP line parsing | 40,000 lines: 41 ms old vs 20 ms offset-based. Qt 6 trims a string's front cheaply |
| P26 | OSC subscription registry "linear per item" | Already a hash of path to subscriber list; unsubscribe walks only the subscribers of one exact path |
| P27c | OSC dispatch for unsubscribed paths | 3,000 distinct paths per batch against 60 subscriptions: 0.88 ms per batch. Batches run every 200 ms (OscRefreshRate default), and messages are merged per path between batches, so about 4.4 ms a second at that load |
| P10b | Active-item flash setStyleSheet per frame | 0.11 ms per frame on a leaf label, ~2.4 ms per flash |
| P37 | SQLite commits on the GUI thread | WAL declined by the user (the database stays one file). The one-file alternative, skipping unchanged device writes, measured on a real SQLite file: an unchanged UPDATE commit 0.18 ms, the same as a guarded one that changes nothing - three per server per refresh |
| P43 | Sheet cache server reads its file per request | The real cache files are 1-80 KB (DREAMFORCE25, CNX24 sheets_data) and served from the OS file cache after the first read; a fraction of a millisecond per template request, and an in-memory copy would add a staleness rule |

## Next

| # | Item | Where | Risk |
|---|------|-------|------|

## Open - later rounds

| # | Item | Where | Risk |
|---|------|-------|------|
| P4b | OGraf folder walk on every template change or filter press (only with OGraf on) | Library/LibraryWidget.cpp appendOgrafGraphics | low |
| P12 | Preview (non-legacy only): per-frame map + toImage, full-size still decode, folder listing per selection | PreviewWidget.cpp, PreviewContentWidget.cpp | low-medium |
| P18 | Each selection change walks the whole tree to build a set | RundownTreeWidget.cpp itemSelectionChanged | medium: guards a known dangling-pointer crash |
