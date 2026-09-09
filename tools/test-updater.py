# The script that replaces the client with a newer build.
#
# This is the one part of the update feature that can leave a machine unable to
# start. It is generated as C++ string concatenation, no compiler ever looks at
# it, and it runs after the client has exited - so nothing else in this repository
# would notice if it were wrong.
#
# So it is reconstructed from the source and RUN, against a throwaway installation
# with a throwaway package. Reconstructed rather than retyped: a copy of the
# script in this file would be a copy that drifts, and would then pass while the
# real one was broken.
#
# The case that matters most is the one that actually happened on a venue machine.
# The package holds ONE FOLDER named for the build. Copy that folder instead of
# what is inside it and the old executable stays exactly where it was - still
# running, still reporting the old number - while every library around it is
# replaced.

import io
import os
import re
import shutil
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, '..', 'src', 'Widgets', 'UpdateDialog.cpp')
# Named for this process, so two runs of the suite - or one that overlaps another
# - cannot use the same folder and fail each other. They did once, and it looked
# exactly like a real regression in the thing being tested.
WORK = os.path.join(os.environ.get('TEMP', r'C:\tmp'),
                    'casparcg-test-updater-%d' % os.getpid())

failures = []
checks = []


def check(ok, what):
    checks.append(what)
    if not ok:
        failures.append(what)
        print("  FAIL  " + what)


def build_script(work):
    """The batch the client would write, with this test's paths in it."""
    text = io.open(SOURCE, encoding='utf-8').read()

    body = text[text.index('QString UpdateDialog::writeUpdaterScript'):]
    body = body[:body.index('script.write(')]

    substitutions = {
        'installDir': os.path.join(work, 'install'),
        'zip': os.path.join(work, 'install', 'updates', 'package.zip'),
        # Inside the installation, which is where a real client keeps it.
        'folder': os.path.join(work, 'install', 'updates'),
        'exe': os.path.join(work, 'install', 'casparcg-client.exe'),
        'scriptPath': os.path.join(work, 'install', 'updates', 'apply-update.cmd'),
    }

    # A string literal, or one of those names, whichever comes next. Order is as
    # important as content: set "WORK=" + folder + "\staged" has to come out as one
    # line with the path in the middle, and emitting the literals first put it on
    # the line after - which produced a script that failed for reasons that had
    # nothing to do with the script.
    pattern = re.compile(r'"((?:[^"\\]|\\.)*)"'
                         r'|\b(' + '|'.join(substitutions) + r')\b')

    script = ''
    for statement in re.findall(r'text \+=(.*?);\s*\n', body, re.DOTALL):
        for match in pattern.finditer(statement):
            literal, name = match.group(1), match.group(2)

            if literal is not None:
                script += (literal.replace('\\r', '\r').replace('\\n', '\n')
                                  .replace('\\"', '"').replace('\\\\', '\\'))
            else:
                script += substitutions[name]

    # The last thing it does is start the client. There is nothing here to start,
    # and a missing executable would read as a failure of the copy.
    return re.sub(r'start "" "[^"]*"\r?\n', '', script)


def run_updater(make_package, keep_extra=False):
    """A fresh installation and package, the updater run over them, what survived."""
    if os.path.exists(WORK):
        shutil.rmtree(WORK)

    install = os.path.join(WORK, 'install')
    download = os.path.join(install, 'updates')
    os.makedirs(install)
    os.makedirs(download)

    io.open(os.path.join(install, 'casparcg-client.exe'), 'w').write('OLD BUILD 208')
    io.open(os.path.join(install, 'Qt6Core.dll'), 'w').write('old qt')

    if keep_extra:
        # Something the new build does not carry. A copy is not a wipe, and an
        # operator's database living beside the binary has to survive one.
        os.makedirs(os.path.join(install, 'Data'))
        io.open(os.path.join(install, 'Data', 'local.db'), 'w').write('the database')

    make_package(download)

    script_path = os.path.join(download, 'apply-update.cmd')
    io.open(script_path, 'w', encoding='utf-8', newline='').write(build_script(WORK))

    result = subprocess.run(['cmd.exe', '/c', script_path], input='\n',
                            capture_output=True, text=True, timeout=180)

    exe = os.path.join(install, 'casparcg-client.exe')
    contents = io.open(exe).read() if os.path.exists(exe) else '<missing>'

    return result.returncode, contents, install, download


def good_package(download):
    staged = os.path.join(WORK, 'make', 'casparcg-client-v2.3.1-209')
    os.makedirs(staged)
    io.open(os.path.join(staged, 'casparcg-client.exe'), 'w').write('NEW BUILD 209')
    io.open(os.path.join(staged, 'Qt6Core.dll'), 'w').write('new qt')
    shutil.make_archive(os.path.join(download, 'package'), 'zip',
                        os.path.join(WORK, 'make'))


def theScriptComesOutOfTheSource():
    script = build_script(WORK)

    check(len(script) > 1500, "the script was reconstructed rather than coming out empty")
    check('Expand-Archive' in script, "it unpacks with Expand-Archive")
    check('tasklist' in script, "it waits for the client to be gone before touching anything")
    check('robocopy' in script, "it copies with robocopy")
    check('%WORK%\\staged' in script or 'staged' in script, "the paths were spliced in place")


def aNewBuildActuallyReplacesTheOldOne():
    code, contents, install, download = run_updater(good_package, keep_extra=True)

    check(code == 0, "the updater finished without an error")
    check(contents == 'NEW BUILD 209',
          "the executable was replaced (found: %s)" % contents)

    # The failure that prompted all of this.
    check(not os.path.exists(os.path.join(install, 'casparcg-client-v2.3.1-209')),
          "the folder inside the package was not copied in as a folder")

    check(os.path.exists(os.path.join(install, 'Data', 'local.db')),
          "files the new build does not carry were left alone")

    previous = os.path.join(download, 'previous', 'casparcg-client.exe')
    check(os.path.exists(previous) and io.open(previous).read() == 'OLD BUILD 208',
          "the build that was running is kept, so it can be put back")

    # The updates folder lives inside the installation, so a backup that does not
    # exclude it copies the folder into itself - the package, the unpacked staging
    # and all. On a real build that is 214 MB of package copied into a subfolder of
    # the folder holding it, and it was only visible once this test used the layout
    # a client actually has rather than two sibling folders.
    check(not os.path.exists(os.path.join(download, 'previous', 'updates')),
          "the backup did not copy the updates folder into itself")

    package_in_backup = os.path.join(download, 'previous', 'package.zip')
    check(not os.path.exists(package_in_backup),
          "and the package was not copied into the backup beside it")


def aPackageWithNoClientInItChangesNothing():
    def package(download):
        staged = os.path.join(WORK, 'make', 'something-else')
        os.makedirs(staged)
        io.open(os.path.join(staged, 'readme.txt'), 'w').write('not a build')
        shutil.make_archive(os.path.join(download, 'package'), 'zip',
                            os.path.join(WORK, 'make'))

    code, contents, install, download = run_updater(package)

    check(code != 0, "a package with no client in it fails rather than reporting success")
    check(contents == 'OLD BUILD 208', "and the installation is exactly as it was")
    check(not os.path.exists(os.path.join(download, 'previous')),
          "nothing was even backed up, because nothing was going to be replaced")


def somethingThatIsNotAZipChangesNothing():
    def package(download):
        io.open(os.path.join(download, 'package.zip'), 'w').write('not a zip file')

    code, contents, install, download = run_updater(package)

    check(code != 0, "an unreadable package fails")
    check(contents == 'OLD BUILD 208', "and changes nothing")


def theRefusalsAreNotJustAlwaysFailing():
    # Without this the two above would pass just as well if the script refused
    # everything, which would be a client that can never update.
    code, contents, install, download = run_updater(good_package)

    check(code == 0, "a good package still installs")
    check(contents == 'NEW BUILD 209', "and does replace the executable")


def main():
    print("Client updater")

    if os.name != 'nt':
        # The script is Windows batch. Nothing to say on another platform, and
        # failing there would be noise rather than a finding.
        print("\n0 passed, 0 failed  (Windows only)")
        return 0

    theScriptComesOutOfTheSource()
    aNewBuildActuallyReplacesTheOldOne()
    aPackageWithNoClientInItChangesNothing()
    somethingThatIsNotAZipChangesNothing()
    theRefusalsAreNotJustAlwaysFailing()

    if os.path.exists(WORK):
        shutil.rmtree(WORK, ignore_errors=True)

    print("\n%d passed, %d failed" % (len(checks) - len(failures), len(failures)))

    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
