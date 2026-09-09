"""Every Settings tab built in code has to scroll.

There is a loop near the top of the SettingsDialog constructor that wraps each tab
in a scroll area, and it cannot help the tabs built in code: it runs before they
exist, and it skips anything that already has a layout, which they all do. So each
of them calls scrollableTab() instead - and the next one added will not, unless
something checks.

It matters more than it sounds. A Qt layout given less height than its rows need
does not clip and does not scroll: it compresses the rows until they draw on top
of one another. The Templates tab reached that point and became unreadable, with
labels sitting over the fields they name. Sheets and Simple Mode are the same
shape and were heading the same way as they grew.

Run from check-all.py.
"""

import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, '..', 'src', 'Widgets', 'SettingsDialog.cpp')


def main():
    text = io.open(SOURCE, encoding='utf-8').read()

    # Every tab handed to addTab from code.
    added = re.findall(r'addTab\(\s*(\w+)\s*,\s*"([^"]*)"', text)

    # Every widget passed through the helper.
    wrapped = set(re.findall(r'scrollableTab\(\s*(\w+)\s*\)', text))

    # A QScrollArea used directly as the tab is the same thing by other means.
    own_scroll = set(re.findall(r'QScrollArea\*\s*(\w+)\s*=\s*new QScrollArea', text))

    print('Settings tabs')

    problems = []
    for widget, title in added:
        if widget in wrapped:
            how = 'scrollableTab()'
        elif widget in own_scroll:
            how = 'is a scroll area itself'
        else:
            how = 'DOES NOT SCROLL'
            problems.append((widget, title))

        print('  %-16s %-14s %s' % (title, widget, how))

    if problems:
        print()
        for widget, title in problems:
            print('  ! The "%s" tab (%s) is added in code without a scroll area.' % (title, widget))
        print()
        print('    Build it as:')
        print('        QWidget* tabX = new QWidget();')
        print('        QWidget* content = scrollableTab(tabX);')
        print('        QVBoxLayout* box = new QVBoxLayout(content);')
        print('    and parent its children to content rather than to tabX.')
        print()
        print('    Without it the tab has to fit the dialog, and when it stops fitting')
        print('    its rows are drawn on top of one another rather than scrolled.')

        return 1

    print()
    print('  %d tabs added in code, all of them scroll' % len(added))

    return 0


if __name__ == '__main__':
    sys.exit(main())
