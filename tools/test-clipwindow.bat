@echo off
REM Builds and runs tools/test-clipwindow.cpp: in point, out point and time
REM left of a clip played with Seek and Length. No network.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set QT=C:\Qt\6.5.3\msvc2019_64
set HERE=%~dp0
set OUT=%TEMP%\casparcg-test-clipwindow
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /MD /W3 ^
   /Fo"%OUT%\\" /Fe"%OUT%\test-clipwindow.exe" ^
   "%HERE%test-clipwindow.cpp" ^
   /I"%HERE%..\src" /I"%HERE%..\src\Common" ^
   /I"%QT%\include" /I"%QT%\include\QtCore" ^
   /link /LIBPATH:"%QT%\lib" Qt6Core.lib
if errorlevel 1 exit /b 1

set PATH=%QT%\bin;%PATH%
"%OUT%\test-clipwindow.exe"
