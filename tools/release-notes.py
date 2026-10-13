"""What is new since a published build, from what CHANGES.md gained since then.

    python tools/release-notes.py --since-build 282

Prints Markdown: one "### <section>" per changelog section that was added or
added to since that build, with the lines it gained. Prints nothing when nothing
was added. tools/publish-build.ps1 puts this at the top of a release's notes,
under "## What's new", and the client's update window shows it.

How "since that build" is found: the build number lives in
src/Common/Version.h.in as DEV_BUILD_ID "N". The commit that changed it away
from N is the first one after build N; its parent is the source as build N was
built. The changelog is compared from there to now.

CHANGES.md puts each new section above the one before it, so the sections come
out newest first.
"""

import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = 'src/Common/Version.h.in'
CHANGELOG = 'CHANGES.md'


def git(*args):
    return subprocess.run(['git', '-C', ROOT] + list(args), capture_output=True, text=True,
                          encoding='utf-8', errors='replace').stdout


def base_commit(build):
    """The last commit at which DEV_BUILD_ID was this build, or None."""
    marker = 'DEV_BUILD_ID "%d"' % build
    for commit in git('log', '--format=%H', '-S', marker, '--', VERSION).split():
        before = git('show', '%s^:%s' % (commit, VERSION))
        after = git('show', '%s:%s' % (commit, VERSION))
        if marker in before and marker not in after:
            return commit + '^'
    return None


def notes_since(build):
    base = base_commit(build)
    if base is None:
        return ''

    diff = git('diff', '-U1000000', base, 'HEAD', '--', CHANGELOG)

    # A section moved further up the file shows as removed and added again; its
    # lines are not new. Lines that were removed somewhere are not counted.
    removed = set(l[1:] for l in diff.splitlines() if l.startswith('-') and not l.startswith('---'))

    sections = []          # [heading, [added lines]]
    current = None
    in_body = False
    for line in diff.splitlines():
        if line.startswith('@@'):
            in_body = True
            continue
        if not in_body or not line or line[0] not in ' +-':
            continue

        kind, text = line[0], line[1:]
        if kind == '-':
            continue

        if text.startswith('## '):
            current = [text[3:].strip(), []]
            sections.append(current)
            continue

        if kind == '+' and current is not None and (text not in removed or not text.strip()):
            current[1].append(text)

    out = []
    for heading, lines in sections:
        while lines and not lines[0].strip():
            lines.pop(0)
        while lines and not lines[-1].strip():
            lines.pop()
        if not lines or all(l.strip() in ('', '---') for l in lines):
            continue

        # A section's own sub-headings sit one level below its heading here.
        body = [('#' + l) if l.startswith('### ') else l for l in lines]
        out.append('### %s\n\n%s' % (heading, '\n'.join(body)))

    return '\n\n'.join(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--since-build', type=int, required=True)
    args = parser.parse_args()

    sys.stdout.reconfigure(encoding='utf-8')
    text = notes_since(args.since_build)
    if text:
        print(text)


if __name__ == '__main__':
    main()
