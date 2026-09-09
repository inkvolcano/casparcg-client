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

    # The list that actually decides what ends up in the binary.
    #
    # This used to check Core.qrc, which the build does not read: the resource is
    # built by qt_add_resources() from Core_resource_files in CMakeLists.txt, and
    # the two drifted thirty scripts apart without a word. Every migration from 229
    # to 258 was on disk, in the .qrc, and absent from the binary - so the upgrade
    # loop opened nothing, stepped over all of them, and left every database in the
    # estate stuck at 228 while the code believed it was at 258.
    #
    # What that looked like: an empty Library, because the queries asked for columns
    # a migration was supposed to have added. Nothing said why. The .qrc is checked
    # too, because a script missing from it is a sign the two have drifted again.
    cmakelists = os.path.join(SRC, 'Core', 'CMakeLists.txt')
    with open(cmakelists, 'r', encoding='utf-8', errors='replace') as handle:
        cmake = handle.read()

    listed = cmake[cmake.index('set(Core_resource_files'):]
    listed = listed[:listed.index('\n)')]

    for number in scripts:
        name = 'ChangeScript-%d.sql' % number

        if name not in listed:
            problems.append(name + ' is not in Core_resource_files, so it is NOT in the '
                                   'binary and will never run')
        if name not in resources:
            problems.append(name + ' is not listed in Core.qrc')

    # A semicolon inside a comment splits the file in the wrong place.
    #
    # The upgrade loop reads a script and splits it on ';' - it has no idea what a
    # comment is - so "-- every pack the relay carries; a list means only those"
    # becomes two statements, the second of which starts mid-sentence and is a
    # syntax error. That stops the client dead, and it was sitting in two migrations
    # and in Schema.sql, which is what a brand new database is built from.
    for name in sorted(os.listdir(sql)):
        if not name.endswith('.sql'):
            continue

        with open(os.path.join(sql, name), 'r', encoding='utf-8', errors='replace') as handle:
            for number, line in enumerate(handle, 1):
                stripped = line.strip()
                if stripped.startswith('--') and ';' in stripped:
                    problems.append('%s line %d has a semicolon in a comment, which '
                                    'splits the script there' % (name, number))

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
