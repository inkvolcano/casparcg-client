# The standalone updater, for clients too old to update themselves.
#
# Install and Restart arrived in build 210. Every client older than that can find
# and download a build but cannot swap itself, so tools/install-update.cmd is the
# one-time hop - and it is the file a person will run, unsupervised, on a machine
# that may be on air. It gets run here before it is run there.
#
# It differs from the script the client generates in one way worth testing on its
# own: nothing is told to it. It works out the installation from its own location
# and picks the newest package sitting beside it, so the two ways a person can get
# this wrong - wrong folder, wrong zip - are the two things to check.

import io
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, 'install-update.cmd')
# Per process, for the same reason as test-updater: two overlapping runs sharing
# one folder fail each other, and the failure reads as a broken updater.
WORK = os.path.join(os.environ.get('TEMP', r'C:\tmp'),
                    'casparcg-test-bootstrap-%d' % os.getpid())

failures = []
checks = []


def check(ok, what):
    checks.append(what)
    if not ok:
        failures.append(what)
        print("  FAIL  " + what)


def make_package(into, name, exe_text='NEW BUILD 210', with_exe=True):
    """A package shaped the way publish-build.ps1 shapes one: a folder inside."""
    make = os.path.join(WORK, 'make')
    if os.path.exists(make):
        shutil.rmtree(make)

    staged = os.path.join(make, 'casparcg-client-v2.3.1-210')
    os.makedirs(staged)

    if with_exe:
        io.open(os.path.join(staged, 'casparcg-client.exe'), 'w').write(exe_text)
    io.open(os.path.join(staged, 'Qt6Core.dll'), 'w').write('new qt')

    archive = shutil.make_archive(os.path.join(WORK, 'tmp-package'), 'zip', make)
    shutil.move(archive, os.path.join(into, name))


def setup(install_exe='OLD BUILD 209', extra=True):
    """An installation with an updates folder inside it, the way a client has."""
    if os.path.exists(WORK):
        shutil.rmtree(WORK)

    install = os.path.join(WORK, 'CasparCG Client')
    updates = os.path.join(install, 'updates')
    os.makedirs(updates)

    if install_exe is not None:
        io.open(os.path.join(install, 'casparcg-client.exe'), 'w').write(install_exe)
    io.open(os.path.join(install, 'Qt6Core.dll'), 'w').write('old qt')

    if extra:
        os.makedirs(os.path.join(install, 'Data'))
        io.open(os.path.join(install, 'Data', 'local.db'), 'w').write('the database')

    shutil.copy2(SCRIPT, os.path.join(updates, 'install-update.cmd'))

    return install, updates


def run(updates):
    result = subprocess.run(
        ['cmd.exe', '/c', os.path.join(updates, 'install-update.cmd')],
        input='\n', capture_output=True, text=True, timeout=180)

    return result.returncode, (result.stdout or '')


def exe_in(install):
    path = os.path.join(install, 'casparcg-client.exe')
    return io.open(path).read() if os.path.exists(path) else '<missing>'


def itFindsTheInstallationAndThePackageOnItsOwn():
    install, updates = setup()
    make_package(updates, 'casparcg-client-v2.3.1-210-windows.zip')

    code, out = run(updates)

    check(code == 0, "it ran without an error")
    check(exe_in(install) == 'NEW BUILD 210',
          "it replaced the executable (found: %s)" % exe_in(install))

    # The mistake this whole file exists to prevent.
    check(not os.path.exists(os.path.join(install, 'casparcg-client-v2.3.1-210')),
          "the folder inside the package was not copied in as a folder")

    check(os.path.exists(os.path.join(install, 'Data', 'local.db')),
          "files the new build does not carry were left alone")

    previous = os.path.join(updates, 'previous', 'casparcg-client.exe')
    check(os.path.exists(previous) and io.open(previous).read() == 'OLD BUILD 209',
          "the build it replaced is kept")


def itTakesTheNewestPackageWhenThereAreSeveral():
    # A machine that has been updated a few times has several zips in there, and
    # installing the oldest would be a silent downgrade.
    install, updates = setup()

    make_package(updates, 'casparcg-client-v2.3.1-207-windows.zip', exe_text='BUILD 207')
    # Make sure the timestamps genuinely differ rather than relying on order.
    os.utime(os.path.join(updates, 'casparcg-client-v2.3.1-207-windows.zip'),
             (1000000000, 1000000000))
    make_package(updates, 'casparcg-client-v2.3.1-210-windows.zip', exe_text='BUILD 210')

    code, out = run(updates)

    check(code == 0, "it ran with several packages present")
    check(exe_in(install) == 'BUILD 210',
          "it took the newest, not the first (found: %s)" % exe_in(install))


def itRefusesFromTheWrongFolder():
    # Somebody puts it on the desktop and double-clicks it. There is no client
    # above the desktop, and guessing would be worse than stopping.
    if os.path.exists(WORK):
        shutil.rmtree(WORK)

    loose = os.path.join(WORK, 'somewhere', 'else')
    os.makedirs(loose)
    shutil.copy2(SCRIPT, os.path.join(loose, 'install-update.cmd'))
    make_package(loose, 'casparcg-client-v2.3.1-210-windows.zip')

    code, out = run(loose)

    check(code != 0, "run from the wrong folder it stops")
    check('no casparcg-client.exe' in out.lower() or 'there is no casparcg' in out.lower(),
          "and says the client is not where it expected")


def itRefusesWithNothingToInstall():
    install, updates = setup()

    code, out = run(updates)

    check(code != 0, "with no package beside it, it stops")
    check(exe_in(install) == 'OLD BUILD 209', "and changes nothing")


def itRefusesAZipThatIsNotAClient():
    install, updates = setup()
    make_package(updates, 'casparcg-client-v2.3.1-210-windows.zip', with_exe=False)

    code, out = run(updates)

    check(code != 0, "a zip with no client in it is refused")
    check(exe_in(install) == 'OLD BUILD 209', "and the installation is untouched")
    check(not os.path.exists(os.path.join(install, 'Qt6Core.dll')) or
          io.open(os.path.join(install, 'Qt6Core.dll')).read() == 'old qt',
          "nothing from it was copied in either")


def main():
    print("Standalone updater")

    if os.name != 'nt':
        print("\n0 passed, 0 failed  (Windows only)")
        return 0

    itFindsTheInstallationAndThePackageOnItsOwn()
    itTakesTheNewestPackageWhenThereAreSeveral()
    itRefusesFromTheWrongFolder()
    itRefusesWithNothingToInstall()
    itRefusesAZipThatIsNotAClient()

    if os.path.exists(WORK):
        shutil.rmtree(WORK, ignore_errors=True)

    print("\n%d passed, %d failed" % (len(checks) - len(failures), len(failures)))

    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
