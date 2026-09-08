@echo off
REM Builds and runs tools/test-server.cpp: the real SheetCacheServer on a spare
REM port, driven over real sockets, with the installer pointed at a temporary
REM folder. Touches nothing the application uses.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set QT=C:\Qt\6.5.3\msvc2019_64
set HERE=%~dp0
set OUT=%TEMP%\casparcg-test-server
if not exist "%OUT%" mkdir "%OUT%"

REM SheetCacheServer declares Q_OBJECT, so its moc output is part of the build.
"%QT%\bin\moc.exe" "%HERE%..\src\Widgets\SheetCacheServer.h" -o "%OUT%\moc_SheetCacheServer.cpp"
"%QT%\bin\moc.exe" "%HERE%..\src\Widgets\SheetDataResolver.h" -o "%OUT%\moc_SheetDataResolver.cpp"
"%QT%\bin\moc.exe" "%HERE%..\src\Widgets\SheetsProjectRegistry.h" -o "%OUT%\moc_SheetsProjectRegistry.cpp"
if errorlevel 1 exit /b 1

cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /MD /Gy /W3 ^
   /Fo"%OUT%\\" /Fe"%OUT%\test-server.exe" ^
   "%HERE%test-server.cpp" "%HERE%test-paths-stubs.cpp" ^
   "%HERE%..\src\Widgets\SheetCacheServer.cpp" "%OUT%\moc_SheetCacheServer.cpp" ^
   "%HERE%..\src\Widgets\SheetDataResolver.cpp" "%OUT%\moc_SheetDataResolver.cpp" ^
   "%HERE%..\src\Widgets\SheetsProjectRegistry.cpp" "%OUT%\moc_SheetsProjectRegistry.cpp" ^
   "%HERE%..\src\Widgets\TemplateInstaller.cpp" "%HERE%..\src\Core\Models\ConfigurationModel.cpp" "%HERE%..\src\Core\Models\DeviceModel.cpp" ^
   /I"%HERE%..\src" /I"%HERE%..\src\Widgets" /I"%HERE%..\src\Core" /I"%HERE%..\src\Core\Models" ^
   /I"%HERE%..\src\Common" /I"%QT%\include" /I"%QT%\include\QtCore" /I"%QT%\include\QtNetwork" ^
   /I"%QT%\include\QtSql" ^
   /DWIDGETS_LIBRARY /DCORE_LIBRARY ^
   /link /OPT:REF /LIBPATH:"%QT%\lib" Qt6Core.lib Qt6Network.lib
if errorlevel 1 exit /b 1

set PATH=%QT%\bin;%PATH%
"%OUT%\test-server.exe"
