"""No range-for over a list built in the loop header with <<.

    for (const QString& name : QStringList() << "a" << "b")    // wrong

reads freed memory: operator<< returns a reference to the temporary list, and a
range-for keeps only that reference, so the temporary is destroyed before the
loop starts. It does not always crash. In build 288 it made the config editor
skip its "no <controllers>" check on some runs and not others, and in build 280
it crashed test-asrun. Name the list first:

    const QStringList names = QStringList() << "a" << "b";
    for (const QString& name : names)                           // right

    python tools/check-loops.py      list them; exit 1 if there are any

Reads src/ (not src/lib, third-party) and tools/, .h and .cpp. Runs in
check-all and in the Linux CI build.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..'))
ROOTS = [os.path.join(REPO, 'src'), HERE]
SKIP = [os.path.join(REPO, 'src', 'lib')]

PATTERN = re.compile(r'for\s*\([^;)]*:\s*(?:std::)?\w*(?:List|Vector|Set|vector|list)(?:<[^>]*>)?\s*(?:\(\)|\{\})\s*<<')


def main():
    found = []
    for root in ROOTS:
        for dirpath, _, filenames in os.walk(root):
            if any(os.path.commonpath([dirpath, skip]) == skip for skip in SKIP):
                continue
            for name in sorted(filenames):
                if not (name.endswith('.h') or name.endswith('.cpp')):
                    continue
                path = os.path.join(dirpath, name)
                for number, line in enumerate(open(path, encoding='utf-8', errors='replace'), 1):
                    code = line.split('//')[0]
                    if PATTERN.search(code):
                        found.append('  %s:%d  %s' % (os.path.relpath(path, REPO).replace('\\', '/'), number, line.strip()))

    if found:
        print('\n'.join(found))
        print('')
        print('%d range-for loops over a temporary list built with <<. Name the list first.' % len(found))
        return 1

    print('No range-for loops over a temporary list.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
