"""Every feature this fork added is described in the changelog.

CHANGES.html is what the What's New dialog shows, so a feature missing from it is a
feature nobody is told about. That had happened: ten builds of work, and two panels
that had been in the client since the panel system landed and were never written
down anywhere a user would look.

The check is deliberately crude. Each entry names a commit, what a user gets from
it, and words that would only appear in the changelog if that thing is actually
described. It cannot tell a good description from a bad one, only that something is
there.

    python tools/check-changelog.py

Add an entry here whenever a commit gives somebody something they could notice.
"""

import io, os, subprocess
os.chdir(r"C:\Users\nvanl\casparcg-client")

md = io.open('CHANGES.md', encoding='utf-8').read().lower()
html = io.open('CHANGES.html', encoding='utf-8').read().lower()

# Each entry: commit, what it gave a user, and words that would only appear in the
# changelog if that thing is actually described there.
FEATURES = [
    ('20b009a0', 'nestable groups',            ['nestable group', 'inner group']),
    ('20b009a0', 'cross-tab search',           ['search bar', 'cross-tab']),
    ('20b009a0', 'undo/redo',                  ['undo']),
    ('20b009a0', 'split view',                 ['split view']),
    ('541dd8ec', 'inspector gateway/transform', ['transform']),
    ('541dd8ec', 'invoke editing',             ['invoke']),
    ('ff4e8607', 'OSC subscriptions',          ['osc']),
    ('ff4e8607', 'HTTP logging',               ['http log', 'httplog']),
    ('3fc72de5', 'NDI panel',                  ['ndi']),
    ('3fc72de5', 'panel system / spanning',    ['span']),
    ('3fc72de5', 'trigger banks',              ['trigger bank']),
    ('0175dbc1', 'UI themes',                  ['theme', 'channel badge']),
    ('c74e40f5', 'timed locks',                ['timed lock', 'locked channel']),
    ('c74e40f5', 'template autoplay',          ['autoplay']),
    ('9e3fd4ca', 'per-item auto-loop',         ['auto-loop', 'autoloop']),
    ('9e3fd4ca', 'Stop All Auto-Loops',        ['stop all auto']),
    ('6f1fb262', 'Google Sheets panel',        ['sheets panel', 'google sheets']),
    ('2e8b8a7a', 'Simple Mode grid',           ['simple mode']),
    ('51042d6a', 'client-hosted sheet cache',  ['host the cache', 'sheet cache', 'cache in this client']),
    ('51042d6a', 'strain measuring',           ['strain']),
    ('fc4dde3f', 'expected-result box',        ['expected-result', 'expected result']),
    ('50f9895b', 'cache stoplight',            ['stoplight', 'cache light', 'server status']),
    ('34a82d7e', 'NDI source restore',         ['ndi source', 'source it had last']),
    ('04a2f614', 'per-key budgets',            ['per key', 'budget', 'keyid']),
    ('2d14f1ca', 'the PHP relay',              ['relay']),
    ('29998ad7', 'push packs to a client',     ['template push', 'push packs']),
    ('ff730501', 'pull from a relay',          ['pull packs', 'pull route', 'pulling from']),
    ('4ccf5728', 'retries',                    ['retry', 'retrying']),
    ('13d96b9d', 'drift / only there',         ['only there', 'drift']),
    ('902d3813', 'GitHub as a source',         ['github']),
    ('902d3813', 'relay check-ins',            ['check in', 'check-in', 'checked in']),
    ('638f3d39', 'relay self-test',            ['selftest', 'self-test']),
    ('a7b12d00', 'venue says why it is stuck', ['why it is stuck', 'failing']),
    ('0af8f27a', 'GitHub Enterprise',          ['enterprise']),
    ('a0b00c0f', 'window taller than screen',  ['taller than the screen']),
    ('667f966e', 'NDI one spelling',           ['two names for one setting', 'ndi panel had two names']),
    ('005c5b4f', 'settings spacing',           ['squashed together', 'room under', 'group title']),
    ('005c5b4f', 'GitHub token help',          ['where a github token comes from', 'token comes from']),
    ('80b3b4b7', 'central assignments',        ['assignment']),
    ('d9ab0e47', 'assignments grid',           ['grid for it', 'machines down the side']),
    ('186a4d22', 'setup walkthrough',          ['setup.md', 'walkthrough']),
    ('4eff80fc', 'getting started',            ['getting started']),
    ('164cfafb', 'published PHP',              ['sheet_cache', 'strain_collector', 'tools/php']),
]

missing = []
print('  %-9s %-30s %s' % ('commit', 'what a user gets', 'in md / in html'))
print('  ' + '-' * 74)

for commit, what, keys in FEATURES:
    inmd = any(k in md for k in keys)
    inhtml = any(k in html for k in keys)
    mark = 'both' if (inmd and inhtml) else ('md only' if inmd else ('html only' if inhtml else 'NEITHER'))
    if not (inmd and inhtml):
        missing.append((commit, what, mark))
    print('  %-9s %-30s %s' % (commit, what[:30], mark))

print()
if missing:
    print('  %d gap(s):' % len(missing))
    for commit, what, mark in missing:
        print('    %-9s %-32s %s' % (commit, what, mark))
else:
    print('  every feature checked appears in both')

# ---------------------------------------------------------------------------
# The same question asked a second way.
#
# The list above comes from commit subjects, and a subject only names what somebody
# chose to name. This one comes from the client itself: every panel it offers, every
# settings tab, every rundown item type the fork added. Two methods that disagree
# would mean one of them has a hole.
# ---------------------------------------------------------------------------

print()


def upstream_has(name):
    """Was this in the client before the fork touched it?

    Anything that already existed upstream does not belong in a changelog of what
    this fork changed, so the question is only ever about the new ones.
    """
    r = subprocess.run(['git', 'cat-file', '-e',
                        'f22a0de5~1:src/Widgets/Rundown/Rundown%sWidget.h' % name],
                       capture_output=True)
    return r.returncode == 0


print('Panels offered in the layout editor')
panels = ['Activity', 'Clock', 'Http Log', 'Inspector', 'Live', 'NDI', 'Performance',
          'ServerStatus', 'Sheets', 'Simple Inspector', 'Trigger Banks', 'AudioLevels',
          'Preview', 'Library', 'StatusBar']
gaps = []
for p in panels:
    key = p.lower()
    ok = key in md and key in html
    if not ok:
        # try the squashed spelling too
        alt = key.replace(' ', '')
        ok = alt in md and alt in html
    print('  %-20s %s' % (p, 'described' if ok else 'NOT DESCRIBED'))
    if not ok:
        gaps.append('panel: ' + p)

print()
print('Rundown item types the fork added (upstream ones are not this changelog\'s job)')
items = ['AutoPlayGateway', 'CommandGateway', 'FocusGateway', 'StopAutoLoops', 'Grid',
         'ImageScroller', 'OscOutput', 'HttpGet', 'HttpPost', 'FileRecorder',
         'CustomCommand', 'PlayoutCommand', 'Commit', 'Print', 'Separator']
for name in items:
    if upstream_has(name):
        continue

    words = {'AutoPlayGateway': ['autoplay gateway', 'auto-play gateway', 'gateway'],
             'CommandGateway': ['command gateway', 'gateway'],
             'FocusGateway': ['focus gateway', 'gateway'],
             'StopAutoLoops': ['stop all auto'],
             'Grid': ['grid'],
             'ImageScroller': ['scroller'],
             'OscOutput': ['osc'],
             'HttpGet': ['http get'],
             'HttpPost': ['http post'],
             'FileRecorder': ['recorder'],
             'CustomCommand': ['custom command'],
             'PlayoutCommand': ['playout command', 'playout action'],
             'Commit': ['commit item'],
             'Print': ['print'],
             'Separator': ['separator']}.get(name, [name.lower()])

    ok = any(w in md for w in words) and any(w in html for w in words)
    print('  %-20s %s' % (name, 'described' if ok else 'NOT DESCRIBED'))
    if not ok:
        gaps.append('item: ' + name)

print()
print('Settings tabs')
for tab in ['Layout', 'Simple Mode', 'Sheets', 'Templates', 'Hotkeys', 'Live Stream', 'GPI', 'OSC']:
    key = tab.lower()
    ok = key in md and key in html
    print('  %-20s %s' % (tab, 'described' if ok else 'NOT DESCRIBED'))
    if not ok:
        gaps.append('settings: ' + tab)

print()
if gaps:
    print('%d thing(s) present in the client and not in the changelog:' % len(gaps))
    for g in gaps:
        print('  ' + g)
else:
    print('everything enumerated from the code appears in both documents')

import sys
sys.exit(1 if (missing or gaps) else 0)
