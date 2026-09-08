@echo off
REM Builds and runs tools/test-preview.cpp: audio levels for the preview panel: how a decoded
REM file becomes meters. No network, no files, no application.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set QT=C:\Qt\6.5.3\msvc2019_64
set HERE=%~dp0
set OUT=%TEMP%\casparcg-test-preview
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /MD /W3 ^
   /Fo"%OUT%\\" /Fe"%OUT%\test-preview.exe" ^
   "%HERE%test-preview.cpp" ^
   /I"%HERE%..\src" /I"%HERE%..\src\Common" ^
   /I"%QT%\include" /I"%QT%\include\QtCore" ^
   /link /LIBPATH:"%QT%\lib" Qt6Core.lib
if errorlevel 1 exit /b 1

set PATH=%QT%\bin;%PATH%
"%OUT%\test-preview.exe"
