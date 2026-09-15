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

## Round 2 - next

| # | Item | Where | Risk |
|---|------|-------|------|
| P1 | One click fires RundownItemSelectedEvent 2-3 times (itemSelectionChanged, itemClicked, currentItemChanged); ~49 listeners redo their work each time | RundownTreeWidget.cpp itemSelectionChanged / itemClicked / currentItemChanged | medium: multi-select refresh must survive |
| P2 | Every selection refills the Inspector target combo from a full library query | InspectorOutputWidget::fillTargetCombo | low |
| P13 | Dialogs created with `new` on every open and never freed; SettingsDialog stays connected to singletons, OpenRundownFromUrlDialog keeps a qApp event filter | MainWindow, RundownWidget, RundownTreeWidget, Inspector Http/Template, SettingsDialog | low |
| P16 | HTTP GET/POST items create a QNetworkAccessManager per request; a second send leaks the first | Web/HttpRequest.cpp | low |
| P21 | ThumbnailWorker connects the reply signal on every tick; NULL model retries forever | Core/ThumbnailWorker.cpp | low |
| P20 | AMCP reply parsing shifts the whole buffer once per line (quadratic on CLS/TLS) | Caspar/AmcpDevice.cpp | low |

## Open - later rounds

| # | Item | Where | Risk |
|---|------|-------|------|
| P4 | Library panel fill: per-row inserts, per-row QIcon, copying loop, sort comparator re-parsing timecodes, OGraf folder walk on every template change | Library/LibraryWidget.cpp | low |
| P5 | Library vs server list comparison: nested loops, copies (the media loop already breaks on a match; re-measure templates/data/thumbnails) | Core/LibraryManager.cpp | low |
| P10 | setStyleSheet per row at build time and ~20 times per active-flash animation | RundownWidgetHelper.h, Rundown*Widget, ActiveAnimation.cpp | medium |
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
