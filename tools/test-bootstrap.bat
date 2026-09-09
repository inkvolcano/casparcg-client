@echo off
REM Runs tools/test-bootstrap.py: the standalone updater a client too old to
REM update itself needs, run for real against a throwaway installation.

python "%~dp0test-bootstrap.py"
