@echo off
REM Builds tools/probe-livestream.cpp: plays the Live panel's UDP stream with
REM Qt Multimedia and reports the first frame, frame rate, size and CPU.
REM Run it with tools/test-livestream.py, which asks a local server for the stream.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set QT=C:\Qt\6.5.3\msvc2019_64
set HERE=%~dp0
set OUT=%TEMP%\casparcg-probe-livestream
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /MD /W3 ^
   /Fo"%OUT%\\" /Fe"%OUT%\probe-livestream.exe" ^
   "%HERE%probe-livestream.cpp" ^
   /I"%QT%\include" /I"%QT%\include\QtCore" /I"%QT%\include\QtGui" /I"%QT%\include\QtMultimedia" ^
   /link /LIBPATH:"%QT%\lib" Qt6Core.lib Qt6Gui.lib Qt6Multimedia.lib
if errorlevel 1 exit /b 1
echo built %OUT%\probe-livestream.exe
