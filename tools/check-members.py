"""Every class member starts with a value.

A pointer, flag or number member declared without one holds whatever was in that
memory until the constructor gets to it. Build 289's startup crash was one:
MainWindow::setupMenu used widgetSimpleInspector before the constructor had made
it. On the dev machine the memory happened to be zero and nothing happened; on
the production machine it held old text, and the client died at every start.

So every member of the project's own classes is declared with a value:
pointers `= nullptr`, flags `= false`, numbers `= 0` - a constructor that sets
it still does, so nothing changes except that leftover memory can no longer
leak in. This finds the ones that are not.

    python tools/check-members.py          list them; exit 1 if there are any
    python tools/check-members.py --fix    give each one its value

What it reads: class and struct bodies in src/**/*.h, members declared on one
line as `Type* name;` or `bool|int|double|... name;`. It does not see enum or
class-typed members (a QString starts empty anyway), arrays, or members set
only in a constructor's initializer list - those last are written with a value
here too, which the initializer list overrides. src/lib (third-party) and
generated ui_*.h are not read. Runs in check-all and in the Linux CI build.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', 'src'))
SKIP_DIRS = {os.path.join(ROOT, 'lib')}

SCALARS = (r'(?:bool|unsigned\s+int|unsigned\s+long|unsigned|long\s+long|long|int|short|qint64|quint64|qint32|quint32|'
           r'qint16|quint16|qint8|quint8|qreal|qsizetype|uint|ulong|size_t|float|double|char)')
PREFIX = r'^(\s*)((?:mutable\s+)?(?:const\s+)?)'
POINTER = re.compile(PREFIX + r'([A-Za-z_][\w:]*(?:<[^;()=]*>)?\s*\*+\s*(?:const\s+)?)([A-Za-z_]\w*)(\s*;)')
SCALAR = re.compile(PREFIX + r'(' + SCALARS + r'\s+)([A-Za-z_]\w*)(\s*;)')
NOT_MEMBER = re.compile(r'^(static|friend|typedef|using|return|enum|Q_|inline|template|public|private|protected|signals|slots|explicit|virtual)\b')
CLASS_HEAD = re.compile(r'^(?:template\s*<.*>\s*)?(class|struct)\s+(?:\w+_EXPORT\s+)?(\w+)[^;]*$')


def value_for(kind, typetext):
    if kind == 'pointer':
        return 'nullptr'
    return 'false' if typetext.strip() == 'bool' else '0'


def strip_comments(text):
    # Block comments become blank lines, so line numbers stay true.
    return re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), text, flags=re.S)


def scan(path):
    """(line index, kind, class, member, match) for each member without a value."""
    found = []
    # newline='' keeps the file's own line endings: a fix must not rewrite every line.
    raw_lines = open(path, encoding='utf-8', errors='replace', newline='').read().split('\n')
    lines = strip_comments('\n'.join(raw_lines)).split('\n')

    depth = 0
    classes = []
    pending = None
    for index, text in enumerate(lines):
        code = text.split('//')[0]
        stripped = code.strip()

        head = CLASS_HEAD.match(stripped)
        if head and not stripped.endswith(';'):
            pending = head.group(2)

        directly_in_class = bool(classes) and depth == classes[-1][0]
        if (directly_in_class and stripped and '(' not in stripped and '=' not in stripped
                and '{' not in stripped and ':' not in stripped.replace('::', '') and not NOT_MEMBER.match(stripped)):
            for kind, pattern in (('pointer', POINTER), ('scalar', SCALAR)):
                match = pattern.match(code)
                if match:
                    found.append((index, kind, classes[-1][1], match.group(4), match))
                    break

        for ch in code:
            if ch == '{':
                depth += 1
                if pending is not None:
                    classes.append((depth, pending))
                    pending = None
            elif ch == '}':
                if classes and depth == classes[-1][0]:
                    classes.pop()
                depth -= 1

        if pending is not None and stripped.endswith(';') and '{' not in code:
            pending = None

    return raw_lines, found


def headers():
    for dirpath, dirnames, filenames in os.walk(ROOT):
        if any(os.path.commonpath([dirpath, skip]) == skip for skip in SKIP_DIRS):
            continue
        for name in sorted(filenames):
            if name.endswith('.h') and not name.startswith('ui_'):
                yield os.path.join(dirpath, name)


def main():
    fix = '--fix' in sys.argv
    total = 0
    files = 0

    for path in headers():
        raw_lines, found = scan(path)
        if not found:
            continue

        files += 1
        total += len(found)
        rel = os.path.relpath(path, os.path.dirname(ROOT)).replace('\\', '/')

        if not fix:
            for index, kind, cls, member, _ in found:
                print('  %s:%d  %s::%s' % (rel, index + 1, cls, member))
            continue

        for index, kind, cls, member, match in found:
            line = raw_lines[index]
            cr = line.endswith('\r')
            body = line[:-1] if cr else line
            semicolon = body.index(';', match.start(5))
            value = value_for(kind, match.group(3))
            raw_lines[index] = body[:semicolon].rstrip() + ' = ' + value + body[semicolon:] + ('\r' if cr else '')

        with open(path, 'w', encoding='utf-8', newline='') as out:
            out.write('\n'.join(raw_lines))

    if fix:
        print('Gave %d members a value, in %d headers.' % (total, files))
        return 0

    if total:
        print('')
        print('%d members in %d headers start with no value. Give each one (= nullptr, = false, = 0),' % (total, files))
        print('or run: python tools/check-members.py --fix')
        return 1

    print('Every member of the project\'s classes starts with a value.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
