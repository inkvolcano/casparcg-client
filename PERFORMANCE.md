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

## Next

| # | Item | Where | Risk |
|---|------|-------|------|

## Open - later rounds

| # | Item | Where | Risk |
|---|------|-------|------|
| P4b | OGraf folder walk on every template change or filter press (only with OGraf on) | Library/LibraryWidget.cpp appendOgrafGraphics | low |
| P5 | Library vs server list comparison: nested loops, copies (the media loop already breaks on a match; re-measure templates/data/thumbnails) | Core/LibraryManager.cpp | low |
| P11 | NDI viewer: full-frame deep copy per frame, queued without limit, scaled on the GUI thread | Ndi/NdiReceiver.cpp, NdiViewerWidget.cpp | low-medium |
| P12 | Preview (non-legacy only): per-frame map + toImage, full-size still decode, folder listing per selection | PreviewWidget.cpp, PreviewContentWidget.cpp | low-medium |
| P14 | Opening a rundown pushes the whole XML through the system clipboard (and Windows clipboard history) | RundownTreeWidget.cpp open paths, pasteSelectedItems | low |
| P15 | Undo keeps full before/after rundown XML per step, serialised twice per structural edit | RundownTreeBaseWidget.cpp, RundownUndoCommands.h | low |
| P17 | A channel spin-box tick rebuilds every group widget in every tab, with a synchronous layout | RundownTreeBaseWidget::updateAllGroupWidgets | low |
| P18 | Each selection change walks the whole tree to build a set | RundownTreeWidget.cpp itemSelectionChanged | medium: guards a known dangling-pointer crash |
| P19 | Sheets panel rebuilds all rows on every poll, even unchanged and hidden | SheetsPanelWidget.cpp | low |
| P22 | Startup builds NDI (loads the DLL, starts discovery) and all 44 Inspector sections before the window shows | MainWindow.cpp, NdiPanelWidget.cpp, InspectorWidget.cpp | low (NDI) / medium (Inspector) |
| P23 | Missing-media check stats files on the GUI thread even for items already in the library | MissingMediaScanner.cpp | low, check verdictFor first |
| P24 | Activity panel: opacity effect per row, 25 s fade animations | ActivityPanelWidget.cpp | low |
| P25 | Simple Mode restyles every tally on every fire, visible or not | SimpleModeWidget.cpp | low |
| P26 | OSC subscription unsubscribe is linear per item on a shared list | Core/OscSubscriptionRegistry.cpp | low |
| P27 | Audio meter decay timer runs while hidden; sheet row cache never trimmed; OSC dispatch for unsubscribed paths | AudioMeterWidget.cpp, SheetDataResolver.cpp, OscMonitorListener.cpp | low |
