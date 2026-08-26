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

## Dialog Positioning Fix

The key/value add/edit dialogs (template data, HTTP GET/POST data) position themselves near the cursor — they are now clamped to the visible screen area, so they can no longer open off-screen when the inspector sits near a screen edge.

---

## Google Sheets Panel

A new **Google Sheets** panel (add it via the layout editor) connects to the same spreadsheets your standalone HTML templates use — and turns sheet data into operator buttons.

### Connection
- Scans every device's template path (and direct subfolders) for `project.js` — the same connection file the templates use (`spreadsheetId` + `apiKey`). Each folder found becomes a selectable project.
- Lists all tabs of the connected spreadsheet; opening a tab shows its actual rows — the operator sees real data (player names, numbers), not the row ids used to reference it.
- Manual refresh plus a selectable auto-refresh interval in the toolbar (10s / 30s / 1m / 2m / 5m / Off, persisted); the status label shows the last fetch time.

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
- Source name overlay once video is received
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
- Select any content item and open the **Embedded Transform** section in the inspector
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

---

## Compatibility

- All changes are backward-compatible with existing rundown XML files
- New properties use sensible defaults when missing from old files
- Database auto-migrates on first launch after update

---

## Stats

**214 files changed** — 36,522 insertions, 29,792 deletions
