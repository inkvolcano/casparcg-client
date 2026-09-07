@echo off
REM Builds and runs tools/test-push.cpp: the push tool driven through its own
REM buttons against a real client server on a spare port. The tool's saved
REM settings are redirected to a temporary file, so the operator's client list is
REM never touched.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set QT=C:\Qt\6.5.3\msvc2019_64
set HERE=%~dp0
set OUT=%TEMP%\casparcg-test-push
if not exist "%OUT%" mkdir "%OUT%"

"%QT%\bin\moc.exe" "%HERE%..\src\Push\PushWindow.h" -o "%OUT%\moc_PushWindow.cpp"
if errorlevel 1 exit /b 1
"%QT%\bin\moc.exe" "%HERE%..\src\Widgets\SheetCacheServer.h" -o "%OUT%\moc_SheetCacheServer.cpp"
if errorlevel 1 exit /b 1
"%QT%\bin\moc.exe" "%HERE%..\src\Widgets\SheetDataResolver.h" -o "%OUT%\moc_SheetDataResolver.cpp"
if errorlevel 1 exit /b 1
"%QT%\bin\moc.exe" "%HERE%..\src\Widgets\SheetsProjectRegistry.h" -o "%OUT%\moc_SheetsProjectRegistry.cpp"
if errorlevel 1 exit /b 1

cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /MD /Gy /W3 ^
   /Fo"%OUT%\\" /Fe"%OUT%\test-push.exe" ^
   "%HERE%test-push.cpp" "%HERE%test-paths-stubs.cpp" ^
   "%HERE%..\src\Push\PushWindow.cpp" "%OUT%\moc_PushWindow.cpp" ^
   "%HERE%..\src\Widgets\SheetCacheServer.cpp" "%OUT%\moc_SheetCacheServer.cpp" ^
   "%HERE%..\src\Widgets\SheetDataResolver.cpp" "%OUT%\moc_SheetDataResolver.cpp" ^
   "%HERE%..\src\Widgets\SheetsProjectRegistry.cpp" "%OUT%\moc_SheetsProjectRegistry.cpp" ^
   "%HERE%..\src\Widgets\TemplateInstaller.cpp" ^
   "%HERE%..\src\Core\Models\ConfigurationModel.cpp" "%HERE%..\src\Core\Models\DeviceModel.cpp" ^
   /I"%HERE%..\src" /I"%HERE%..\src\Push" /I"%HERE%..\src\Widgets" /I"%HERE%..\src\Core" ^
   /I"%HERE%..\src\Core\Models" /I"%HERE%..\src\Common" ^
   /I"%QT%\include" /I"%QT%\include\QtCore" /I"%QT%\include\QtGui" ^
   /I"%QT%\include\QtWidgets" /I"%QT%\include\QtNetwork" /I"%QT%\include\QtSql" ^
   /DWIDGETS_LIBRARY /DCORE_LIBRARY ^
   /link /OPT:REF /LIBPATH:"%QT%\lib" Qt6Core.lib Qt6Gui.lib Qt6Widgets.lib Qt6Network.lib
if errorlevel 1 exit /b 1

set PATH=%QT%\bin;%PATH%
"%OUT%\test-push.exe"
