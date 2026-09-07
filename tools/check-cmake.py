"""Every source in the tree is actually in the build.

The syntax checker compiles files directly with include paths of its own, so it
would happily pass a file that CMake has never heard of. That file then fails at
link time on the machine doing the real build, which is the one place none of this
tooling runs.

So this reads the CMakeLists files instead and asks the only question they can
answer: is every .cpp under src/ named in one of them, and does every generated SQL
script exist and appear in the resource file that ships it.

    python tools/check-cmake.py
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src')

# Not built as part of the application: platform-specific sources CMake picks per
# platform, and third-party code with a build of its own.
SKIP_DIRS = ('lib', 'Mac')


def cmake_text():
    """Everything every CMakeLists in the tree says, as one blob."""
    blob = []
    for base, _, files in os.walk(SRC):
        for name in files:
            if name == 'CMakeLists.txt':
                with open(os.path.join(base, name), 'r', encoding='utf-8', errors='replace') as handle:
                    blob.append(handle.read())

    top = os.path.join(ROOT, 'CMakeLists.txt')
    if os.path.exists(top):
        with open(top, 'r', encoding='utf-8', errors='replace') as handle:
            blob.append(handle.read())

    return '\n'.join(blob)


def referenced_elsewhere(stem, ownPath):
    """Does anything outside this file and its own header mention it."""
    own = os.path.basename(ownPath)[:-4]

    for base, dirs, files in os.walk(SRC):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]

        for name in files:
            if not name.endswith(('.cpp', '.h')):
                continue
            if name[:-4] == own or name[:-2] == own:
                continue          # itself, and its own header

            with open(os.path.join(base, name), 'r', encoding='utf-8', errors='replace') as handle:
                if stem in handle.read():
                    return True

    return False


def main():
    build = cmake_text()
    if not build.strip():
        print('No CMakeLists found. Nothing to check against.')
        return 2

    missing = []
    counted = 0

    for base, dirs, files in os.walk(SRC):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]

        for name in files:
            if not name.endswith('.cpp'):
                continue

            counted += 1
            # Named anywhere in any CMakeLists is enough: this is about a file being
            # forgotten entirely, not about which target claims it.
            if name not in build:
                missing.append(os.path.relpath(os.path.join(base, name), ROOT))

    # Worth saying carefully, because the two cases are not the same. A file nothing
    # else references is dead code sitting in the tree: harmless, but somebody
    # editing it would watch their changes have no effect. A file that IS referenced
    # and not built is a link error waiting for the real build.
    print('Sources')
    print('  %d .cpp files under src/, %d not named in any CMakeLists' % (counted, len(missing)))
    for path in sorted(missing):
        stem = os.path.basename(path)[:-4]
        used = referenced_elsewhere(stem, path)
        print('    %-9s %s%s' % ('IN USE' if used else 'unbuilt', path,
                                 '  <- referenced elsewhere, so the build would fail' if used else
                                 '  <- nothing references it, so it is dead rather than broken'))

    # ---- the database scripts, which fail differently and just as quietly ----
    print()
    print('Database')

    sql = os.path.join(SRC, 'Core', 'Sql')
    qrc = os.path.join(SRC, 'Core', 'Core.qrc')
    version = os.path.join(SRC, 'Common', 'Version.h.in')

    problems = []

    scripts = sorted(int(re.search(r'(\d+)', n).group(1))
                     for n in os.listdir(sql) if n.startswith('ChangeScript-'))

    with open(qrc, 'r', encoding='utf-8', errors='replace') as handle:
        resources = handle.read()
    with open(version, 'r', encoding='utf-8', errors='replace') as handle:
        declared = re.search(r'DATABASE_VERSION "(\d+)"', handle.read()).group(1)

    # A script that is not in the resource file is not in the binary, so an upgrade
    # would step over it and every setting it adds would be missing at runtime.
    for number in scripts:
        if ('ChangeScript-%d.sql' % number) not in resources:
            problems.append('ChangeScript-%d.sql is not listed in Core.qrc' % number)

    # A gap in the sequence stops the upgrade loop dead at the gap.
    for number in range(scripts[0], scripts[-1] + 1):
        if number not in scripts:
            problems.append('ChangeScript-%d.sql is missing, so upgrades stop there' % number)

    if str(scripts[-1]) != declared:
        problems.append('DATABASE_VERSION is %s but the highest script is %d'
                        % (declared, scripts[-1]))

    print('  %d change scripts, highest %d, DATABASE_VERSION %s'
          % (len(scripts), scripts[-1], declared))
    for problem in problems:
        print('    ' + problem)

    print()
    serious = [p for p in missing if referenced_elsewhere(os.path.basename(p)[:-4], p)]

    if serious or problems:
        print('%d problem(s) that would break a build or leave a setting missing.'
              % (len(serious) + len(problems)))
        return 1

    if missing:
        print('%d unbuilt file(s), referenced by nothing. Dead rather than broken; '
              'worth deleting when someone is sure.' % len(missing))
        return 0

    print('Everything in the tree is in the build.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
