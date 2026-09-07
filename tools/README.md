# tools

Everything here is for the person working on the client. None of it is part of the
application, none of it is shipped, and none of it writes to the build directory, a
real templates folder, or the push tool's saved settings.

## Before you build

```
python tools/check-all.py
```

Type-checks what changed and runs every test suite. Takes a few minutes, mostly
spent starting servers. `--fast` skips the ones that need a network and finishes in
under a minute. `--list` says what it would run and why.

It exits non-zero if anything failed, so it can go in a hook or a task.

## What each thing is

| | |
|---|---|
| `check-all.py` | one command that runs everything below |
| `check-cmake.py` | every .cpp under src/ is named in a CMakeLists, every change script is in Core.qrc, and the version matches. Catches what compiling a file directly cannot |
| `syntax-check.py` | type-checks changed files with `/Zs`, and finds members declared but never defined. Not a build: no object files, no linking |
| `test-paths.bat` | path rules, protected files, git digests, the bad-token throttle |
| `test-install.bat` | `installFile` against a real folder, and a sweep for anything that landed outside it |
| `test-target.bat` | how the push tool reads an address and every URL it builds from one |
| `test-layout.bat` | panel heights kept inside the screen, so a window can never be taller than the display |
| `test-assign.bat` | who decides which packs a machine takes, and the empty-list case that must not mean "everything" |
| `test-server.bat` | the HTTP parser, over real sockets, with malformed requests |
| `test-push.bat` | a whole push, clicked through the buttons, to a real client |
| `test-pull.bat` | the pull route end to end against a real PHP relay |
| `test-github.bat` | the GitHub pull route against `mock-github.php` |
| `php/` | everything uploaded to a web host rather than built: the relay, and the sheets cache and collector |
| `mock-github.php` | just enough of the GitHub API to run the pull route against |
| `test-paths-stubs.cpp` | test doubles for the database, so settings come from the environment |

The four network suites need PHP. Without it they say so and are reported as
skipped rather than passed.

## Writing a test here

Two habits, both learned the hard way in this repo.

**Break the thing you are testing and watch the test fail.** Twice a check here
passed for a reason that had nothing to do with what it claimed to cover: once
because a character rule rejected the input before the rule under test saw it, once
because a filter was quietly keeping a file out. Both looked fine until the code
they were meant to guard was switched off and nothing happened.

**Assert on the result, not on the return code.** The check that found the worst
problem in this work was not "did that call return 400". It was a sweep of the
whole sandbox asking whether any file had landed somewhere it should not have.
