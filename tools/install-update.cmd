@echo off
REM Put a downloaded build in place, on a client too old to do it itself.
REM
REM Install and Restart arrived in build 210. A client older than that can find an
REM update and download it - Check for Updates has been there since 207 - but has
REM no way to swap itself, so this is the one-time hop.
REM
REM   1. In the client: Help - Check for Updates - Download, then Show Download.
REM   2. Put this file in the folder that opens, beside the .zip.
REM   3. Close the client.
REM   4. Double-click this.
REM
REM After that the client can do it on its own and this file is not needed again.
REM
REM It works out where it is rather than being told: the installation is the folder
REM above this one, and the package is the newest client zip sitting beside it.
REM Nothing is typed in, so nothing can be typed wrong.

setlocal
title CasparCG Client update

echo.
echo   Putting a downloaded build in place.
echo.

REM ---- where -----------------------------------------------------------------

set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"

for %%I in ("%HERE%\..") do set "INSTALL=%%~fI"

if not exist "%INSTALL%\casparcg-client.exe" (
    echo   ! There is no casparcg-client.exe in:
    echo       %INSTALL%
    echo.
    echo     This file has to sit in the "updates" folder inside the client's own
    echo     folder - the one Show Download opens. Nothing has been changed.
    echo.
    pause
    exit /b 1
)

REM The newest client package beside this file. Newest rather than a name, because
REM the name carries a version that changes every time.
set "ZIP="
for /f "delims=" %%F in ('dir /b /o-d "%HERE%\casparcg-client-*.zip" 2^>nul') do (
    if not defined ZIP set "ZIP=%HERE%\%%F"
)

if not defined ZIP (
    echo   ! No casparcg-client-*.zip next to this file, so there is nothing to
    echo     install. Download one first: Help - Check for Updates - Download.
    echo.
    pause
    exit /b 1
)

echo   installing  %ZIP%
echo   over        %INSTALL%
echo.

set "WORK=%HERE%\staged"
set "BACKUP=%HERE%\previous"

REM ---- wait for the client to be gone ----------------------------------------
REM
REM Windows will not replace a running executable. Copying while the client is up
REM replaces every library around it and skips the one file that decides the
REM version, which leaves a client that starts, works, and reports the old number.

echo   Waiting for the client to close...
set /a TRIES=0

:waitloop
set /a TRIES+=1
if %TRIES% GTR 60 goto :stillrunning
tasklist /fi "IMAGENAME eq casparcg-client.exe" 2>nul | find /i "casparcg-client.exe" >nul
if errorlevel 1 goto :closed
ping -n 2 127.0.0.1 >nul
goto :waitloop

:stillrunning
echo.
echo   ! The client is still running after a minute. Nothing has been changed.
echo     Close it and run this again.
echo.
pause
exit /b 1

:closed

REM ---- unpack ----------------------------------------------------------------

echo   Unpacking...
if exist "%WORK%" rmdir /s /q "%WORK%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -LiteralPath '%ZIP%' -DestinationPath '%WORK%' -Force"
if errorlevel 1 goto :unpackfailed

REM The package holds ONE FOLDER named for the build. Copying that folder instead
REM of what is inside it is the mistake that leaves the old executable in place -
REM which is exactly what happened by hand and why this file exists.
set "SOURCE=%WORK%"
for /d %%D in ("%WORK%\*") do set "SOURCE=%%~fD"

if not exist "%SOURCE%\casparcg-client.exe" goto :noexe

REM ---- keep what is there ----------------------------------------------------

echo   Backing up the build you are running...
if exist "%BACKUP%" rmdir /s /q "%BACKUP%"
REM Excluding this folder: it lives INSIDE the installation, so backing the
REM installation up into it without this copies the folder into itself - the
REM package, the unpacked staging and all.
robocopy "%INSTALL%" "%BACKUP%" /E /XD "%HERE%" /NFL /NDL /NP /NJH /NJS /R:1 /W:1 >nul
if errorlevel 8 goto :backupfailed

echo   Installing...
robocopy "%SOURCE%" "%INSTALL%" /E /NFL /NDL /NP /NJH /NJS /R:3 /W:2 >nul
if errorlevel 8 goto :copyfailed

echo.
echo   Done. Starting the new build.
echo   The one it replaced is in: %BACKUP%
echo.
start "" "%INSTALL%\casparcg-client.exe"
exit /b 0

REM ---- everything that can go wrong ------------------------------------------

:unpackfailed
echo.
echo   ! Could not unpack the download. Nothing has been changed.
echo.
pause
exit /b 1

:noexe
echo.
echo   ! That zip does not contain casparcg-client.exe, so it is not a client
echo     build. Nothing has been changed.
echo.
pause
exit /b 1

:backupfailed
echo.
echo   ! Could not back up the build you are running, so nothing was replaced.
echo.
pause
exit /b 1

:copyfailed
echo.
echo   ! The copy failed. Putting the previous build back...
robocopy "%BACKUP%" "%INSTALL%" /E /NFL /NDL /NP /NJH /NJS /R:3 /W:2 >nul
if errorlevel 8 (
    echo     The restore ALSO failed. The build you were running is in:
    echo       %BACKUP%
) else (
    echo     The previous build is back. Nothing has changed.
)
echo.
pause
exit /b 1
