@echo off
REM Runs tools/test-updater.py: reconstructs the updater script out of
REM UpdateDialog.cpp and runs it against a throwaway installation. No compiler, no
REM network, no application - but it does write to a folder under %TEMP%.

python "%~dp0test-updater.py"
