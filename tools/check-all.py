"""Run every check, and say plainly what passed.

There are ten test suites and a syntax checker. Ten commands that each take a
minute is a thing nobody runs, so this is the one command: it builds and runs all
of them, adds up the assertions, and exits non-zero if anything failed.

    python tools/check-all.py           everything
    python tools/check-all.py --fast    only the suites that need no network
    python tools/check-all.py --list    what it would run, and why

The fast set finishes in well under a minute and covers the path rules, the
installer, and how the push tool reads an address. The rest start real servers on
spare ports and need PHP, so they take longer.

Nothing here touches the project's build directory, any real templates folder, or
the push tool's saved settings. Every suite works in a temporary folder of its own.
"""

import os
import re
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, 'tools')

# name, needs a network and PHP, one line on what it is for
SUITES = [
    ('test-paths',   False, 'path rules, protected files, git digests, the bad-token throttle'),
    ('test-install', False, 'installFile against a real folder, and where anything landed'),
    ('test-target',  False, 'how the push tool reads an address and builds every URL'),
    ('test-layout',  False, 'panel heights kept inside the screen so the window can always fit'),
    ('test-assign',  False, 'who decides which packs a machine takes: the source, or the machine'),
    ('test-autosave',False, 'naming a recovery copy, and reading back the rundown it came from'),
    ('test-server',  True,  'the HTTP parser, driven over real sockets with real rubbish'),
    ('test-push',    True,  'a whole push, clicked through the buttons, to a real client'),
    ('test-pull',    True,  'the pull route end to end against a real PHP relay'),
    ('test-github',  True,  'the GitHub pull route against a mock of the API'),
]


def run(name):
    """Build and run one suite. Returns (passed, failed, ok, output)."""
    script = os.path.join(TOOLS, name + '.bat')
    if not os.path.exists(script):
        return (0, 0, False, 'no such suite')

    result = subprocess.run(['cmd', '/c', script], cwd=ROOT,
                            capture_output=True, text=True)
    output = result.stdout + result.stderr

    found = re.search(r'(\d+) passed, (\d+) failed', output)
    if not found:
        # A build failure, or PHP missing, or something else that never got as far
        # as counting. The reason is in the output and worth showing.
        return (0, 0, False, output.strip().splitlines()[-1] if output.strip() else 'no output')

    passed = int(found.group(1))
    failed = int(found.group(2))
    return (passed, failed, failed == 0 and result.returncode == 0, output)


def main():
    # Each suite takes the better part of a minute, so a run that printed nothing
    # until the end would look like a hang. Line buffering makes each result appear
    # as it happens.
    try:
        sys.stdout.reconfigure(line_buffering=True)
    except AttributeError:
        pass

    fast = '--fast' in sys.argv

    if '--list' in sys.argv:
        for name, network, what in SUITES:
            print('  %-13s %-9s %s' % (name, '(network)' if network else '', what))
        return 0

    wanted = [s for s in SUITES if not (fast and s[1])]

    print('Syntax')
    # The last commit and anything uncommitted, not the whole branch: this is the
    # check before a build, and type-checking fifty unchanged files to find nothing
    # is how a check stops being run.
    checker = subprocess.run([sys.executable, os.path.join(TOOLS, 'syntax-check.py')],
                             cwd=ROOT, capture_output=True, text=True)
    syntax_ok = checker.returncode == 0
    for line in checker.stdout.splitlines():
        if 'FAIL' in line or 'passed,' in line or 'no definition' in line or '::' in line:
            print('  ' + line.strip())
    if not checker.stdout.strip():
        print('  nothing changed to check')

    # Compiling a file directly says nothing about whether the real build knows it
    # exists, and a source missing from a CMakeLists fails on the machine none of
    # this runs on.
    print()
    print('Build wiring')
    wiring = subprocess.run([sys.executable, os.path.join(TOOLS, 'check-cmake.py')],
                            cwd=ROOT, capture_output=True, text=True)
    wiring_ok = wiring.returncode == 0
    for line in wiring.stdout.splitlines():
        if line.strip() and not line.startswith('Sources') and not line.startswith('Database'):
            print('  ' + line.strip())

    # A feature nobody is told about is a feature nobody uses.
    print()
    print('Changelog')
    story = subprocess.run([sys.executable, os.path.join(TOOLS, 'check-changelog.py')],
                           cwd=ROOT, capture_output=True, text=True)
    story_ok = story.returncode == 0
    for line in story.stdout.splitlines():
        if 'NEITHER' in line or 'md only' in line or 'html only' in line or 'gap(s)' in line or 'every feature' in line:
            print('  ' + line.strip())

    print()
    print('Suites')

    totalPassed = 0
    totalFailed = 0
    broken = []

    for name, network, _ in wanted:
        started = time.time()
        passed, failed, ok, output = run(name)
        took = time.time() - started

        totalPassed += passed
        totalFailed += failed

        if ok:
            print('  %-13s %4d passed            %4.0fs' % (name, passed, took))
        elif passed == 0 and failed == 0:
            # Never ran. A missing PHP is a skip, not a failure: the suite is
            # honest about needing it and saying otherwise would be worse.
            skipped = 'PHP' in output or 'not found' in output
            print('  %-13s %s  %s' % (name, 'SKIPPED' if skipped else 'DID NOT RUN', output[:60]))
            if not skipped:
                broken.append(name)
        else:
            print('  %-13s %4d passed, %d FAILED  %4.0fs' % (name, passed, failed, took))
            broken.append(name)
            for line in output.splitlines():
                if line.strip().startswith('FAIL'):
                    print('        ' + line.strip())

    print()
    print('%d assertions passed, %d failed' % (totalPassed, totalFailed))
    if fast:
        print('(--fast: the suites that need a network were not run)')

    if not syntax_ok:
        print('Syntax check failed.')
    if not wiring_ok:
        print('Build wiring has a problem.')
    if not story_ok:
        print('A feature is missing from the changelog.')
    if broken:
        print('Failed: ' + ', '.join(broken))

    return 0 if (syntax_ok and wiring_ok and story_ok and not broken and totalFailed == 0) else 1


if __name__ == '__main__':
    sys.exit(main())
