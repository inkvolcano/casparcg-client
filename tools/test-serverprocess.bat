@echo off
REM Builds and runs tools/test-serverprocess.cpp: finding, starting and stopping a
REM server process by its executable's path, against a real stand-in process.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set QT=C:\Qt\6.5.3\msvc2019_64
cd /d "%~dp0"
set OUT=%TEMP%\casparcg-test-serverprocess
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /MD /W3 /DWIDGETS_LIBRARY ^
   /Fo"%OUT%\\" /Fe"%OUT%\test-serverprocess.exe" ^
   test-serverprocess.cpp ..\src\Widgets\ServerProcessControl.cpp ^
   /I..\src\Widgets /I..\src ^
   /I"%QT%\include" /I"%QT%\include\QtCore" ^
   /link /LIBPATH:"%QT%\lib" Qt6Core.lib >"%OUT%\build.log" 2>&1
if errorlevel 1 (
    type "%OUT%\build.log" | findstr /i "error"
    exit /b 1
)

set PATH=%QT%\bin;%PATH%
"%OUT%\test-serverprocess.exe"
