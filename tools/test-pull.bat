@echo off
REM Builds and runs tools/test-pull.cpp: the real RelayClient pulling from a real
REM PHP relay started for the test on a spare port. Needs PHP on the machine.
REM Touches nothing the application uses.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set QT=C:\Qt\6.5.3\msvc2019_64
set HERE=%~dp0
set OUT=%TEMP%\casparcg-test-pull
if not exist "%OUT%" mkdir "%OUT%"

"%QT%\bin\moc.exe" "%HERE%..\src\Widgets\RelayClient.h" -o "%OUT%\moc_RelayClient.cpp"
if errorlevel 1 exit /b 1

cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /MD /Gy /W3 ^
   /Fo"%OUT%\\" /Fe"%OUT%\test-pull.exe" ^
   "%HERE%test-pull.cpp" "%HERE%test-paths-stubs.cpp" ^
   "%HERE%..\src\Widgets\RelayClient.cpp" "%OUT%\moc_RelayClient.cpp" ^
   "%HERE%..\src\Widgets\TemplateInstaller.cpp" ^
   "%HERE%..\src\Core\Models\ConfigurationModel.cpp" "%HERE%..\src\Core\Models\DeviceModel.cpp" ^
   /I"%HERE%..\src" /I"%HERE%..\src\Widgets" /I"%HERE%..\src\Core" /I"%HERE%..\src\Core\Models" ^
   /I"%HERE%..\src\Common" /I"%QT%\include" /I"%QT%\include\QtCore" /I"%QT%\include\QtNetwork" ^
   /I"%QT%\include\QtSql" ^
   /DWIDGETS_LIBRARY /DCORE_LIBRARY ^
   /link /OPT:REF /LIBPATH:"%QT%\lib" Qt6Core.lib Qt6Network.lib
if errorlevel 1 exit /b 1

REM Run from the repository root so the test can read tools/relay/relay.php.
cd /d "%HERE%.."
set PATH=%QT%\bin;%PATH%
"%OUT%\test-pull.exe"
