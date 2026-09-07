# CasparCG Client — Changelog

All changes since forking the original CasparCG Client.

---

## Nestable Groups

Groups can now contain other groups, up to 2 levels deep. An outer group can hold inner groups, but inner groups cannot nest further.

### Creating Inner Groups
- Select items inside a group and use Group (right-click or shortcut) to wrap them in an inner group
- Drag a top-level group into another group to make it an inner group
- Copy/paste groups into other groups

### Moving Items
- Move-into-group works for both items and groups (with depth validation)
- Move-out-of-group promotes items one level: depth 2 items move to the outer group, depth 1 items move to top level
- Ungroup dissolves the selected group, promoting children to the parent level

### Autoplay with Inner Groups
- **Expanded inner group**: Children are flattened into the parent group's autoplay queue — they play sequentially as if they were direct children
- **Collapsed inner group**: All children execute at once as a single batch, then the outer queue advances to the next item
- Video-to-collapsed-group transitions defer to end-of-clip for clean handoffs
- Loop rebuild handles inner groups the same way as initial queue building

### Depth Enforcement
- Groups are allowed at depth 0 (top level) or depth 1 (inside a group)
- Non-group items are allowed at depth 0, 1, or 2
- All operations (group, ungroup, move, paste, drag-drop) enforce these limits
- Context menu disables "Group" when nesting would exceed the maximum depth

---

## Visual Overhaul

### Channel Badges
Every rundown item now shows a **36px channel badge** with a unique color per channel, replacing the old plain color strip. Colors use golden-angle hue rotation with a 120° offset for maximum visual separation: Ch1 blue, Ch2 orange, Ch3 cyan, Ch4 purple. The formula is centralized so badges, active animations, and activity entries all use identical colors.

### Active Animation
When an item is triggered, it flashes from white to the channel-specific color instead of the old red-to-green flash.

### Color Palette
The entire color system has been replaced with 30+ semantic RGBA colors organized by hue. Old named CSS colors like "Sienna" and "OliveDrab" are gone — everything uses translucent RGBA values with consistent opacity levels.

### Menu Margins
All menus (top menubar dropdowns, right-click context menus, and hamburger/panel menus) now have consistent left padding. Previously only the hamburger menus had left margins.

### Duration Labels
All item types now show human-readable duration labels:
- Movies and Audio: "1m23s" format instead of timecode
- Stills and Templates: duration with stopwatch icon
- Aware of delay/duration unit (frames vs milliseconds) and actual channel FPS

### Delay Display
Delay indicators now use an hourglass icon and show human-readable values ("1s" instead of "1000").

### Inspector Unit Toggles
The delay and duration fields in the inspector each have a toggle button ("ms" / "fr") to switch between milliseconds and frames independently. Clicking the button converts the value using the channel's FPS. Items without a CasparCG channel (Group, GPI, OSC, HTTP, Playout Command) are locked to milliseconds.

### Duration Format Setting
A "Duration format" setting in Settings > General lets you choose between:
- **Human Readable** (default): "1m23s" style
- **Timecode (HH:MM:SS)**: legacy "00:01:23" style

Changing the format updates all rundown item labels immediately.

---

## AutoPlay System

### Top-Level Chaining
Items can chain automatically between groups using autoplay. Movies preload via LOADBG AUTO, stills chain when their duration expires. Video-to-still transitions use deferred routing to ensure clean handoffs.

### AutoPlay on Stills
Stills now have an AutoPlay toggle in the inspector, allowing them to participate in autoplay chains.

### Group Loop & AutoPlay
Groups have loop and autoplay properties with visual indicators. Loop causes the group to restart after the last item finishes; autoplay chains to the next group.

### Live Loop Toggle
Toggling loop on a playing movie sends a live CALL LOOP command to CasparCG — no need to stop and restart.

### Per-Item Auto-Loop with Countdown
Movies, stills, templates, and groups now have a per-item **Auto-Loop** toggle. When enabled, the item fires, then re-fires after a configurable delay (in seconds), and continues looping until the toggle is turned off. Auto-looping a group re-plays the whole group (including its AutoPlay chain) each cycle.

- Set delay and toggle Auto-Loop in the item's inspector, or right-click the item → **Auto-Loop** for quick presets (5s / 10s / 30s / 60s / Custom…).
- Each item runs its own countdown — multiple items can loop independently.
- A distinct **LOOP** row appears in the Activity panel per looping item, with a progress bar counting down to the next fire.
- Delay and enabled state persist with the rundown XML (`<autoloop>`, `<autoloopdelay>`).
- The loop cancels on any manual Stop or Clear of the item, on disabling the item, and when a Clear Output item wipes its channel — it never re-fires after you've stopped it.
- New **Stop All Auto-Loops** tool item (Library → Tools, or right-click → Other): fire it to instantly kill every running auto-loop countdown in one go — a panic button for loops. It fires immediately, ignoring any item delay.

---

## Simple Mode

A new operator-focused interface, toggled via **View → Simple Mode**, aimed at prebuilt projects where the full rundown UI is more than the operator needs.

### Button grid instead of the rundown
- The center area becomes a **streamdeck-style grid of buttons** — one per rundown item flagged with the new **"Show as button"** checkbox in the inspector's new **Simple Mode** section (flag persists in the rundown XML).
- Clicking a button **selects** the item (same semantics as clicking a rundown row) — all playout hotkeys (F1/F2/…) then act on it, exactly like the normal interface.
- **Square, Companion-style keys** in a fixed slot grid — empty slots render as numbered dashed placeholders so the full grid is always visible. Buttons whose sub-buttons need more room stay grid-aligned by spanning extra slots vertically. Cells size themselves to fill the panel width; labels word-wrap to fill the key face; the selection border wraps the whole key including its controls.
- Per-item **Next** control on the button face (checkbox under "Show as button") alongside the readable ▶/■ controls. The ▶/■/⏭ controls are **true squares** — each control's height matches its share of the key width, so they read like real hardware keys.
- **Dropdown groups**: tick **"Treat as dropdown"** on a group (inspector → Simple Mode) and that group becomes a chooser instead of a container. Its Simple Mode key shows the group name, the **armed choice as a sub-label**, and a dropdown of the group's children, and **Play / F2 fires only the selected child** — as do Stop, Next and PVW. Build the list the normal way: make a group, put the items in it, tick the box. The selection is remembered in the rundown XML (`<treatasdropdown>`, `<dropdownindex>`), and picking from the dropdown also selects that key so the hotkeys land on it. The redirect happens in the central playout path, so it applies to hotkeys, buttons and OSC triggers alike.
- **Preview (PVW) control** on each key, toggled in Settings → Simple Mode. It plays the item — or the whole group — on the device's **preview channel**, leaving program untouched. It sits in the same square control row as ▶/■/⏭ and shrinks them evenly to keep every control square.
- **Taking a previewed item to program clears its preview layer**: pressing Play (button or hotkey) on an item that is sitting on the preview channel clears **only that item's layer** there — other items parked on other preview layers stay up. The client tracks the preview layers it lit (per device/channel/layer), so nothing else on the preview channel is ever touched.
- **Arrow-key grid cursor**: the arrow keys walk a cursor over the whole grid, empty slots included (Companion-style). Landing on a button selects it — same as clicking it, so the playout hotkeys immediately act on it; an empty slot under the cursor shows a light border. Clicking anywhere in the grid puts the cursor there too.
- **Two-step move, Companion-style**: select a button, press **Move**, then click a free slot (free slots highlight blue) — buttons never overwrite or displace each other. **Remove** takes a button off the grid by clearing its flag; the rundown item itself is untouched and keeps its slot for re-flagging. Slots are saved per item in the rundown XML (`<simplemodeslot>`), so arrangements travel between clients.
- Optional **▶ / ■ controls** on each button face for mouse-only operation (Settings → Simple Mode).
- **Explicit key size** in the button's right-click menu (**Button Size**): **Width** single or double, **Height** auto / single / double. Auto height grows only as far as the content needs; an explicit height is honoured exactly. Double-width keys occupy two columns and never straddle the grid edge or overlap a neighbour — moving one still refuses any placement that would overwrite. Persists per item in the rundown XML (`<simplemodewidth>`, `<simplemodeheight>`).
- Keys only span a second slot when their content genuinely needs it: the row calculation mirrors the rendered key exactly — the same widgets, heights and spacing, with the label's wrapped height measured from the real text — instead of a padded estimate. Adding an **icon**, a dropdown or a couple of invoke sub-buttons no longer pushes a key to double height unless it truly doesn't fit.
- **Grid dimensions (columns × rows)** are set directly in the bottom bar (1–12 each, persisted); cells scale dynamically so the configured grid fills the panel in both directions while staying square.
- Item colors (Colorize Item) carry over as button colors; the selected button shows a green border.
- Each key carries a **tally strip** along its top edge that lights while that key is the **last thing fired** on its channel — the same moment and the same per-channel rule the rundown uses for its own active marker, and the same active colour (including a custom one). The strip always occupies its height, so lighting up never moves anything on the key.
- Each key carries a **channel badge** directly under its control row, showing channel and layer, coloured by the same channel-colour logic the rundown badges use — so a channel reads the same everywhere in the client.
- **Keys hold their place.** Positions are written down as soon as a key is drawn, so removing a button — or deleting its item from the rundown — leaves every other key exactly where it was instead of sliding up to fill the gap. Changing the grid width translates saved positions through their row and column, so keys stay put there too. A position is written only the first time a key is drawn, so a key temporarily displaced by a neighbour that grew keeps its saved home and returns to it — resizing the window can never permanently rearrange the surface.

- All of these live in one **Simple Mode** section in the inspector — "Show as button", "Show next button", "Show invokes on group button" and "Treat as dropdown" — collapsed by default like Embedded Transform, so it stays out of the way until you need it.

### Labeled invokes as sub-buttons
- The Invoke section in the normal inspector gains a **Label column** per invoke row (persisted in the rundown XML).
- New **Discover Functions** button in the Invoke section: scans the template's HTML for its JavaScript function declarations (webcg lifecycle functions — play, stop, next, update, remove, init, data — are skipped) and offers them as **dropdown choices on each Function field** (still free-typeable). The dropdowns also refresh automatically for the selected item's template when switching rundown items. Fill the Label column for the invokes that should appear as Simple Mode sub-buttons.
- In Simple Mode, template buttons show a sub-button per **labeled** invoke — the label is what the operator sees; unlabeled invokes are hidden there. Each sub-button spans the **full width** of the key, so long labels stay readable; the key grows downward into extra slots to fit them.
- **Invokes on group buttons**: a template inside a group can publish its labeled invokes onto the *group's* button via **"Show invokes on group button"** (inspector → Simple Mode). The group key then shows those invokes as sub-buttons and firing one hits that child template on its own channel/layer — so an operator can drive a whole group from one key. Persists in the rundown XML (`<simplemodegroupinvokes>`); works for templates nested one group deep.
- Invoke rows in the inspector have a **drag handle** (grip at the left of each row) — drag a row up or down to reorder the invokes. The order is what Simple Mode's sub-buttons follow and what gets saved to the rundown XML; the F7 default marker stays with its row.

### Own layout
- Simple Mode has a completely **independent panel layout** (own column order and panel assignments), edited in **Settings → Simple Mode** — same editor as the normal layout, empty by default. Any panel can be placed, including the new **Simple Inspector**.

### Simple Inspector
- A stripped-down inspector panel showing only: **label, channel/videolayer, and the template's key/values** with an **Update** button that pushes edited values to the on-air template.
- Header matches the full template inspector: **Update, + and −** sit right-aligned above the data table (same tool-button style); rows are edited — including their mode (text/integer/decimal/boolean/color/cycle) — by **double-clicking**, via the same dialog as the full inspector.
- The panel carries the **standard panel menu** (hamburger): move between panels, size mode, anchor, collapse.

### Button context menu (right-click)
- **Label Size** (Small → Huge) and **Set Icon...** per button — the icon picker offers a glyph/emoji grid plus free text, stored as text in the rundown XML so icons travel between clients with no image files. Both persist per item (`<simplemodelabelsize>`, `<simplemodeicon>`).
- **Remove from Grid** is also available in the menu (clears the flag; the rundown item is untouched).

### Update button in the full inspector
- The normal template inspector's Import Fields row now also has an **Update** button — both inspectors share the same look, and template data can be pushed to air from either.


---

## Dialog Positioning Fix

The key/value add/edit dialogs (template data, HTTP GET/POST data) position themselves near the cursor — they are now clamped to the visible screen area, so they can no longer open off-screen when the inspector sits near a screen edge.

---

## Sheet-Bound Templates

A template that lives off a Google Sheet row can now say so, and the inspector resolves it — the operator sees real names and numbers instead of the ids used to fetch them.

### What a template declares
Alongside the two objects that already exist, a template adds three more. Same rules: flat, quoted strings, inline.

```js
window.debugData      = { "name": "Amine Gouiri", "number": "9" };
window.debugDataModes = { "number": "integer" };

window.debugDataTab   = "LINEUP";                              // which tab
window.debugDataKey   = "f0";                                  // column identifying a row
window.debugDataMap   = { "name": "NAME", "number": "NUMBER" }; // template key <- sheet column
```

### What the item stores
Only which row it is. The tab, the key column and the mapping all come from the template, so the same binding never has to be described twice. Persists in the rundown XML (`<sheetrow>`, `<sheetresolvedat>`).

### In the inspector
- A **Sheet** row appears — and only appears — for templates that declare a source. It shows the tab and key column, a row picker, a **Resolve** button, and when the values were last read.
- Resolve fills the mapped keys and stamps the time. The picker then lists every row, showing the id alongside the first mapped column so the list reads as names rather than ids.
- **Bound values are read-only**, tinted and flagged, since the sheet owns them. Clearing the row binding hands them back for editing by hand.
- A missing row or a renamed column says so rather than sending blanks.

### Sheets strain meter
The Performance panel gains a **Sheets** row whenever something touches a sheet: a meter showing reads in the last minute against the per-minute budget (60 by default, configurable). It counts this client's own reads exactly and estimates template reads from the plays it fires, since a sheet-driven template reads the sheet every time it renders and that traffic happens inside the graphic where the client cannot see it. Green under half, amber past half, red past three quarters; the tooltip splits the two figures. The row stays hidden on shows that never touch a sheet.

### Publishing strain outward
One client can never see the whole picture: it counts its own reads exactly, but reads made inside a graphic happen where it cannot look. So it publishes what it does know and lets something with a wider view do the totalling.

Set **Settings → Sheets → Report to URL** and the client POSTs a small JSON body every ten seconds, broken down per spreadsheet because the budget belongs to the API key rather than to the client:

```json
{
  "source": "casparcg-client",
  "host": "GALLERY-PC",
  "windowSeconds": 60,
  "limitPerMinute": 60,
  "at": "2026-09-02T12:00:00Z",
  "sheets": [
    { "spreadsheetId": "1AbC...", "clientReads": 3, "templateReadsEstimated": 9 }
  ],
  "note": "clientReads are exact; templateReadsEstimated is inferred from plays of sheet-driven templates"
}
```

Nothing is sent when the field is empty, or when no sheet has been read in the last minute. The same tab now also exposes the **cache service URL** and the **per-minute budget**, which previously had to be set in the database by hand.

A collector to receive it (`strain_server.php`, sits beside `local_server.php`) accepts these reports, and also accepts a one-line tick from a template each time it reads the sheet:

```
GET strain_server.php?tick=1&spreadsheetId=<id>&source=<template>
```

Ticks are counted rather than inferred, so where templates report for themselves their own count **replaces** the client's guess about them instead of being added to it. `GET strain_server.php` then returns the total per spreadsheet, flagged `counted` or `estimated`, with stale sources listed but excluded — one URL for the Salvo connector to poll.

### Hosting the cache in the client
The client can now be the cache service, so the PHP one beside it becomes optional. **Settings → Sheets → Host the cache in this client**, pick a port (3000 by default) and a folder.

It answers on the parameters rather than on a path — any request carrying `spreadsheetId` and `sheetNumber` is a cache request — so a template still asking for `local_server.php` reaches it untouched. File names match the PHP service too, so an existing `sheets_data` folder can be used as it stands.

| | |
|---|---|
| `GET …?spreadsheetId=X&sheetNumber=N` | cached rows, or 404 |
| `POST …?spreadsheetId=X&sheetNumber=N` | store rows |
| `GET /strain` | reads in the last minute, totalled |
| `GET /strain?tick=1&spreadsheetId=X` | a template reporting one read |
| `GET /bypass?on=1` / `?on=0` | throw the switch from anywhere |

**Bypass** now lives where it is quick to reach: **Other → Sheet Cache Bypass**, the settings tab, or that URL. Reads answer 404 so graphics go live to the sheet; writes still land, so the cache stays warm for the moment it is switched back on.

Because a template warming the cache has already paid Google for that read, a `POST` is counted as a read — which means **strain becomes counted rather than estimated with no change to any template**. The client's own warm writes carry `via=client` and are skipped, since the read behind them was already counted on the way in. Where counted reads exist for a spreadsheet they replace the play-based guess instead of adding to it.

`GET /strain` returns the same body the client POSTs outward, so the Salvo connector can either be pushed to or poll — whichever suits it.

### Warming, driven by demand
Nothing is warmed on a schedule. A tab is refreshed because something asked for it — a template went to air, the inspector resolved a row, the cache came back empty — and then only if what is held has gone stale (30 s by default). The traffic stays proportional to the show rather than to the clock, and a show that touches nothing generates nothing.

A play is the strongest signal there is, so it refreshes the tab it used: not for the play that just happened, but for the next one.

Refreshes read through the **keyless proxy** named as `mainurl` in the project's `project.js` — the client parses it now — so they cost nothing against the per-minute budget, and the rows come back already in the shape the cache holds. Without a proxy a refresh has to come out of the budget, so it is skipped once the budget is half spent: the show's own reads come first.

Several tabs wanted at once trickle rather than burst, spaced 1.5 s apart. Both numbers live in **Settings → Sheets → Warming**. Proxy reads are reported separately in the strain figures, as traffic that is not budget.

Rows also stay in memory for ten seconds, so walking a list of them is not a request apiece.

### A row picker built for Simple Mode
The full inspector's version is a data source with a resolve step. This one is a list of names: pick one and the values follow, in a single click. It appears only for bound templates, loads its list on selection without being asked, and shows the sheet and the last resolve time in its tooltip rather than spending a row on them. Values the sheet owns are tinted and not editable by hand, the same rule as the full inspector.

### Duplicated items keep their settings
`clone()` is written once per item type and only ever knew about that type's own command, so anything shared by every command had to be repeated forty-odd times — and was not. A duplicated item silently lost its Simple Mode button settings, and a template lost its sheet binding.

Duplication now goes through one seam that carries those across, so the fix holds for properties added later too. The one thing deliberately not copied is the button's **slot**: a slot holds one item, so a copy settles into the first free place instead of fighting the original for its own.

### Seeing what the sheet returned
The inspector shows only the columns a template declares, which is the right amount to work with and the wrong amount to debug with: a blank value could be a renamed column, an empty cell, or a row that resolved somewhere unexpected, and they all look the same.

A **Data** button beside Resolve (and the same button in the Simple inspector) shows the whole row as it came back:

- Mapped columns first, in the order the template declares them, each with the template key it feeds
- Unmapped columns after, dimmed — what the template *could* read next
- A declared column the sheet does not have is marked in red as **(no such column)**, and a column that exists but is blank reads **(empty)** — the distinction that is otherwise invisible
- **Copy** puts the row on the clipboard as JSON

If nothing has been read yet the button reads the row first and opens on the answer, so it means the same thing whether or not you resolved.

### Switching a project between cache and live, from inside the client
**Settings → Sheets → Template Projects** lists every discovered project with a checkbox. Ticked writes `local = true` into that project's `project.js`, so its templates read the cache at localhost:3000; unticked writes `false`, which is a relative path that fails inside CasparCG and sends them straight to Google.

The flag is written into the project's own `project.js` rather than kept as a second copy here, because that file is what the templates actually read. Only the value is rewritten — comments, spacing, line endings and the keyword in front of it are left exactly as they were. A project without a `local = true/false` line is listed but not switchable; nothing is invented in a file we do not own.

Templates pick the change up the next time they load.

### The key/value table sizes itself
It is now as tall as the keys in it plus one free row, so there is always somewhere obvious to add the next key and no empty space when there are only two. Applies however the rows arrived — typed, imported, resolved from a sheet, or undone. Past twenty rows it stops growing and scrolls, so a large template does not turn the inspector into one long scroll.

### A light for the cache in the Server Status panel
The hosted cache now sits with the playout servers, in the same shape as a device row — name, light, button — because it answers the same question: is this thing responding right now, and can I change that from here.

- **Green** — listening and serving. The button reads **Bypass**.
- **Amber** — listening, bypassing. Deliberately serving nothing while writes still land, so it is amber rather than red: a red light here would read as a fault every time somebody went live on purpose. The button reads **Serve**.
- **Red** — enabled but not listening, which almost always means the port already belongs to something else. The tooltip says so, and names the port.

The row is hidden entirely when the client is not hosting the cache.

The button is the bypass switch, so throwing it mid-show does not mean going looking for a menu. The light is read back from the server every couple of seconds rather than remembered from the last click, so it stays right when bypass is flipped from the menu, the settings or over HTTP.

### Strain reporting works both ways
The client already published what it was spending. It now also accepts what other applications are spending, so whichever end does the adding up has the whole picture instead of its own corner of it.

```
POST /strain          a windowed report from another application
GET  /strain?tick=1&spreadsheetId=X[&count=N]   one read, or N of them
GET  /strain          everything, totalled
```

The body is the same one the client publishes, so a client can report to another client with no translation:

```json
{ "source": "dataconnector", "host": "SALVO-PC", "windowSeconds": 60,
  "sheets": [ { "spreadsheetId": "1AbC…", "reads": 12 } ] }
```

`reads` is the plain way to say it; a report carrying `clientReads` and `templateReads` instead is added up the same way, which is what makes the client's own output valid input.

Reported reads appear as `externalReads` per spreadsheet, counted into the total and kept separate as well, so it stays clear which part of a total was not seen first-hand. They also feed the Performance meter, whose tooltip names them only when somebody is reporting.

A report describes a window, not a running tally, so a reporter that stops fades out after ninety seconds rather than holding a number up forever. `GET /strain` lists every reporter with its age and whether it has gone stale, so a total can be trusted or not on the evidence.

The `?tick=` form now takes an optional `count`, so an application batching its own reads can say so in one request instead of one per read.

Reports are read for numbers and identity and nothing else: no field of one reaches a file path, a URL or a command, source and host are length-capped, and a reporter that keeps renaming itself cannot grow the table without bound.

### Clearing the cache
**Settings → Sheets → Host the cache** gains a **Clear cache** button, which carries the count and size of what it would remove — so it states a fact about the cache rather than being a lever with an unknown effect. It disables itself and reads *(empty)* when there is nothing to clear, and re-counts when the folder is repointed.

Clearing asks first, naming the folder and how many files. Nothing is lost that a read will not fetch again: each tab goes back to the sheet the next time something asks for it, so this costs reads rather than data — which the confirmation says.

Only files named the way this service names them (`<spreadsheetId>_<sheet>.json`) are touched, because the folder is meant to be shared with the PHP service and with whatever else is kept beside it. A file that will not delete is reported by name rather than passed over quietly.

### The Sheets panel joins the picture
The panel predates the resolver and was fetching the API directly with its own network stack, which meant two things nobody could see: its reads did not appear in the strain meter, and — because it polls, as often as every ten seconds — it was quietly the most regular reader in the client while showing as nothing at all.

Both of its calls now declare themselves, so the meter counts them like any other read.

More usefully, a poll now **warms the cache**. The call to Google was already paid for, so the rows are reshaped the way the cache holds them and written back on the way past. A panel left open on a tab keeps that tab warm for the templates and the inspector for free, and the warmer stops asking for what the panel is already fetching.

### Expected result, and one declaration instead of two
A template that reads a sheet says so, in the form the templates already carry:

```js
window.sheetConnection = { "tab": "LINEUP", "key": "homelineup" };
```

`tab` is the sheet tab; `key` names the template's own data field whose value picks the row. That value is an **index into the rows**, not a column to match on, because `functions.js` resolves it as `data[key]` — the client resolves it exactly the same way, or it would answer confidently with the wrong row.

Below the key/value table, any template that declares a connection gets an **expected result** box: headed `TAB → key`, showing how many rows came back and the resolved row column by column, with a **Refresh** that re-reads the tab on demand. `row = 4` is not an answer until you can see who row 4 is. It re-resolves as the key is typed — no read, it already holds the tab — and only goes back to the sheet when Refresh is pressed. A template that declares nothing gets no box rather than an empty one, and that silence is meaningful: the connector-driven templates have no sheet at all.

**The earlier declaration is gone.** `debugDataTab` / `debugDataKey` / `debugDataMap` described a different architecture — one where the client resolved a row and filled the template's fields, which is why bound values were read-only — and no template ever carried it. With it go the sheet row picker, the Resolve and Data buttons, the read-only field marking, the row dialog, and the `<sheetrow>` / `<sheetresolvedat>` item properties. One declaration, the one that is actually deployed, shared with what `preview.html` already reads.

### Where the box's rows came from, and how old they are
The box was already filled from the cache — `fetchRows` reads the cache before it spends anything on the API — but silently, so a copy read a second ago and one left over from last week looked identical. Both facts are now on screen beside the row count:

- **live** — straight from the sheet
- **cache · 4m old** — served from the cache, with the age of that copy
- **proxy** — read through the keyless proxy
- **cache · age unknown** — a cache that does not say when it stored what it served

Grey under an hour, amber past one, red past a day: a cached copy is the normal case and not a problem in itself, so the colour follows the **age** rather than the source. A day-old row may be describing last week's match.

For the client to know the age, the cache has to say. The client's own service now sends both `Last-Modified` and an ISO `X-Sheet-Cached-At` on every cache read, taken from the file it is serving. The PHP service sends neither, and that reads as *age unknown* rather than as a guess.

A memory hit reports the age of the underlying copy rather than the age of our copy of it, so glancing twice does not make stale rows look fresh.

### Simple Mode first in the inspector
The **Simple Mode** section now sits at the top, above Output. It is the section reached for on every item while building a button surface, where the rest are reached for when something specific needs changing. Still collapsed by default, and the toggle still sticks for the session.

Under the hood the section is still declared last, so the hundred and twenty places in the inspector that name a section by its declared index keep meaning what they say. A single helper resolves declaration order to display order — renumbering them by hand is how one missed line ends up quietly hiding the wrong section.

### Undo works in Simple Mode
Moving, removing and resizing a button could not be taken back, while the rundown those buttons live in could. Every grid edit now goes on the rundown's own undo stack, reachable with **Ctrl+Z** from Simple Mode:

- Move a button — *Move Simple Mode Button*
- Remove one, from the move bar or the right-click menu — *Remove From Simple Mode*
- Change its width or height — *Change Button Size*
- Change its label size or icon — *Change Label Size*, *Set Button Icon*

A grid edit is an edit to the rundown, so it belongs on the same stack rather than a private one: the snapshot the rundown already takes carries every Simple Mode property, because they are persisted with the item. Undo restores them and the grid redraws itself, which is what the structure-change notification added in build 128 was for.

A move captures the positions of the *other* buttons too. Placing one is what fixes everything else in place, so undoing it has to put those back as well or the surface would come back subtly rearranged.

The dropdown group's current selection is deliberately not undoable — it is an operational choice like selecting an item, not an edit to the surface.

### NDI tiles come back on their sources
The panel already carried tile assignments across a layout change: it snapshots what each viewer is showing, rebuilds, and puts them back. That snapshot is taken from the live viewers, so at startup there is nothing to take it from — and although every change was written to `NdiOutputConfig`, nothing ever read it back. The grid returned in the right shape with every tile empty.

It is read now, and the saved set stands in for the snapshot when there are no viewers to take one from.

Discovery is the awkward part: it runs well after the panel is built, so a saved name almost never matches anything on the first attempt. Those names are held and applied as sources appear — `NdiManager::sourcesChanged` had no listener until now — and dropped once satisfied. A tile the operator has already filled by hand is left alone.

**Settings → NDI → Reconnect NDI sources on startup** turns it off. Reconnecting on launch is a decision about the network rather than about this window, so it is a switch rather than an assumption.

### Fixed: the inspector handed every section the wrong widget
Moving Simple Mode to the top of the inspector in build 140 introduced a helper that maps a section's declared index onto its display row. The helper assumed Simple Mode already occupied row 0 — but it was inserted at the *end* of the constructor, long after all forty-one sections had been built through that helper.

So during construction every section was handed its neighbour's widget, and the last one was handed nothing at all. On screen the section headers stacked at the top with their contents below them.

Simple Mode is now inserted before the first use of the helper, which is what makes its assumption true. Present in 140 through 142.

### One budget per API key
Each project reads with its own key, and Google's per-minute budget belongs to the key rather than to the spreadsheet — so two projects on separate keys are two budgets, and adding their reads together said "60/60" when neither was half spent.

Strain now groups by key. The meter shows the key **closest to its own limit** and names the project it belongs to, because "48/60" is only an answer once you know whose sixty it is. A sheet whose project cannot be resolved is counted on its own rather than folded into somebody else's budget.

The key itself is never published. `GET /strain` reports a `keyId` — six hex characters of a digest — which is enough to group sheets that share a budget and useless to anyone who does not already hold the key. Each sheet entry also carries its `project` name.

### Editing a project's API key
**Settings → Sheets → Template Projects** now shows each project's key beside its cache checkbox, and writes it into that project's `project.js` as you type. Only the value between the quotes is replaced; the declaration keyword, spacing and any comment around it are left as they were.

As you type means as you type: a half-typed key is a half-typed key on disk, and a template loading mid-edit reads what is there. If the file refuses the write the field is outlined and the reason goes to the status bar, rather than the field quietly looking saved.

### A meter per key
The Performance panel now draws one Sheets row per API key instead of one overall, since that is what the budget belongs to. Each row carries the project name, its own meter against its own sixty, and its own colour.

Busiest key first, so the one nearest its limit is the one at the top. A row is created when its key is first seen and kept afterwards — a project that falls quiet shows zero rather than vanishing, because a panel that reshuffles itself every time something pauses is harder to read at a glance than one with a quiet row in it.

A spreadsheet no discovered project claims still gets its own row, labelled with the key digest, rather than being folded into somebody else's budget.

`GET /strain` already carries `project` and `keyId` per sheet, so an outside collector can group the same way.

### The expected-result box reads the declaration the templates actually carry
The box showed nothing for any template, and the reason was in the parser, not the data. All thirty-eight templates declare their sheet like this:

```js
window.sheetConnection = { "tab": "LINEUP", "homelineup": "12", "awaylineup": "12" };
```

`tab` is the tab; every other property names one of the template's own fields whose value picks a place in the sheet, and the number is **how many lines are read from there**, header line included. The calendar takes six, a lineup side twelve. The client was looking for a `"key"` property that none of them has, found nothing, and hid the box — silently.

It now reads every field and its line count, accepts the documented `"key": "row"` form as one line, and shows **the whole block**, line by line. A block that comes back short is flagged in amber — `homelineup = 1 · 11 of 12 lines` — because eleven names where twelve were asked for is a set the sheet has not finished, and that is worth seeing before air. A field the operator has not set is said so; one out of range is said so in red. The raw `f0` stands in for a named field, as the doc allows.

Two things stop it going quiet again. A declaration that exists but has no `tab` is **shown as broken** rather than hidden. And **Refresh re-reads the declaration** as well as the rows, so a template corrected while the client is running does not need a restart to be believed.

One honest limit, stated in the heading's tooltip: the client cannot run template code, so it never calls a template's `getNumberForRow()`; it always counts from the field's value as a row index. For a template whose set id is not its row number the block may start elsewhere than the preview stand shows. The stand stays the authority for those.

### The box places a block the way the template does
Build 145 read the declaration correctly and then placed every block the same way — counting from the field's value as a row index — with a note that this could be wrong for templates that place sets by their own arithmetic. That note has been retired. Every template in both packs places a value one of three ways, and each is chosen from the declaration alone, so nothing has to run:

- **One line, no `sets`** — the row whose `ID` column equals the value, else the value as a 1-based position. This is the templates' own `findRow()`, and it is what all fourteen single-line templates do. `row = 1` is the row with ID 1, not the second row.
- **Several lines, no `sets`** — base row `(value − 1) × lines`. This is `getNumberForRow()`: calendar and results at 6, standings at 21. The stride was checked against each template's `ROWS_PER_SET` in both packs.
- **`sets` declared** — the value-th run of member rows, the row above the run being its header. This is `findTeams()`, which the lineups use, and it is the one case a count cannot express: the sets are only as long as their runs, and the lineup tab is found by looking, not by multiplying.

`sets` is a new declaration keyword naming the member-row test — `"NUMBER:digit"` for the lineup rule, `"COLUMN"` for a filled cell — and the heading's tooltip now says which rule is in use. The only templates that need it are the two `lineups_column.html`; the doc on the Desktop carries the exact line. Without it the client falls back to the stride rule for them and says so.

### Fixed: the expected-result box was never built
Since build 138 the box under the key/value table had been declared, wired and documented — and never constructed. Its builder asked the table's parent widget for its layout and cast the result to a `QVBoxLayout`. The table sits in `verticalLayoutData`, which *is* a `QVBoxLayout`, but nested inside the section's grid; the parent widget's layout is the grid, the cast failed, and the builder returned before creating anything. Every later feature on the box — blocks, staleness, the placement families — was correct code attached to a widget that did not exist.

The nested layout is generated by name, so it is now taken by name. Eight builds of "does not give me the values back" came down to one cast.

Also gone: a stray **"Sheet"** label left behind when the earlier binding row was removed. It was created with the section as its parent and never placed in a layout, so it painted at the section's origin.

### The expected-result box: sheet order, fallback keys, and where things sit
Four changes now that the box exists:

- **Columns in the sheet's order.** `SheetRow` is a `QMap`, which sorts its keys, so the box showed ID, QUESTION, REFERENCE where the sheet says ID, REFERENCE, QUESTION. The header order now travels with the rows from wherever they were read — the API's header row, or the first object's key order read off the cache text, since `QJsonObject` sorts too.
- **Fallback keys by position.** The doc says the raw `f0` stands in for the named field, and the box only ever looked at `f0`. The templates read them by position — lineups take `f0` as home and `f1` as away — so the *n*th declared field now falls back to `f(n−1)`.
- **Update and Import Fields sit between the table and the result**, where they act on the table they follow.
- **The option rows moved below the result.** Use stored data, uppercase, trigger on next, send as JSON and newline behaviour are set once per template and left alone; the table and its result are what an operator works in. Auto-play and Auto-loop stay last.

### The result answers a key the moment it is added
Adding a key through the **+** dialog inserts a finished row, so the table's model reports `rowsInserted` and never `dataChanged` — and the result box was only listening for the latter. A freshly added `row` key went unanswered until the item was reselected. Insert, remove and reset now re-resolve the box as an edit does, and the tree's own `itemChanged` is listened to as well, so an inline edit committed through the value delegate is caught even where the model signal is coalesced.

The per-block header line is now shown only when it says something the status line does not: a set of several lines, a set that came back short, or a failure. A single line that resolved cleanly no longer spends a row repeating `row = 1`.

### Template Settings, a section of its own
The option rows — use stored data, uppercase, trigger on next, send as JSON, newline behaviour, auto-play, auto-loop — are set once per template and then left alone. They now live in their own inspector section, **Template Settings**, directly under Template and **collapsed by default**, like Embedded Transform and Simple Mode. The Template section keeps what an operator works in: the key/value table, Update and Import Fields, and the expected result.

Nothing about the controls changed. They are still owned and wired by the Template widget; it lays them out in a panel it hands to the inspector, which mounts the panel as the section. Shown and hidden together with Template, so a non-template item shows neither.

### No more dead space in the Template section
The gaps above and below the result box were the section being taller than its content. An inspector row keeps the height it was first given, and the grid inside spreads any excess between its rows — so as the table shrank to fit its keys and the option rows moved out, the slack showed up as empty bands.

The Template section now raises `contentChanged` whenever its height changes — the table resized, the result box shown, hidden or re-rendered — and the inspector re-reads its size hint on the next turn of the loop, the same arrangement the Invoke section already had. The result box is pinned to a fixed vertical policy so it cannot absorb slack, and whatever remains lands in an empty stretch row at the very bottom rather than between rows. The Template Settings row is sized from its panel the same way.

### The last of the dead space
The Template section's `.ui` gave it a minimum height of 480 px with a `MinimumExpanding` policy — fine when the table was a fixed 200 px block and every option row lived inside, wrong once the table shrinks to its keys and the options have a section of their own. The section had to be at least 480 px tall, and the grid inside had to put the difference somewhere.

Where it put it was instructive. The result box carried a `Fixed` vertical policy, which only stops a layout *asking* a widget to grow; when a cell is forced taller regardless, the widget is still stretched into it, and the slack landed in the box's header row — a status line and a button centred in eighty pixels. The floor is gone from the `.ui`, so the section is now exactly as tall as its content and there is nothing to distribute.

Three belts against it coming back: the box's layout is pinned with `SetFixedSize`, so the widget itself can never exceed its content; it is top-aligned in its cell; and the result tree's height is counted from the rows actually added, which removes the empty line that appeared under a single-line block once its header became lazy.

### The result box is full width again
`SetFixedSize` pins both dimensions to the size hint, so build 152 left the box exactly as wide as its longest value. The constraint is gone; the box expands to the row, and its **height** is pinned by hand at the same moments its content changes — shown, hidden, rendered, or a tab-only read — which is the half of `SetFixedSize` that was wanted.

### Pushing template packs from a dev machine
A small tool, **CasparCG Template Push**, built beside the client. It holds a folder of packs, a list of clients, and a button. **Compare** asks each client what it has and reports what would change without writing anything; **Push** sends only the files that differ.

The receiving half is two endpoints on the HTTP server the client already runs for the sheet cache, so there is no second service to start:

```
GET /templates              the packs on that client
GET /templates/<PACK>       every file with a digest, for comparing
PUT /templates/<PACK>/<path>   install one file
```

**It is off until switched on, and every request carries a token.** A template is HTML that CasparCG executes, so an open install endpoint on a playout machine is a way to run code on it. **Settings → Templates** has the switch, a Generate button for the token, and the folder to install into — left empty, that is the template path of the first device that has one. An unset token never matches, so turning the feature on without setting one leaves the endpoint shut rather than open.

**`project.js` and `extensions.json` are never written by a push.** The client itself edits the first (the API key and the `local` flag) and the Sheets panel writes the second, so a push that replaced them would quietly undo operator settings. The rule is enforced on the client, not just avoided by the pusher.

Paths are taken apart segment by segment and the result checked against the folder it must sit under, so nothing escapes a pack. Files are written whole or not at all: a template half-replaced while CasparCG has it open is worse than one that was not updated.

### File-level review before anything is written
Push is now two steps and always in that order. **Compare** reads every ticked pack on every ticked client and lists each file with what it found; **Push ticked** sends the ones still ticked. Nothing is written until the list has been seen, which is the right default when the far end may be on air.

Unchanged files were never being sent — both halves compared by digest from the start — but there was no way to see that. They are now listed too, dimmed and unticked, so the list is the evidence rather than a promise. New is green, changed is amber, and either can be unticked to hold it back; an unchanged one can be ticked to force it.

A row says what happened to it after a push, and a sent file unticks itself so a second press cannot send it twice. **Tick all** and **Tick none** are there for a big pack.

The templates-folder tooltip now says what it wants: the folder that *holds* the packs, not a pack itself. Pointing it at `...\templates\SEVILLE` would have read `webcg` and `font` as packs — and now the file list would show that before anything was written.

### Push across networks: identify, deadlines, and keeping probes out
Groundwork for a pusher that is not on the same network as the client it is pushing to.

**Identify** asks each ticked client who it is — machine name, OS, where its packs live, how many it has — and answers the question a push should not be the first to ask: can I reach it, and is my token right. It writes nothing.

**Every request now has a deadline** of twenty seconds. On a local network that never fires. Across the internet it is the difference between a slow push and one that has silently died with the queue behind it.

**Repeated wrong tokens from one address are refused outright** for five minutes, because a wrong token is the shape a probe takes and an endpoint that can be reached from the internet will be probed. One correct token clears the count, so an operator who mistyped it four times and then got it right is not treated as an attack. The pusher names that refusal, along with a switched-off endpoint and a bad token, rather than reporting a transport error to decipher.

### A relay, so the venue opens nothing
Pushing straight at a client works on one network and is the better option there. Across the internet it asks a playout machine to accept an inbound connection on an endpoint that writes HTML CasparCG will run, and that is not a port anyone should open at a venue.

So the direction is turned around. `tools/php/relay/relay.php` is a single file that drops on any PHP host from 7.4 up. The dev machine uploads a pack to it; each client asks it what is there and fetches what it does not already have. Both ends make outbound connections only, and neither has to know where the other is.

```
  dev machine  --upload-->   relay.php   <--poll & fetch--  client
                             (your host)                    client
```

**Two tokens, deliberately not the same one.** The dev machine holds the upload token; the clients hold the download token. A client that is stolen reads only what it was already going to install, and cannot put a template on the relay for the other clients to pick up.

**The pusher treats a relay as one more row.** Anything typed into Host : Port with a `://` in it is a relay rather than a client, so the table, Identify, Compare and Push all work unchanged and a relay and a direct client can be pushed to in the same pass. Identify says which relay answered and whether that token can actually upload, which is how the download token being pasted in the wrong box gets caught.

**Settings -> Templates -> Pull Packs From A Relay** is the receiving half. An address, the download token, how often to check, and optionally the packs this client owns so one relay can carry every venue while each client takes only its own. **Test** reaches the relay and writes nothing; **Check now** polls immediately. A client also checks a few seconds after it starts, because a machine that was switched off is the one most likely to be behind.

**A pull never deletes.** A file removed from the relay stays on every client that already has it. Taking a template off a machine that may be on air is not a decision worth making from the other side of the internet.

**The bytes are checked against what the manifest promised.** A client hashes what it fetched and refuses to install it if the digest does not match, so a body that changed in transit is never written where CasparCG would run it. `project.js` and `extensions.json` are refused by the relay *and* by the client, and paths are checked at both ends, so neither end depends on the other being careful.

**Put the relay behind HTTPS.** The tokens travel in a header and the templates travel as bytes. That is the one part the PHP file cannot do for itself, and `tools/php/relay/README.md` says so in the install steps.

### Windows device names are refused as filenames
`con.html`, `nul`, `lpt1.js` and the rest of the reserved names are device names on Windows with or without an extension, so writing one opens a console or a port instead of a file. Both the relay and the client's installer now refuse them by name, rather than leaving a confusing write failure to be discovered on a playout machine.

### Retrying the failures that are worth retrying
Over the internet a dropped connection is the ordinary case, not the interesting one. A single blip used to fail a file outright: the pusher moved to the next one, and a client waited out the whole poll interval before trying again.

Both ends now try a file three times, backing off two seconds and then four. **Only failures that could plausibly succeed next time are retried** - a connection that went away, or a 5xx from a host under load. A 401, 403, 409 or a throttle is an answer and will be the same answer next time, so retrying one would only waste the operator's time and hammer the far end. The log says which file is being retried, why, and which attempt it is on.

The client retries its manifest read too, because the next poll may be a quarter of an hour away and a blip there costs the whole interval.

### The relay has a light in the Server Status panel
Whether templates are arriving was only visible by opening Settings, which is not somewhere anyone goes mid-show. The relay now has a row next to the sheet cache, the same shape and the same reason: is this working right now, and can I do something about it from here.

Green with how long ago it last checked, amber while it is checking, red if the last check failed, and grey when it has not run yet - grey rather than red on startup, because nothing has gone wrong. The tooltip carries the address and what the last check actually did. A **Check** button polls immediately. The row is hidden entirely on a client that does not pull.

### The upload leg is checked too
A client already hashed what it fetched from the relay and refused to install anything that was not what the manifest promised. The other leg had no such check: whatever arrived at the relay, or at a client over a direct push, was written.

The pusher now sends the digest of what it read off disk, and **both a relay and a client refuse a body that does not match it**, with a 422 and nothing written. An existing file is left exactly as it was rather than replaced by something that changed on the way. The header is optional, so an older sender still works, but the current one always sends it.

### Files that exist only at the far end are visible now
Neither a push nor a pull deletes anything, which is the right default when the far end may be on air. The cost is drift: rename `calendar.html` to `schedule.html` and every client keeps the old one forever, and the relay keeps serving it.

Compare now lists the other direction too. A file the far end has that the pack no longer does shows as **only there**, in its own colour, and the log says how many there are and what they mean. It is information first: nothing acts on those rows by itself.

**Clear** takes ticked "only there" files off a **relay**, and asks before it does. Clients are never touched by it - they have no delete endpoint on purpose, and a relay's own remove leaves whatever a client already installed exactly where it is. Ticking an "only there" row never sends anything either: Push skips them entirely, so the one checkbox cannot mean two things at once.

### Templates from a private GitHub repository
A second way to reach clients on a network you do not control, and for most people the easier one. It costs nothing to run, does the authentication and the HTTPS for you, and keeps the history of every template you have shipped as a side effect. A bad template can be reverted.

Write `github:owner/repo` where a relay address would go, on a client and in the push tool alike. Add `@branch` for anything other than the default. Each folder at the root of the repository is one pack; root files and dot folders are ignored.

**One request lists the whole repository with a digest per file.** Those digests are Git blob hashes, so a client hashes its own files the same way and fetches only what differs. A pack that has not changed costs a single request for the whole estate. Each file is then fetched **by its digest rather than its path**, so a branch that moves mid-poll cannot hand a client a different file than the one it was told about, and the bytes are hashed again on arrival.

**On the dev machine you may not need the tool at all.** Clone, copy the pack in, commit, push, and every client picks it up on its next poll. The push tool is there if you want the same review-before-you-send flow: Compare lists what differs, Push commits the ticked files one at a time with a message naming each, and Clear removes leftovers as commits, so nothing is ever really lost.

Clients, relays and repositories sit in the same table and can be pushed to in one pass. Identify names the repository, its default branch, and says loudly if it is **public**, because templates in a public repository are readable by anyone.

Tokens are fine-grained and scoped to the one repository: read-only on every client, read and write only in the push tool. A venue machine that goes missing cannot then change what the rest of the estate installs. `tools/php/relay/GITHUB.md` has the setup.

A repository too large for a single tree listing is refused rather than guessed at, because a truncated listing looks exactly like a repository missing files and would be reported as "up to date" while being wrong.

### The relay knows which clients have caught up
Uploading to a relay was uploading into a void: you heard that the relay took the file, and never learned whether the venue actually pulled it. Before a show that is the only question worth asking.

Clients now check in after each poll, reporting their machine name and the version they have of each pack they follow. `?action=clients` answers with the estate, each machine marked current or behind and named per pack. A client's own token cannot ask that question - it reports about itself and nothing more - and a hostile machine name cannot escape the folder it is written into.

That endpoint shipped in build 160 with nothing calling it on either side. Both ends exist now.

**A client reports after every poll**, naming each pack it follows and the version it reached. A pack that had a file fail is left out of the report rather than claimed: being absent is honest, being listed as current would not be. Nothing about the check-in is retried and nothing is said when it works, because bookkeeping for somebody else's benefit must never be the reason a poll looks failed.

**Identify now shows the estate.** Point it at a relay and, under the relay's own line, every machine that has checked in with when it last did and which packs it is behind on. Naming the pack matters: whether a stale MARSEILLE is a problem depends on tonight's show.

It only reads that list when the token can upload, so a dev machine holding the wrong token is told that rather than shown an empty estate.

**The GitHub route deliberately has no equivalent.** A report back would mean giving every venue machine write access to the templates every other machine installs, which is a bad trade for a status line. `tools/php/relay/GITHUB.md` says so where somebody choosing between the two will read it.

### Fixed: the client would not have compiled
`RelayClient.h` declared a method taking a `QNetworkRequest&` while only forward-declaring `QNetworkAccessManager`. Six errors, and the first thing a build of 154 through 161 would have hit. It is one line, and it had been sitting there since the GitHub work landed.

### A syntax check, so a build break is found before the build
`tools/syntax-check.py` parses and type-checks changed files without building anything: no object files, nothing written to the build folder, no linking. It is not a build and does not replace one.

```
python tools/syntax-check.py                 files changed since the last commit
python tools/syntax-check.py a.cpp b.cpp     just these
python tools/syntax-check.py --all-mine      everything changed on this branch
```

It runs uic over the .ui files and moc over every `Q_OBJECT` header the named sources need, and compiles the moc output too, which is where a signal with a type that does not exist shows up. Visual Studio, the Qt kit and the dependencies the build fetches for itself are all found by looking rather than by being told, so a version bump does not turn every check into a complaint about a missing header.

It will not catch anything that only fails at link time, such as a slot declared and never defined.

It does now. It reads each header for members declared and never given a body anywhere, which is the classic link error: the compiler is happy, moc connects the slot by name, and the link fails. Tuned against the real tree until it reported nothing on code that already links, then checked that it still catches a member deliberately left undefined.

### The relay can check its own deployment
`?action=selftest`, with the upload token. It asks the questions whose answers are silent when they are wrong.

- a token still on the shipped default, or both tokens the same, which collapses the read/write split
- storage sitting under the web root, which on nginx or IIS means every template is downloadable without a token, because the `.htaccess` beside it only binds Apache
- plain HTTP, where the token and the templates cross the network readable
- an upload limit smaller than the one the relay advertises
- errors that would be printed into responses

It answers `200` when nothing is wrong and `500` when something is, so it can go in a monitor.

### Two real faults it found immediately
**PHP's `post_max_size` is 8 MB by default, under the 32 MB the relay advertised.** A body over that limit does not arrive short, it arrives empty, and the relay would have written an empty file over a working template and reported success. Uploads now compare what arrived against the declared `Content-Length` and refuse a body that did not come whole, naming `post_max_size` as the cause. `ping` reports the limit that actually applies rather than the one in the source.

**A PHP warning printed in front of a JSON answer breaks every client.** On a host with `display_errors` on, one deprecation notice is enough, and the symptom gives no hint of the cause. The relay now turns display errors off for itself and discards anything that reached the output buffer before it answered, including on the raw file download where a warning would have corrupted the bytes.

That is not the whole fix: some warnings are emitted at request startup, before any code in the file runs. A `.user.ini` and an `.htaccess` now ship beside `relay.php` to cover that, and **they have to be deployed with it** - they are dotfiles, so check your upload tool is not hiding them. The self-test reports it as a problem if they were not.

### A venue can now say why it is stuck
A client that falls behind was a phone call: nobody can open the log on a machine at someone else's venue, and "behind on SEVILLE" does not say whether the disk is full, the token is wrong, or a file will not write.

The check-in now carries what the last poll actually did, and Identify prints it beside the machine that is behind. A client that is current is not made to repeat itself.

**A client reporting failures is never shown as current.** One that could install nothing reports no packs at all, so judging by the pack list alone called it up to date. It reads **FAILING** now, on both ends, and the reason comes with it. The reported text is capped, like everything else a client can send.

### Measured: the network timeouts are inactivity timeouts
Both halves set a transfer timeout, and it mattered whether that is a deadline or a gap. If it were a deadline, a large template on a slow venue link would be killed while it was transferring perfectly well, and every retry would do the same.

Measured against Qt 6.5.3 rather than assumed, because the documentation does not say. A transfer delivering a byte every two seconds for eighteen seconds completed under a five second timeout. A transfer that stopped delivering was cut at 5.0 seconds.

So it is a gap, the current values are right, and neither needs raising for a big file. Both are now commented to say so, because raising them would only mean waiting longer to notice a dead connection.

### The path rules are tested now, not argued about
Everything that reaches a client - a direct push, a relay, a GitHub repository - ends at the same function, and that function writes HTML CasparCG will execute. If a path could escape its pack, anyone who reached any of those routes could write anywhere the client can write. Until now that was a careful reading of the code rather than a fact.

`tools/test-paths.cpp` builds and runs against the real `TemplateInstaller`, with the three database calls stubbed because the rules under test never touch it. **60 checks, all passing.** Among them:

- parent traversal, plain and after a segment, forward slash and backslash
- absolute paths, drive letters, drive-relative paths, UNC paths
- NTFS alternate data streams, `a.html:hidden.exe` and `a.html::$DATA`, which hide a second file behind the first
- Windows device names with any extension, while `console.html` still installs normally
- trailing dots and spaces, which Windows strips, so two paths become one file
- embedded nulls, newlines, tabs, and a right-to-left override that makes a filename read backwards
- both protected files in every capitalisation, and near misses like `project.json` that must not be caught
- Git blob digests against values git itself produces, and that two files of the same length do not share one

**It was checked against a regression, not only against success.** Removing the device-name rule produced six failures; removing the backslash handling produced two. A suite that only ever passes proves nothing.

That second one corrected a misunderstanding of my own. The backslash traversal cases had been passing for the wrong reason - the character allowlist rejects a backslash whatever else happens - so they were not testing the normalisation at all. The rule is what lets `css\site.css` install to the right place; it is not what stops `..\evil.html`. Two cases were added that actually exercise it.

### Hardened: the second line of defence did not cover pack names
`installFile` checks the finished path against the folder it must sit under, described in its own comment as being there because the rules "should" have made an escape impossible and should is not a guarantee. Testing it showed the backstop was narrower than that.

It compared the destination against the **pack** folder, and the pack folder is built from the pack name. A pack name that climbed out of the templates folder took the prefix with it, so the check compared an escaped path against an escaped prefix and agreed with itself.

This was never reachable: the pack-name rule refuses those names, and that rule is tested. But a backstop that only works while the thing it backs up is working is not a backstop. It now compares against the templates root as well.

**Measured both ways.** With the pack-name rule switched off, the old code let two files land outside the templates root entirely. With the same rule switched off and the new check in place, nothing escapes at all.

### installFile is tested end to end
`tools/test-install.cpp` runs the real function against a real folder, because between the rules and the disk sit a path join, a `cleanPath`, a prefix check and a `QSaveFile`, and any of those could undo the answer the rules gave. **30 checks.**

It installs plain, nested and Windows-style paths and reads the bytes back; replaces a file and confirms none of the old content survives; refuses traversal in the path and in the pack name, absolute paths, drive letters, device names and data streams; refuses both protected files in any capitalisation while letting a nested `webcg/project.js` through as ordinary template code; accepts a matching digest, refuses a wrong one, and confirms a refused write leaves the existing file untouched.

Then it asks the question that actually matters. It sweeps the whole sandbox and asserts that **nothing landed outside the templates root**, and that the pack holds exactly the files it should and no others. That assertion, not the return codes, is what found the gap above.

### The digest map and the throttle are tested too
Two more things the whole system leans on, neither of which had ever been run.

**The digest map is what every GitHub comparison is made against.** A client lists the repository, hashes its own files the same way, and fetches what differs. If the keys did not look exactly like the paths in a tree, nothing would ever compare equal and every client would re-download every file on every poll, forever, with no error to show for it. On Windows that is a plausible way to be wrong, so the keys are now asserted to use forward slashes, at every depth, with the git-style and plain digests each checked against the value they claim to be.

**The throttle is what stops an install endpoint being guessed at.** Blocking after five wrong tokens is the obvious property. The one that matters more is that it blocks the address that got them wrong and nobody else: keyed on the wrong thing, a single probe would lock every venue out of its own updates. Both are tested, along with a correct token clearing the count rather than the count carrying over.

**Both were checked by breaking them.** Native separators in the digest keys produced five failures. Pointing the throttle at one shared bucket instead of one per address produced three, the isolation check among them.

That is 111 assertions across the two suites now, all passing.

### The HTTP parser is driven over a real socket now
The sheet cache server is hand-rolled HTTP on a `QTcpServer`, and it is the one part of this that listens. Whatever arrives on that port arrives as bytes off a network, from something that may not be a client at all, and every request is taken apart by code written here rather than by a web server somebody else maintains. It was the last piece with no test at all.

`tools/test-server.cpp` starts the real server on a spare port, points the installer at a temporary folder, and connects real sockets to it. **25 checks.**

The ordinary path first, including a template installed over TCP and verified on disk, which is the whole direct-push route in one assertion. Then the token: wrong, missing, and a header name in the wrong case, because HTTP header names are case-insensitive and a client that sends one in lower case is not wrong.

Then rubbish. An empty request, a request line with no target, one with no HTTP version, a header with no colon, binary noise, a negative content length, a length of 999999999, a length of the word banana, and a repeated token header. Each has to produce an answer or a refusal, and each is followed by a check that **the server is still listening**, because a crash here takes the client with it.

Two behaviours turned out to be worth asserting on their own. A request split across two packets with a real gap between them must be held until the rest arrives rather than acted on half-read. And a body longer than its declared length must be cut to that length, or a sender could append to somebody else's file.

**Both were confirmed by breaking them.** Removing the truncation produced one failure; removing the wait for the rest of a split request produced five, including an ordinary request afterwards.

That is 136 assertions across three suites. Every part of the template route that can be exercised without launching the client now is.

### The pull route runs end to end
This is what the whole exercise was for: a client on one network fetching templates from a host on another, with nothing reaching in to it. Every piece of that had been tested except the piece that does it. `RelayClient` reads a manifest, works out what differs, fetches it, checks the bytes, installs them and reports back, and none of that had ever run.

`tools/test-pull.cpp` starts a real PHP relay on a spare port, seeds it with two packs, points the installer at a temporary folder, and pulls. **17 checks.** It needs PHP; without it the test says so and stops rather than pretending to have passed.

- a first pull installs a template and a nested one, with the bytes compared
- the pack this client does not follow is left where it is, which is what lets one relay carry every venue
- a second pull fetches nothing and says it is up to date
- a file changed at the relay is fetched again, and the new bytes replace the old
- the check-in reached the relay, named only the pack this client holds, and reads as current with nothing failed
- a wrong token is reported as a token problem rather than as a transport error
- a relay that has stopped ends the poll instead of hanging, does not claim to be up to date, and leaves everything already installed exactly where it is

**Confirmed by breaking it.** Ignoring the pack filter cost two checks, including the one that says a venue does not take another venue's packs. Never recognising a file as unchanged cost the check that a second poll is quiet, which is the difference between a client that costs one request an hour and one that re-downloads everything forever.

That is 153 assertions across four suites. The route this was all built for now has a test that runs it.

### GitHub Enterprise Server, and the test it made possible
A self-hosted GitHub answers the same API at its own address, usually `https://github.example.com/api/v3`. The client addressed `api.github.com` directly, so anyone on one could not reach their own repositories at all.

The client takes a database setting, `RelayGitHubApi`. There is no field for it in Settings on purpose: almost nobody needs one, and empty means github.com. The push tool takes the whole thing in the address instead, `github:https://github.example.com/api/v3/owner/repo`.

That also removed the reason the GitHub route had no test. It could only ever be checked against the live API for the shape of its answers; the logic that reads a tree, works out which pack a path belongs to, compares Git blob hashes and fetches by digest had never executed.

`tools/test-github.cpp` runs it against `tools/mock-github.php`, which answers the three calls a client makes in the shapes github.com actually uses, including the directory entries a real tree carries and the token and user-agent rules it enforces. **23 checks**: a first pull with the bytes compared, a pack this client does not follow left alone, a second pull that fetches nothing, a changed file fetched again, a truncated tree refused rather than guessed at, a refused token named as one, and an unreachable API that ends the poll without claiming to be up to date or disturbing what is installed.

**One of those checks was worthless until it was tested.** The assertion that a repository's own `.github` is not installed as a pack passed with the rule that prevents it switched off, because the pack filter was keeping it out anyway. A section that follows every pack was added, and only then did removing the rule produce a failure. Refusing a truncated tree cost two checks when switched off, one of them the client reporting itself up to date while being wrong.

That is 176 assertions across five suites.

### How an address is read, and every URL built from one
The push tool was the last part with no test at all. One text field decides whether a push goes to a client on the next rack, a relay on the internet, or a Git repository, and the three are told apart by how the address is written. Misread it and a push goes somewhere it was not meant to, carrying a token in a header the far end was not expecting.

`tools/test-target.cpp` runs that. **53 checks**, no network and no files.

- `host:port` is a client, a missing port means 3000, and every endpoint on it is built from that
- an address with a scheme is a relay, so a URL is never split on the colon in `https:` and read as a host of "https"
- a relay URL that already carries a query gets its action added rather than a second question mark that would break it
- `github:owner/repo`, with and without a branch and a trailing slash
- `github:https://github.example.com/api/v3/owner/repo`, where only the last two segments are the repository, with and without a branch after it
- spaces are encoded in a path while the separators are not, on all three kinds
- each kind gets its own authentication and **not** the others': a client's push token never leaves as a bearer token, and a relay's upload token never goes out in a client's header

**Both address rules were checked by breaking them.** Not recognising an Enterprise address cost six checks; not telling a relay from a client cost fourteen.

### One Git hash function instead of two
The push tool links Qt and nothing else of this project, so it carried its own copy of the Git blob hash. Two identical copies today are two copies that can disagree later, and the symptom would be every client deciding every file had changed, on every poll, with nothing to show for it but traffic.

There is one now, in a header both include. It is the same function the tests already check against `git hash-object`.

That is 229 assertions across six suites.

### A push, driven through the buttons
Everything either side of a push had a test and the middle did not. Comparing a pack against a client, listing what would change, sending only the ticked rows, stamping each with the digest of what was read off disk, and the client checking that digest before writing: none of it had ever run together.

`tools/test-push.cpp` runs it the way an operator does. The window is built but never shown, the fields are filled in, and **Compare** and **Push ticked** are clicked. Nothing is invoked behind the interface, so the wiring is under test as much as the code. The receiving end is the real client server on a spare port with its templates in a temporary folder. **20 checks.**

- Compare lists both files as new, and never offers `project.js` at all
- Push puts them on the client with the bytes compared, and the rows say sent
- Compare again lists them as unchanged rather than sending them twice
- an edit on the dev machine shows as changed while the file beside it stays unchanged, and pushing replaces the old bytes
- a wrong token lists nothing rather than guessing, and the client keeps what it already had

**The tool's saved settings are redirected to a temporary file before the window is built**, so running the test never touches the operator's own list of clients.

**Three ways of breaking it, three sets of failures.** Offering protected files cost the check that `project.js` stays put. Never recognising a file as unchanged cost three. Sending a wrong digest cost five, and those five are the interesting ones: the file simply never arrives, because the client refuses to write bytes that are not what the sender said they were. The two halves are genuinely checking each other rather than both assuming.

That is 249 assertions across seven suites. Every part of this now has a test that runs it.

### One command to check the lot
Seven suites and a syntax checker meant eight commands, each taking about a minute, and knowing which of them needed PHP. That is a thing nobody runs.

```
python tools/check-all.py           everything, about four minutes
python tools/check-all.py --fast    no network needed, under a minute
python tools/check-all.py --list    what it would run, and why
```

It type-checks what changed, builds and runs every suite, adds up the assertions and exits non-zero if anything failed, so it can sit in a hook or a task. Results print as each finishes rather than in a lump at the end, because a four-minute run that shows nothing looks like a hang. A suite that needs PHP and cannot find it is reported as **skipped**, never as passed.

`tools/README.md` says what each piece is for, and carries the two habits this work earned:

**Break the thing you are testing and watch the test fail.** Twice here a check passed for a reason that had nothing to do with what it claimed to cover. Once a character rule rejected the input before the rule under test saw it; once a filter was quietly keeping a file out. Both looked fine until the code they were meant to guard was switched off and nothing happened.

**Assert on the result, not the return code.** The check that found the worst problem in this work was not "did that call return 400". It was a sweep of the whole sandbox asking whether any file had landed somewhere it should not have.

### Fixed: the window could end up taller than the screen
Dragging a panel tall stores its height, and that height is applied as a fixed one, which a panel cannot shrink below. Stacked in a column those heights become a floor the whole window cannot go under. There was already a guard setting a maximum size, but in Qt a layout minimum always beats a maximum, so it did nothing: the window was taller than the display and could not be dragged back.

It happened after dragging a panel tall on a large monitor and opening the client on a smaller screen, which is why it seemed to come and go.

Each column is now measured before anything is given out, and if it does not fit, every panel in it is reduced by the same proportion so the relative sizes survive. Nothing is touched when the column already fits, and nothing is touched when the screen height cannot be read. On a 1080p screen a real layout gives up three to four per cent; above about 1200 px nothing is reduced at all.

### The NDI panel had two names for one setting
Everything about the panel is keyed `NDI`, except the header colour overrides, which said `Ndi`. Those two agreed with each other so the feature worked, but under a key nothing else in the panel system would look for. They now match, any colour already chosen is carried across, and a stale height setting left by an older build is cleared out.

### Deciding centrally which machine gets which packs
One relay or repository already carried every project, and each machine took a subset. But that subset was set on the machine, so moving a project between venues meant getting to the venue.

The source now carries the answer, and **Assignments...** in the push tool is a grid for it: machines down the side, packs across the top, a tick where one takes the other. The `*` row is what a machine gets when it is not listed, which is how a shared pack reaches an estate without naming every box in it.

Rows come from three places, because none is complete on its own: machines that have checked in, machines already named in the file, and anything added by hand for a venue that does not exist yet. A machine named but never heard from is greyed, because a typo in a machine name looks exactly like a machine that is switched off.

**Nothing is written until Save**, and Save asks first when a row has nothing ticked, because that means those machines take nothing and it is one tick away from a venue quietly never updating again.

Precedence: a machine set to decide for itself always does, then the source when it names that machine, then the machine's own Packs field. A source that says nothing about a machine cannot change what that machine already does, so turning this on breaks nothing that already works. **Ignore what this machine is assigned** in a client's own settings takes that one machine back out of central control.

### Settings that are not squashed together
The group boxes built in code set four pixels between rows and no margins, so the first row of each started where the title already was and the two crowded each other. All nine now have room under the title and air between rows.

The Templates tab also says where a GitHub token comes from, under the Token field rather than in a tooltip nobody hovers, including that a client wants a read-only one and the push tool a second one with write.

### Somewhere to start
`tools/php/SETUP.md` is a walkthrough from nothing to a template arriving at a venue, for somebody who has not done this before. It picks a route, goes step by step, and every step ends with what you should see, so a step that quietly did not work is found there rather than five steps later. It ends with a table of symptoms and what each usually means.

The README also has a Getting started section for a fresh install: what a server is, where its media and template paths come from, and why the client has to be told the same two folders.

### Two panels that had never been written down
Both have been in the client since the panel system landed, and neither was ever described here. An audit of every commit since the fork against this document found them.

**The Http Log panel** shows what came back. Every HTTP GET and POST item the rundown fires is listed with its method, the address, the status code and the body that came back, and every playout action beside them with its device, channel and layer. When a template does not appear, this is where you find out whether the request went out, what answered, and what it said. It is a panel like any other: add it from the layout settings.

**Timed channel locks.** A locked channel refuses playout, which is what you want while somebody is working on air. A timed lock is the same thing with an end: set a duration and the channel unlocks itself when it runs out, counting down on the button so anyone looking at it can see how long is left. It saves the mistake that matters, which is locking a channel to protect a segment and then leaving it locked into the next one.

Every source touched between builds 154 and 161 now passes it, along with the generated moc for each.

`/templates/info` sits behind the same token as everything else: it says where templates are installed on that machine, which is not something to hand out unauthenticated.

### Where the data comes from
The resolver reads the **local cache service first** — the same one the templates race against — and falls back to the Sheets API when the cache has nothing, which is exactly what a cache miss means there. Any answer it gets from the API is written back to the cache in the shape the templates expect, so a resolve leaves the cache warmer than it found it. The project (spreadsheet id and key) is inferred from the template's own folder, so there is nothing to configure.

---

## Google Sheets Panel

A new **Google Sheets** panel (add it via the layout editor) connects to the same spreadsheets your standalone HTML templates use — and turns sheet data into operator buttons.

### Connection
- Scans every device's template path (and direct subfolders) for `project.js` — the same connection file the templates use (`spreadsheetId` + `apiKey`). Each folder found becomes a selectable project.
- Lists all tabs of the connected spreadsheet; opening a tab shows its actual rows — the operator sees real data (player names, numbers), not the row ids used to reference it.
- Manual refresh plus a selectable auto-refresh interval in the toolbar (10s / 30s / 1m / 2m / 5m / Off, persisted); the status label shows the last fetch time.
- **Show/hide columns per tab** (hamburger → **Columns**): tick the columns you want visible for the selected tab. Each project+tab keeps its own selection, so a wide sheet can show just the few columns the operator needs. Hiding is display-only — `{COLUMN}` placeholders in button data still resolve from hidden columns.

### Field modes from templates (debugDataModes)
Import Fields in the template inspector now also reads an optional `window.debugDataModes = { "f0": "integer", "align": "cycle:left|center|right" }` object from the template HTML — imported rows arrive with their edit mode (integer/decimal/boolean/color/cycle) and cycle values already set, so the operator gets the right controls without configuring each row. See `CLIENT_INTEGRATION.md` in the template project folder for the template-side contract.

### Operator buttons (extensions.json)
- Per-tab button definitions are edited in the panel (hamburger → **Edit Actions...**) and stored in `extensions.json` next to `project.js` — so they travel with the project folder, like everything else.
- **Row buttons** appear next to every data row. `{COLUMN}` placeholders in the label and data resolve per row — e.g. a 🟨 button per player sending `name={NAME},number={NUMBER},card=yellow`. Rows whose placeholders all resolve empty (set headers, spacers) get no buttons.
- **Standalone buttons** render once above the table — e.g. a VAR bar with `toggle` action (play on press, stop on second press, lit while on air).
- Actions: `play`, `stop`, `update`, `toggle`. Buttons fire CG commands directly on the project's device (channel/videolayer per button; templates always use flash layer 1), send data as componentData XML, and report into the Activity panel and playout log.
- The action editor's Template field is a dropdown of the client's template library (still free-typeable), and the panel's size mode is configurable in Settings → Layout → Panel Sizing (resizable by default).
- Toggles are exclusive per channel/layer: pressing a different row's toggle while one is on air **stops the current template first** (out animation plays) instead of overwriting it — press again to play the new one. Plain play/stop buttons on the same layer clear any stale lit toggle state.

---

## Gateways

Three gateway types for routing playback, focus, and commands across the rundown. Each gateway has an **entrance** and one or more **exits**. Place them anywhere in the rundown tree to create jump points.

### AutoPlay Gateway (Teal)
Routes autoplay chains to a different location. When playback reaches the entrance, it jumps to the items after the selected exit and continues the autoplay queue from there.
- Entrance shows exit selection buttons — choose which exit to jump to
- Exit shows a "Return" button for focus navigation back to the entrance
- First movie after exit uses direct PLAY, subsequent items use LOADBG AUTO
- Works across tabs if the exit is in a different rundown pane

### Focus Gateway (Purple)
UI-only navigation — jumps visual focus in the rundown tree without executing any playback commands.
- Entrance jumps focus to the selected exit
- Exit jumps focus back to the entrance
- Supports time-based conditional branching (same as other gateways)
- Works across tabs

### Command Gateway (Orange)
Relays playout commands to items after the selected exit. Supports **time-based conditional branching**: configure a time condition to automatically choose a different exit based on the current clock.
- Entrance evaluates the time condition and relays the command to the item after the matching exit
- Time condition operators: after, before, at or after, at or before, exact time
- If condition is false (or disabled), falls back to the default selected exit
- Works across tabs

### Time Conditions (All Gateways)
All three gateway types support time-based conditional branching on entrance items:
- Enable "time condition" in the inspector to choose a different exit based on the current clock
- Operators: "is after", "is before", "is at or after", "is at or before", "equals"
- Time includes hours, minutes, and seconds (HH:MM:SS)
- Inspector shows which exit is currently active (bold "Then use:" or "Else use:" label)
- Rundown entrance buttons highlight the effective exit in real-time, updating every 30 seconds
- Multi-line inspector layout: "If time at gateway" → operator dropdown → time spinboxes → Then/Else exits

---

## Trigger Banks

Nine configurable trigger banks (B1-B9) for quick item access:
- Assign any rundown item to a bank via the inspector
- Bank badge overlay shows the assignment on items
- Ctrl+1 through Ctrl+9 to trigger the assigned item
- Bank status displayed in the Trigger Banks panel
- All hotkeys are reconfigurable

---

## Linked Clones

Items can be "linked clones" — change a property on one clone and all siblings update automatically:
- Right-click > Create Linked Clone to link items
- Right-click > Unlink Clone to break the link
- Blue link badge on cloned items
- When a group drops to one member, it auto-unlinks
- Cut (Ctrl+X) and paste (Ctrl+V) preserves clone links
- Ctrl+Shift+V pastes copied items as new linked clones of the originals
- Regular copy/paste (Ctrl+C / Ctrl+V) creates independent items
- File save/load preserves links

---

## Preview Panel — Video Playback and Zoom

The Preview panel now supports local video playback and content zoom:

### Video Playback
- When a video item is selected and the server has a **Media path** configured, the preview plays the actual video file locally
- Transport bar at the bottom: play/pause button, seek slider, and time display (M:SS / M:SS)
- Transport bar only appears when a local video file is found — otherwise falls back to the static thumbnail
- Pauses at the first frame on load — click play to start

### Media Path (Per-Device Setting)
- New **Media path** field in the server configuration dialog (alongside the existing Template path)
- Points to the folder where CasparCG media files are stored (local or network path)
- The client resolves CasparCG media names to actual files by scanning for matching filenames with common video extensions (.mov, .mp4, .mxf, .avi, .mkv, etc.)
- Optional — without a media path, the preview works as before (static thumbnails only)

### Content Zoom
- **Double-click** the preview content to zoom in to 150%
- **Double-click again** to return to fit-to-panel view
- When zoomed: **drag to pan** the view, blue "150%" badge in the corner, blue border around the content
- Works for both video playback and still thumbnails

---

## Preview Mode

Two ways to preview items on a separate channel:
- **Hold modifier key** (Shift, Ctrl, or Alt — configurable) while pressing any playout hotkey to redirect to the preview channel
- **Toggle PVW button** in the Server Status panel for persistent preview mode (green when active)
- Affects Play, PlayNow, and Load commands on Movies, Stills, Audio, and Templates
- **Clean override**: Preview channel override is temporary — it does not affect the inspector display, saved rundown files, or subsequent OSC-triggered playback. Manually changing the channel in the inspector clears any lingering preview override

**Settings:** Preview modifier key selector in Hotkeys tab, configurable toggle hotkey (default Ctrl+P). Preview channel is configured per server in the device settings (requires restart).

---

## Autostep Mode

Global toggle that automatically advances selection to the next item after pressing Play (F2) or Play Now:
- **STEP button** in the Server Status panel (dark purple when active, grey when inactive)
- **Configurable hotkey** (default Ctrl+Shift+N) to toggle on/off
- **Purple background highlight** on the autostep item, alongside the normal green selection indicator
- Works on all item types — not limited to groups
- Group-aware stepping: expanded groups step into first child, collapsed groups stay selected
- Show/hide STEP button toggle in Settings > General

Replaces the old per-group AutoStep checkbox which has been removed.

---

## Layout System

### Dynamic Panel Placement
The interface is divided into configurable columns. Each panel widget can be placed in any column via the **Layout Editor** in Settings > Layout:
- Drag panels between columns to rearrange
- Available panels: AudioLevels, Library, Preview, Live, NDI, iNews, StatusBar, Clock, ServerStatus, Activity, TriggerBanks, Inspector
- Up to 4 side panels plus the main rundown area
- All 4 panels always visible in the editor — empty panels are automatically hidden at runtime

### Panel Sizing
Every panel has a **Size Mode** with three mutually exclusive options:
- **Fixed**: Panel uses its natural fixed height
- **Resizable**: Drag handle appears below the panel for manual height adjustment — custom height persists across restarts
- **Expanding**: Panel stretches to fill all remaining vertical space in its column

Change settings instantly via the hamburger menu on each panel (Size Mode submenu), or configure all panels at once in Settings > Layout > Panel Sizing. Changes apply immediately without restart.

Default: Library/Inspector/Activity are Expanding, Preview/Live/NDI are Resizable, all others are Fixed.

### Per-Panel Anchor
Each panel has its own anchor setting (Up or Down) in its hamburger menu. Setting a panel to "Down" inserts stretch above it, pushing it and any panels below it to the bottom of the column. Default is Up for all panels.

### Live Panel Reordering
Panels can be moved via the **Move** submenu in their hamburger menu — no restart needed:
- **Up/Down**: Reorders the panel within its column
- **Left/Right**: Transfers the panel to an adjacent side panel column, skipping the main window area
- Actions are disabled when the move isn't possible (e.g. no panel to the left, already at top of column)

### Panel Span
A panel can span across multiple adjacent side panel columns, never crossing the main window area. The span is incremental — each "Span More" click adds one column, "Span Less" removes one. Direction is automatic: expands right first, then left if right is exhausted.
- **Span**: Start spanning into the next available column (right preferred, then left)
- **Span More**: Expand span by one more column
- **Span Less**: Shrink span by one column
- **Unspan**: Remove all spanning (shown when span is exactly 1 column)
- Non-span widgets stay in their original sub-columns above and below the spanning widget
- Spanned panels support all size modes including Resizable — the drag handle appears below the full-width span widget
- Move Left/Right is disabled while a panel is spanning

### Collapsible Panels
Every panel has a hamburger menu (≡) in its header with collapse/expand, size mode, anchor, and move options. Click collapse to fold a panel down to just the tab bar (25px).

### Library Panel
Expands to fill all available vertical space in its column by default. Filter bar and device filter sit below the media list.

---

## Clock Widget

Dual-timezone clock displayed as two side-by-side or stacked cards with LCD segment-style digits:
- Each card uses QLCDNumber with flat segment rendering and shadow digit outlines (matching the iNews countdown style)
- GMT offset label above each clock (e.g. "GMT+1", "UTC"), updates dynamically for DST — labels can be hidden
- Clock 1 and Clock 2 digit colors and shadow color are configurable in Settings
- Side-by-side or stacked layout (configurable in Settings)
- Collapsible to just the header bar

**Settings:** Two timezone dropdowns in Settings > Layout with descriptive labels (e.g. "Europe/Amsterdam (GMT+1)"). Defaults to Local and UTC.

**Quick timezone change:** Right-click the clock panel → "Clock 1 Timezone" or "Clock 2 Timezone" to change timezone on the fly (Local, GMT-12 to GMT+14). Takes effect immediately without restart.

---

## Status Bar

Foldable message log at the bottom of any panel:
- Shows the latest status message by default (single line)
- Click the arrow to expand and see the last 10 messages with timestamps
- Error messages highlighted in red
- Messages auto-clear after their timeout

---

## Status Panel

### Server Status
Shows each connected CasparCG server with:
- Device name, connection status dot (green/red), media availability dot
- Connect/Disconnect button per device
- Channel lock grid for locking specific channels per device or globally
- Device names truncate when the panel is narrowed

### Activity Monitor
Live "now playing" display with:
- Progress bar per video with countdown timer
- Type badges (VIDEO, IMAGE, TEMPLATE, etc.) with color coding
- Loop and pause indicators
- Channel grouping with headers
- Automatic cleanup of stale entries
- Static entries for non-video items (stills, templates)
- Text labels truncate gracefully when the panel is narrowed — badges stay fixed size

### Activity Panel Auto-Grow
The Activity panel now auto-grows to fit its content by default (matching the Trigger Banks panel behavior), instead of always expanding to fill available space. The Size Mode menu (Fixed/Resizable/Expanding) is still available for users who prefer the old behavior.

A **"Show No Activity"** toggle in the Activity hamburger menu shows a placeholder label when there are no active entries, preventing the panel from collapsing to zero height.

### Minimum Height (Activity & Trigger Banks)
Both the Activity and Trigger Banks panels now have a **Minimum Height** setting in their hamburger menu, with presets (None, 30px, 50px, 80px, 100px). This sets a floor height that the panel won't shrink below, even when empty. The minimum height is respected across collapse/expand, layout rebuilds, and all size modes.

### Trigger Banks Display
Shows current bank assignments (B1-B9) with item name and channel badge. Bank icons light up orange when assigned. Text labels truncate when the panel is narrowed instead of forcing a minimum width.

---

## Split View Rundowns

The rundown area supports horizontal or vertical splitting:
- Secondary tab widget for the split pane
- Double-tap arrow key to switch focus between panes
- Drag tabs between panes
- **Ctrl+]** / **Ctrl+[** to cycle between tabs in the active pane

---

## Configurable Hotkeys

All playout actions can be remapped in Settings > Hotkeys:
- Each action has a primary and alternate hotkey
- Default F1-F12 mapping: Stop, Play, PlayNow, Load, PauseResume, Next, Update, Invoke, Preview, Clear, ClearVideoLayer, ClearChannel
- Bank triggers: Ctrl+1 through Ctrl+9
- Preview toggle: Ctrl+P
- Autostep toggle: Ctrl+Shift+N
- Restore defaults button to reset all hotkeys

---

## Settings Options

### General
| Setting | Default | Description |
|---------|---------|-------------|
| Start Fullscreen | Off | Launch in fullscreen mode |
| Theme | Flat | Visual theme (Flat or Curve) |
| Font Size | 12 | UI font size |
| Drop Frame Notation | Off | Use drop-frame timecode format |
| Auto Refresh Library | Off | Periodically refresh the media library |
| Refresh Interval | 60s | How often to auto-refresh |
| Thumbnail Tooltips | On | Show media thumbnails on hover |
| Reverse OSC Time | On | Count down instead of up |
| Polling Rate | 200ms | How often OSC data is batched and dispatched to the UI (50–1000ms). Lower = smoother audio meters, higher = less CPU |
| Disable In/Out Points | On | Ignore in/out point markers |
| Mark Used Items | Off | Visually mark items after they play |
| Freeze on Load | Off | Show first frame when loading |
| Preview Border | On | Show border on preview channel |
| Duration Format | Human Readable | Display format for duration/delay labels (Human Readable or Timecode) |
| Show STEP Button | On | Autostep mode toggle in Server Status |
| Show PVW Button | On | Preview mode toggle in Server Status |
| Show Servers | On | Device connection rows in Server Status |
| Show Channel Locks | On | Per-channel lock grid in Server Status |
| Show Channel Headers | On | Channel group labels in Activity |
| Show Bank Icons | On | B1-B9 quick-status row in Trigger Banks |
| Disconnect Mode | Ask | Confirm before disconnecting (ask/hidden/direct) |
| Template Preview Freeze | Off | Load template without playing on F8 preview |
| Activity Grow Mode | Grow | Activity panel fills space (grow) or fits content (fit) |
| Show No Activity | On | Show "No Activity" placeholder when the Activity panel is empty |
| NDI Outputs | 1 | Number of NDI viewer outputs in the NDI panel (1-9) |
| Show Second Clock | On | Display second clock in Clock widget |
| Clock Timezone 1 | Local | Primary clock timezone |
| Clock Timezone 2 | UTC | Secondary clock timezone |

### Layout
| Setting | Default | Description |
|---------|---------|-------------|
| Layout Editor | — | Visual drag-and-drop panel arrangement |
| Panel Sizing | — | Per-panel sizing mode (Fixed, Resizable, Expanding) for all 12 panels |
| Per-Panel Anchor | Up | Each panel can independently anchor Up (top) or Down (bottom of column) via its hamburger menu |
| Panel Move | — | Move Up/Down/Left/Right in the hamburger menu for live reordering without restart |
| Panel Span | Off | "Span Right" in hamburger menu to span a widget across two adjacent side panel columns |

### Customization
Status panel visibility toggles grouped in their own section below the database path:
- Show PVW/STEP buttons, Servers, Channel Locks, Channel Headers, Bank Icons
- Disconnect Mode, Activity Grow Mode
- Show Second Clock, Clock Timezone pickers

**Channel Colors:** Four sliders to tune the channel color formula — Color spacing (golden angle), Color offset, Saturation, and Lightness. Five preview boxes show Ch1-Ch5 colors in real time as you adjust.

**Interface Colors:** Seven color pickers to customize accent colors throughout the UI:
- PVW button active color
- STEP button active color
- Preview mode window border color
- Autostep item highlight color
- Active play indicator color (overrides channel-based coloring when set)
- Library section line color (the green indicator under the open Library section header)

**Header Colors:** Three customizable color properties per panel — Line (border under tab bar), Block (selected tab background), and Text (selected tab text color):
- Master row sets default colors for all panels
- Active Rundown row overrides the master for the Rundown tabs
- "Customize individually" checkbox reveals per-widget rows for the remaining 9 panels (Library, Inspector, Audio Levels, Preview, Live, Clock, Server Status, Activity, Trigger Banks)
- Each row has 3 clickable color swatches (Line, Block, Text) — click to open color picker
- Changes apply live — no restart needed
- Database version 233: migrates old WidgetHeaderColor/TabColor keys to the new Header{Line,Block,Text} system

**Clock:** Three color pickers and two toggles for the clock widget:
- Clock 1 digit color, Clock 2 digit color, Shadow digit color
- Show/hide timezone labels above each clock
- Stack clocks vertically or keep side-by-side

### Hotkeys
All playout actions with primary + alternate bindings (see Configurable Hotkeys above).

---

## Database

Current version: **236**. All settings are stored in SQLite and persist across restarts. Migration scripts handle upgrades from any previous version automatically.

- **Device addition error feedback**: Adding a device now shows a warning message if the database insert fails, instead of silently closing the dialog with no device added

---

## Multi-Row Template Invoke

The template inspector's invoke field now supports multiple invoke labels, and lives in its own collapsible **Invoke** section in the inspector (between Output and Template):
- **Separate inspector section**: Invoke has its own collapsible tree item, keeping it visually distinct from template data fields
- **Add/remove rows** with [+] and [-] buttons
- **Column headers** ("Function", "Call", "Key") label the text field, play button, and F7 radio columns
- Each row has a **text field**, a **play button** that triggers that specific invoke immediately, and an **F7 radio button** to select the default
- The radio-selected invoke is used for **F7 hotkey** and **OSC invoke** commands
- **Backward compatible**: old rundowns with a single invoke load as one row; new format saves as a list

---

## Template Data Edit Mode

Each row in the template key-value table has its own **data edit mode**, set via the Mode dropdown in the add/edit dialog:
- **Text** (default): Values are plain text — no interaction column
- **Integer**: Up/down arrows increment/decrement by 1. Zero-padded values (e.g. "007") automatically preserve their padding width
- **Decimal**: Up/down arrows increment/decrement by 0.1
- **Boolean**: Toggle indicator in the interaction column — click to switch between "true" and "false"
- **Color**: Color swatch in the interaction column — click opens a color picker, stores the value as "#rrggbb"
- **Cycle**: Up/down arrows cycle through a pipe-separated list of values (e.g. "left|center|right"). The values are configured via a "Values" field that appears in the edit dialog when Cycle mode is selected

The interaction column is narrow (20px) and separate from the value text, so clicking never accidentally opens the edit dialog. The mode is saved per-row in the rundown file and is not sent to CasparCG.

**Import Fields merges instead of replacing**: importing from `window.debugData` keeps the rows you already have — existing keys keep their current value and position (declared modes refresh from the template), and only missing keys are appended. Deleting a key and importing again simply brings that key back.

---

## Template Newline Behavior

Per-item dropdown in the template inspector ("Newline behavior") controls how newlines in template data values are encoded when sent to CasparCG:
- **Ignore**: Strips newlines from values — no line breaks in output
- **innerText**: Encodes newlines as `&#10;` XML entities — works with templates that use `innerText` to render values
- **innerHTML** (default): Replaces newlines with `<br>` tags — works with templates that use `innerHTML` to render values

The setting persists in rundown files. Default is innerHTML for maximum compatibility with common HTML templates.

---

## NDI Panel

A new NDI (Network Device Interface) panel replaces the old VLC-based Live stream panel as the default video monitor. NDI provides low-latency video over the local network directly from CasparCG's built-in NDI output.

### Multi-Output Grid
The NDI panel supports up to 9 simultaneous video outputs in a configurable grid layout:
- Choose 1–9 outputs via the hamburger menu or Settings > General > NDI Outputs
- Select from valid grid arrangements (e.g. 1x1, 2x2, 3x2, 3x3) via the Layout submenu
- Each viewer connects independently to any discovered NDI source

### Source Discovery
NDI sources are discovered automatically on the local network:
- Right-click any viewer > Set Source to see all available NDI sources
- Sources update in real-time as CasparCG servers come online or go offline
- Source assignments persist across restarts

### Audio
Each viewer receives and plays audio from its NDI source:
- Audio plays through the default system output device
- Per-viewer mute toggle via right-click context menu
- Mute All / Unmute All in the hamburger menu

### Connection Status
Visual overlay indicators show the current connection state:
- "Connecting..." while waiting for the first frame from a source
- Source name overlay once video is received — it auto-hides after 5 seconds to keep the picture clean (status/error overlays always stay visible)
- "Signal Lost" with red background when the NDI source goes offline
- Automatic recovery when the source comes back — no need to reconnect manually

### 16:9 Grid Scaling
The viewer grid maintains 16:9 aspect ratio per cell, scaling to fill the available panel space while centering the grid. Changing the layout or resizing the panel recomputes the grid to use space optimally without stretching. Old stretch factors are properly cleared when switching layouts so viewers always fill their cells correctly.

### Performance Settings
Three configurable quality settings to reduce CPU and network usage, accessible from the hamburger menu (Quality submenu) and Settings > General:
- **Bandwidth**: High (full quality) or Low (reduced resolution from the NDI sender) — biggest single performance win
- **Frame Rate Limit**: Off / 30 / 15 / 10 / 5 fps — skips frames in the capture loop
- **Scaling**: Smooth (bicubic) or Fast (nearest-neighbor) — reduces CPU cost of display scaling

Bandwidth changes reconnect the viewer; FPS limit and scaling apply instantly. Combined Low + 15fps + Fast can reduce CPU ~70-80%.

### Panel Features
- Collapsible to just the header bar (same as other panels)
- Resizable height via drag handle at the bottom edge
- Movable to any panel column via the Layout Editor in Settings
- If NDI Runtime is not installed, shows a clear error message with download link

### Requirements
- Install the free [NDI Runtime](https://ndi.video/for-developers/ndi-sdk/) on Windows
- Configure CasparCG server with an NDI consumer per channel

The old VLC-based Live panel remains available in the Layout Editor but is hidden by default.

---

## Activity Progress Bars for Stills and Audio

The Activity panel now shows progress bars for stills (with duration) and audio items, not just movies.

- **Stills with duration**: When a still image plays with a duration set, a timer-based progress bar counts down in the Activity panel, matching the configured duration
- **Audio files**: CasparCG sends real-time playback position via OSC for audio files. The Activity panel now subscribes to this data and shows a live progress bar with countdown, just like movies
- **Type badges**: Each progress entry displays the correct type badge (IMAGE, AUDIO, VIDEO) with its own color, rather than always showing VIDEO

---

## Performance Panel

A new Performance panel shows real-time system statistics in 3 compact rows, updated every 2 seconds:
- **Client** — Combined CPU usage and RAM of the CasparCG Client process, shown as "X.X% | XXX MB" with individually colored values
- **Server** — Aggregated CPU and RAM across all running CasparCG server processes, in the same format. Always visible (shows "--" when no server is detected)
- **System** — Combined system memory and CPU usage, shown as "XX% | XX.X%" (memory | CPU) with individually colored values. Tooltip labels each percentage

All values are color-coded independently: green (normal), yellow (moderate), red (high). The panel is collapsible, resizable, and movable to any column via the Layout Editor.

---

## Embedded Transforms

Content items (Template, Movie, Still, Audio, HTML, Image Scroller) can carry their own MIXER transform properties directly on the item — no more separate transform items and visual jumps on-air.

### How It Works
- Select any content item and open the **Embedded Transform** section in the inspector (it starts collapsed to keep the inspector compact — click its header to expand; the toggle sticks for the session)
- Check any transform property to enable it: Fill, Opacity, Rotation, Anchor, Crop, Clip, Brightness, Contrast, Saturation, Volume
- On play, all enabled transforms are sent as `MIXER ... DEFER` commands, then `MIXER COMMIT`, then the content plays — fully atomic, no visual jumps
- Optional **Entrance Animation**: animate a property (e.g. fade opacity from 0 to 1) after the content appears

### Rotation & Anchor
- Enabling Rotation automatically enables Anchor (pivot point, default center 0.5/0.5)
- Anchor is always sent before Rotation; if Rotation is set without an explicit Anchor, center is used automatically

### Absorb & Extract
- **Right-click > Absorb Transforms**: reads sibling standalone transform items on the same channel/layer into the content item's embedded transforms, removes the absorbed items
- **Right-click > Extract Transforms**: creates standalone transform items (with defer=true) + a Commit item from the embedded transforms, then clears them

### Compatibility
- All 19 standalone transform items remain fully functional — zero changes
- Old rundown files load without changes
- Embedded transforms are serialized, cloned, copy/pasted, and undo/redo'd through the existing property pipeline

---

## HTTP Response & Playout Log Panel

The Log panel displays HTTP responses and playout actions in a unified timeline.

### HTTP Responses
- Timestamp, HTTP method (GET/POST), request URL, and HTTP status code
- Response body (truncated to 500 characters for long responses)

### Playout Action Log
- Every playout action (Play, Stop, Load, Clear, etc.) is logged with timestamp, action type, item label, device, channel, and video layer
- OSC-triggered and bank-triggered actions are tagged with "(OSC)"
- Clear commands (F10/F11/F12) are logged even on groups
- **"Log Playout Actions"** toggle in the panel hamburger menu — enabled by default, persisted to DB

The panel auto-scrolls, is collapsible, resizable, and movable to any column

---

## Audio Meters Per Channel

The Audio Levels panel now shows separate tabs per CasparCG channel:
- Each tab displays 8 audio level meters for one channel
- Tab label shows "CH 1", "CH 2", etc. (or "DeviceName CH1" with multiple servers)
- Blue selected-tab indicator matching the main window style
- Automatically rebuilds when devices connect/disconnect

---

## Rundown Lock Toggle

Any rundown tab can be locked to prevent accidental edits. Repository rundowns are locked automatically on load.
- **Lock Rundown** checkbox in the main menu bar (Rundown > Lock Rundown) and the rundown hamburger menu — both stay in sync
- When locked: drag-and-drop, delete, paste, cut, group/ungroup, and move operations are all disabled
- Copy and playback commands still work on locked rundowns
- Locked tabs show a lock icon (🔒) prefix in the tab title for quick visual identification
- Lock state is per-tab and syncs correctly when switching between tabs or panes
- **Override repository auto-lock**: Manually unlocking a repository rundown overrides the auto-lock — it stays unlocked even when repository events fire
- **Channel locks respect OSC triggers**: OSC-triggered and bank-triggered playout commands now check channel locks before executing

---

## Rundown Search

Press **Ctrl+F** to open a search bar above the rundown tabs. Search spans **all open rundowns** across all tabs and both split-view panes.
- Searches item names and labels (case-insensitive) across every open tab
- Match counter shows "1 of N" with ▲/▼ buttons to cycle through results
- Each match is selected and scrolled into view automatically
- Automatically switches to the correct tab and pane when navigating results
- Press Enter for next match, Escape to close the search bar
- Searches inside groups (children are included in results)
- Results refresh automatically when split view is toggled or closed

---

## Library Channel/Layer Selector

Channel (CH) and Layer (L) spinboxes at the top of the Library panel set the default channel and video layer for items dragged into the rundown. Positioned just below the tab header for quick access.

---

## Panic Button

Press F1 rapidly twice within 500ms to trigger a panic clear — stops all items on all channels across all connected devices. A single F1 press still works as normal Stop.

---

## Server Process Management

New **Server path** field in the device configuration dialog. Points to the CasparCG server executable for optional process management features.

---

## Light Theme

A new Light theme option alongside the existing Flat and Curve themes. Lighter backgrounds and adjusted contrast for daylight environments.

---

## UI Polish

- **Fixed-height panels**: Clock and StatusBar no longer grow to fill available column space when expanded — they take only the height they need
- **Narrower device filter**: Library device filter dropdown reduced from 120px to 90px, giving the search bar more space
- **What's New dialog timing**: The What's New dialog now appears after the interface has fully rendered, instead of showing over an unloaded window
- **Settings General tab dynamic layout**: The General tab now uses a grid layout instead of absolute pixel positioning. All settings flow naturally — adding or removing an option no longer requires shifting every coordinate below it
- **Resizable Preview & Live panels**: Content (VLC stream / thumbnail) now scales to fill the panel size while maintaining aspect ratio, instead of being locked to a fixed 287x161 box. A subtle resize handle bar at the bottom edge provides a visual drag indicator
- **Template inspector button sizes**: The [+], [🗑], [▶], and [F7] buttons in the template inspector are now 40px wide instead of 28px, making them easier to click. The Import Fields button properly sizes to fit its text
- **Aligned bottom row columns**: Rundown items with device/channel info use a fixed 5-column layout on the bottom row (device, layer, delay, duration, UID). Columns align vertically across different item types
- **Status icons overlay**: Status icons (GPI, Disconnected, AutoPlay, Thumbnail, OscTime) now float as an overlay on the right side of rundown items instead of taking layout space
- **Removed flash layer**: The unused flash layer property has been removed from the template widget
- **Smooth scroll**: The rundown tree now scrolls pixel-by-pixel instead of jumping item-by-item
- **Paste Properties (No Data)**: New context menu option that pastes command settings but preserves existing template data
- **Drop indicator line**: A blue horizontal line shows the exact drop position when dragging items
- **Last action in statusbar**: Optional setting to display the most recent playout action in the statusbar
- **Group channel badge**: Group items show a channel color badge reflecting their children's channel (single = color, mixed = neutral dark)
- **Audio panel blue tab indicator**: Selected channel tab in audio levels has a blue bottom border
- **Library CH/L selectors at top**: Channel and layer spinboxes positioned below the tab header
- **Panel collapse persistence**: Collapsed panel state survives layout rebuilds and restarts
- **Window size constraint**: Application window clamped to available screen geometry on startup and after layout changes
- **Panel resize handle clamped**: Drag handle stops at the window bottom edge
- **Inspector channel spinbox**: Allows values up to the item's current channel even before server reports formats
- **Inspector channel reliability**: Channel, videolayer, delay, and duration changes always write to the primary command directly
- **Audio panel tab style**: Selected channel tab uses the same full blue background as rundown tabs
- **Library "Default" label**: Channel/layer selectors at top of library have a "Default" label
- **Inspector delay/duration minimum**: Clamped to minimum 0 — negative values no longer possible
- **Solid color fix**: SolidColor command handles both #AARRGGBB and #RRGGBB formats correctly
- **Wheel-safe inspector inputs**: spin boxes, dropdowns and sliders in the inspector (and the Simple Inspector) only react to the mouse wheel after being clicked — scrolling the inspector page no longer changes values by accident; the wheel scrolls the page instead

---

## Performance

- **OSC subscription registry**: Replaced broadcast-based OSC message delivery with a centralized routing registry. Previously every OSC message was delivered to every subscription object in the application (up to 16,000 with large rundowns), where each one performed a string comparison. Now a QHash-based registry does O(1) path lookup and notifies only matching subscribers. Eliminates ~99.9% of per-message overhead.
- **OSC subscription memory leak fix**: Fixed a memory leak where changing a rundown item's channel or video layer would disconnect old OSC subscriptions but not delete them. The leaked objects continued to receive and process every OSC message. Now properly deleted before creating replacements.
- **Lightweight active animation**: The play animation (white flash to channel color) now uses QPalette instead of setStyleSheet. Previously each animation frame triggered full CSS parsing and style recalculation on the widget. QPalette directly sets the background color with no parsing overhead.
- **Configuration cache**: Frequently-read database settings (DelayType, MarkUsedItems, UseFreezeOnLoad, ReverseOscTime, ShowThumbnailTooltip) are now cached in memory. With 500 items, this eliminates ~1,500 SQL queries during rundown load. Cache auto-refreshes when settings change.
- **Pixmap cache**: GPI connection status pixmaps (GpiConnected/GpiDisconnected) are loaded from resources once and shared across all widgets, instead of being re-decoded from PNG on every state change.
- **Preview border no longer causes UI slowdown**: Activating PVW mode (via button or modifier key) no longer degrades GUI performance. The border now uses a lightweight overlay frame instead of applying a stylesheet to the root widget, which had forced Qt to recalculate styles for every child widget on every repaint.
- **Color cache**: All custom color settings are cached in memory at startup, eliminating repeated database reads during active animation, autostep highlighting, and clock rendering.

---

## Stability Fixes

- **Fixed crash on group selection**: OscSubscription pointers were not nulled after deletion, causing double-free / heap corruption. Fixed across all 40 rundown widget types.
- **Fixed ActiveAnimation use-after-free**: Animation target widget pointer changed from raw pointer to QPointer.
- **Clone sync signal cascade reduced**: Signals blocked on sibling commands during property sync.
- **AutoPlay null guard**: Added null checks in the autoplay handler for source widget and dynamic_cast results.
- **Fixed active color indicator**: Switched from QPalette to setStyleSheet for compatibility with global CSS rules.
- **Fixed device add dialog**: Adding a server no longer fails with "parameter count mismatch" when the ServerPath column is missing.
- **Fixed preview channel override leak**: Preview override no longer leaks into inspector display, saved files, or OSC-triggered playback.
- **Fixed OSC trigger channel override**: OSC/bank triggers clear any lingering preview override before executing.
- **Fixed crash editing template key/values after the item changed**: the template, HTTP GET/POST and Simple inspectors kept a raw pointer to the selected item's command; if that item was deleted or rebuilt (move, undo, reload), the next click in the key/value table dereferenced freed memory and crashed the client. All cached command pointers are now QPointers that auto-null, with guards on every write-back path.
- **Fixed Activity panel keeping rows after Clear CH / Clear VL**: clearing a channel (F12) or videolayer (F11/F10) now broadcasts the clear, so the Activity panel removes every row on the affected channel/layer — play rows, progress rows and auto-loop countdowns alike. The same broadcast also stops other items' delayed auto-loops on the cleared channel, matching the Clear Output panic item's behavior.
- **Fixed spanned panels not collapsing correctly**: a spanned panel's height lives on its layout-grid row, which never followed the panel's collapse. Collapse/expand on a spanned panel now rebuilds the layout: collapsed rows shrink to the compact header (resize handle hidden), expand restores the saved span height.
- **Fixed a crash after deleting rundown items or pressing undo while Simple Mode is open.** The grid held pointers to tree items that had been destroyed — deletion frees them, and an undo rebuilds every item from scratch — so pressing a key could dereference freed memory. Both paths now tell the grid, which drops the stale pointers and rebuilds (or defers the rebuild if it is hidden).
- **Fixed Simple Mode keys not updating when items are deleted from the rundown.** The removed item's key now disappears, and the remaining keys keep their positions.
- **Fixed invoke dropdowns showing the previous item's functions**: the Function dropdowns kept the functions discovered for the previously selected template. They now re-scan the selected item's own template on every selection.

---

## Compatibility

- All changes are backward-compatible with existing rundown XML files
- New properties use sensible defaults when missing from old files
- Database auto-migrates on first launch after update

---

## Stats

**214 files changed** — 36,522 insertions, 29,792 deletions
