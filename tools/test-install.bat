@echo off
REM Builds and runs tools/test-install.cpp against the real TemplateInstaller.
REM Writes only into a temporary folder of its own; touches neither the project's
REM build directory nor any real templates folder.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set QT=C:\Qt\6.5.3\msvc2019_64
set HERE=%~dp0
set OUT=%TEMP%\casparcg-test-install
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /MD /Gy /W3 ^
   /Fo"%OUT%\\" /Fe"%OUT%\test-install.exe" ^
   "%HERE%test-install.cpp" "%HERE%test-paths-stubs.cpp" ^
   "%HERE%..\src\Widgets\TemplateInstaller.cpp" "%HERE%..\src\Core\Models\ConfigurationModel.cpp" ^
   /I"%HERE%..\src" /I"%HERE%..\src\Widgets" /I"%HERE%..\src\Core" /I"%HERE%..\src\Core\Models" ^
   /I"%HERE%..\src\Common" /I"%QT%\include" /I"%QT%\include\QtCore" /I"%QT%\include\QtSql" ^
   /DWIDGETS_LIBRARY /DCORE_LIBRARY ^
   /link /OPT:REF /LIBPATH:"%QT%\lib" Qt6Core.lib
if errorlevel 1 exit /b 1

set PATH=%QT%\bin;%PATH%
"%OUT%\test-install.exe"
