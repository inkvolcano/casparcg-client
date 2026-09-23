@echo off
REM Builds and runs tools/test-rundowntiming.cpp: rundown timing arithmetic: hard
REM out, over and under, clip and duration lengths. No network.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set QT=C:\Qt\6.5.3\msvc2019_64
set HERE=%~dp0
set OUT=%TEMP%\casparcg-test-rundowntiming
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /MD /W3 ^
   /Fo"%OUT%\\" /Fe"%OUT%\test-rundowntiming.exe" ^
   "%HERE%test-rundowntiming.cpp" ^
   /I"%HERE%..\src" /I"%HERE%..\src\Common" ^
   /I"%QT%\include" /I"%QT%\include\QtCore" ^
   /link /LIBPATH:"%QT%\lib" Qt6Core.lib
if errorlevel 1 exit /b 1

set PATH=%QT%\bin;%PATH%
"%OUT%\test-rundowntiming.exe"
