@echo off
REM Builds and runs tools/test-paths.cpp against the real TemplateInstaller.
REM
REM Its own little binary in a temp folder: it does not touch the project's build
REM directory and is not part of the application.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set QT=C:\Qt\6.5.3\msvc2019_64
set HERE=%~dp0
set OUT=%TEMP%\casparcg-test-paths
if not exist "%OUT%" mkdir "%OUT%"

REM /Gy plus /OPT:REF so the functions that reach the database are discarded
REM rather than dragging the whole application in behind them.
cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /MD /Gy /W3 ^
   /Fo"%OUT%\\" /Fe"%OUT%\test-paths.exe" ^
   "%HERE%test-paths.cpp" "%HERE%test-paths-stubs.cpp" "%HERE%..\src\Widgets\TemplateInstaller.cpp" "%HERE%..\src\Core\Models\ConfigurationModel.cpp" "%HERE%..\src\Core\Models\DeviceModel.cpp" ^
   /I"%HERE%..\src" /I"%HERE%..\src\Widgets" /I"%HERE%..\src\Core" /I"%HERE%..\src\Common" /I"%HERE%..\src\Core\Models" /I"%QT%\include\QtSql" ^
   /I"%QT%\include" /I"%QT%\include\QtCore" ^
   /DWIDGETS_LIBRARY /DCORE_LIBRARY ^
   /link /OPT:REF /LIBPATH:"%QT%\lib" Qt6Core.lib
if errorlevel 1 exit /b 1

set PATH=%QT%\bin;%PATH%
"%OUT%\test-paths.exe"
