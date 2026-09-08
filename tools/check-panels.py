"""The one place a panel id still has to be written out by hand.

PanelRegistry gave the id list, the display names, the two heights, the sizing
default and the preset key set a single source. One thing could not follow: the
map from an id to the actual widget member in MainWindow, because there is no
generic way to write `return widgetPreview`.

So that map is the last place a new panel can be forgotten, and forgetting it is
the worst of the failure modes: the panel appears in the layout editor, is offered
a sizing row, is saved by a named layout, and then places nothing at all.

A compiled test cannot reach it - MainWindow needs the whole application. This
reads the two files instead and compares the ids in them, which is enough, because
both are literal lists in C++ source.

Run by tools/check-all.py; exits non-zero on a mismatch.
"""

import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

REGISTRY = os.path.join(ROOT, 'src', 'Common', 'PanelRegistry.h')
MAINWINDOW = os.path.join(ROOT, 'src', 'Widgets', 'MainWindow.cpp')

# The files that must not grow a hand-written panel list again. Each is a file
# that had one, mapped to the marker proving it now reads the registry instead.
CONSUMERS = {
    os.path.join('src', 'Widgets', 'LayoutEditorWidget.cpp'): 'PanelRegistry::offeredIds(',
    os.path.join('src', 'Widgets', 'SettingsDialog.cpp'): 'PanelRegistry::all()',
    os.path.join('src', 'Common', 'LayoutPreset.h'): 'PanelRegistry::ids()',
    os.path.join('src', 'Widgets', 'MainWindow.cpp'): 'PanelRegistry::defaultHeight',
}


def read(path):
    return io.open(path, encoding='utf-8').read()


def registered_ids():
    """The ids from the panels.append({ "Id", ... }) calls in the registry."""
    source = read(REGISTRY)

    start = source.index('inline const QList<Entry>& all()')
    end = source.index('inline QStringList ids()')

    ids = re.findall(r'panels\.append\(\{\s*"([^"]+)"', source[start:end])

    if not ids:
        sys.exit('check-panels: found no panels in PanelRegistry.h - has all() been rewritten?')

    return ids


def mapped_ids():
    """The ids MainWindow::widgetById can actually resolve to a widget."""
    source = read(MAINWINDOW)

    marker = 'QWidget* MainWindow::widgetById(const QString& id)'
    if marker not in source:
        sys.exit('check-panels: could not find MainWindow::widgetById - has it been renamed?')

    start = source.index(marker)
    end = source.index('\n}', start)
    body = source[start:end]

    # `if (id == "Live" || id == "NDI") return ...` is one statement naming two
    # ids, so match every comparison rather than every return.
    return re.findall(r'id\s*==\s*"([^"]+)"', body)


def main():
    registered = registered_ids()
    mapped = mapped_ids()

    problems = []

    missing = [i for i in registered if i not in mapped]
    if missing:
        problems.append(
            'registered but MainWindow::widgetById returns nullptr for them,\n'
            '  so they can be placed and will show nothing: ' + ', '.join(missing))

    stale = [i for i in mapped if i not in registered]
    if stale:
        problems.append(
            'MainWindow::widgetById knows them but PanelRegistry does not,\n'
            '  so they can never be placed: ' + ', '.join(stale))

    duplicates = sorted(set(i for i in registered if registered.count(i) > 1))
    if duplicates:
        problems.append('registered more than once: ' + ', '.join(duplicates))

    for relative, marker in sorted(CONSUMERS.items()):
        source = read(os.path.join(ROOT, relative))
        if marker not in source:
            problems.append(
                '%s no longer reads the registry (looked for %s)' % (relative, marker))

    if problems:
        print('Panel registry')
        for problem in problems:
            print('  FAIL  ' + problem)
        return 1

    print('Panel registry: %d panels, all placed by MainWindow, %d consumers reading the registry'
          % (len(registered), len(CONSUMERS)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
