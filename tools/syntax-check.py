"""Syntax-check changed C++ files without building anything.

This exists because a wrong header or a missing forward declaration costs a full
rebuild to discover, and the person who finds out is whoever pressed build. It
compiles with /Zs, which parses and type-checks and then throws the result away:
no object files, nothing written to the build directory, no linking. It is not a
build and does not replace one.

    python tools/syntax-check.py                 files changed since the last commit
    python tools/syntax-check.py a.cpp b.cpp     just these
    python tools/syntax-check.py --all-mine      everything changed on this branch

It also runs uic over every .ui and moc over every Q_OBJECT header the given
files need, into a temporary folder, and compiles the moc output too. A signal
whose type does not exist shows up there rather than three minutes into a build.

What it will not catch: anything that only fails at link time, such as a slot
that is declared and never defined.
"""

import glob
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, 'build')


def first(*patterns):
    """The first path that actually exists, so versions can move."""
    for pattern in patterns:
        for hit in sorted(glob.glob(pattern)):
            if os.path.exists(hit):
                return hit
    return None


def find_vcvars():
    return first(
        r'C:\Program Files\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvars64.bat',
        r'C:\Program Files (x86)\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvars64.bat',
    )


def find_qt():
    # The kit the project builds against, whatever version is installed.
    return first(r'C:\Qt\6.*\msvc*_64\include')


def include_dirs(qt_include):
    dirs = [ROOT, os.path.join(ROOT, 'src'), BUILD, os.path.join(BUILD, 'Common')]

    # Every source folder that holds a header, because this tree includes by bare
    # name across modules rather than by path.
    for base, _, files in os.walk(os.path.join(ROOT, 'src')):
        if any(f.endswith('.h') for f in files):
            dirs.append(base)

    # Bare "osc/..." and "vlc/..." includes want the parent of those folders.
    for extra in (r'src\lib\oscpack', r'lib\oscpack', r'src\lib', 'lib'):
        candidate = os.path.join(ROOT, extra)
        if os.path.isdir(candidate):
            dirs.append(candidate)

    # Dependencies the build fetches for itself. Globbed, so a version bump does
    # not quietly turn every check into a failure about a missing header.
    for pattern in (r'boost-prefix\src\boost\include\boost-*',
                    r'libvlc*-prefix\src\libvlc*\include'):
        for hit in glob.glob(os.path.join(BUILD, pattern)):
            dirs.append(hit)

    for module in ('', 'QtCore', 'QtGui', 'QtWidgets', 'QtNetwork', 'QtXml', 'QtSql'):
        dirs.append(os.path.join(qt_include, module) if module else qt_include)

    return dirs


def generate_ui(qt_bin, out_dir):
    """uic every .ui in the tree. Cheap, and saves working out which are needed."""
    uic = os.path.join(qt_bin, 'uic.exe')
    if not os.path.exists(uic):
        return 0

    made = 0
    for base, _, files in os.walk(os.path.join(ROOT, 'src')):
        for name in files:
            if not name.endswith('.ui'):
                continue
            target = os.path.join(out_dir, 'ui_' + name[:-3] + '.h')
            if subprocess.call([uic, os.path.join(base, name), '-o', target],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0:
                made += 1
    return made


def moc_headers(qt_bin, sources, out_dir):
    """moc the header beside each source, when it declares Q_OBJECT."""
    moc = os.path.join(qt_bin, 'moc.exe')
    if not os.path.exists(moc):
        return []

    generated = []
    for source in sources:
        header = os.path.splitext(source)[0] + '.h'
        if not os.path.exists(header):
            continue

        with open(header, 'r', encoding='utf-8', errors='replace') as handle:
            if 'Q_OBJECT' not in handle.read():
                continue

        target = os.path.join(out_dir, 'moc_' + os.path.basename(header)[:-2] + '.cpp')
        if subprocess.call([moc, header, '-o', target],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0:
            generated.append(target)
        else:
            print('  moc FAILED   ' + os.path.relpath(header, ROOT))

    return generated


def changed_files(all_mine):
    """What to check when nothing was named."""
    if all_mine:
        base = subprocess.run(['git', 'merge-base', 'HEAD', 'main'], cwd=ROOT,
                              capture_output=True, text=True).stdout.strip()
        spec = [base, 'HEAD'] if base else ['HEAD~1', 'HEAD']
        args = ['git', 'diff', '--name-only'] + spec
    else:
        args = ['git', 'diff', '--name-only', 'HEAD~1', 'HEAD']

    out = subprocess.run(args, cwd=ROOT, capture_output=True, text=True).stdout
    named = [line.strip() for line in out.splitlines() if line.strip().endswith('.cpp')]

    # Anything edited but not yet committed belongs in the check too.
    out = subprocess.run(['git', 'diff', '--name-only'], cwd=ROOT,
                         capture_output=True, text=True).stdout
    named += [line.strip() for line in out.splitlines() if line.strip().endswith('.cpp')]

    return sorted({os.path.join(ROOT, n) for n in named if os.path.exists(os.path.join(ROOT, n))})


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    all_mine = '--all-mine' in sys.argv

    vcvars = find_vcvars()
    qt_include = find_qt()

    if not vcvars:
        print('No Visual Studio found. Nothing to check with.')
        return 2
    if not qt_include:
        print('No Qt 6 kit found under C:/Qt. Nothing to check against.')
        return 2

    qt_bin = os.path.join(os.path.dirname(qt_include), 'bin')

    sources = [os.path.abspath(a) for a in args] if args else changed_files(all_mine)
    sources = [s for s in sources if s.endswith('.cpp')]

    if not sources:
        print('Nothing to check.')
        return 0

    out_dir = tempfile.mkdtemp(prefix='syntaxcheck-')
    made = generate_ui(qt_bin, out_dir)
    generated = moc_headers(qt_bin, sources, out_dir)

    print('%d ui header(s), %d moc file(s), %d source(s)'
          % (made, len(generated), len(sources)))
    print()

    dirs = include_dirs(qt_include) + [out_dir]
    includes = ' '.join('/I"%s"' % d for d in dirs)

    # The library defines matter: without them every export macro reads as
    # dllimport and moc output fails on a static member it is entitled to define.
    defines = ('/DQT_CORE_LIB /DQT_GUI_LIB /DQT_WIDGETS_LIB /DQT_NETWORK_LIB '
               '/DWIDGETS_LIBRARY /DCORE_LIBRARY /DCOMMON_LIBRARY /DCASPAR_LIBRARY')

    failures = []
    for source in sources + generated:
        script = os.path.join(out_dir, 'run.bat')
        with open(script, 'w') as handle:
            handle.write('@echo off\r\n')
            handle.write('call "%s" >nul 2>&1\r\n' % vcvars)
            handle.write('cd /d "%s"\r\n' % ROOT)
            handle.write('cl /Zs /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /W3 %s %s "%s"\r\n'
                         % (defines, includes, source))

        result = subprocess.run(['cmd', '/c', script], capture_output=True, text=True)
        name = os.path.relpath(source, ROOT) if source.startswith(ROOT) else os.path.basename(source)

        if result.returncode == 0:
            print('  PASS  ' + name)
        else:
            print('  FAIL  ' + name)
            failures.append(name)
            for line in (result.stdout + result.stderr).splitlines():
                if 'error C' in line:
                    print('        ' + line.strip())

    print()
    print('%d passed, %d failed' % (len(sources) + len(generated) - len(failures), len(failures)))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
