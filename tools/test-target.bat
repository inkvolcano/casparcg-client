@echo off
REM Builds and runs tools/test-target.cpp: how the push tool reads an address and
REM every URL it builds from one. No network, no files, nothing to clean up.

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set QT=C:\Qt\6.5.3\msvc2019_64
set HERE=%~dp0
set OUT=%TEMP%\casparcg-test-target
if not exist "%OUT%" mkdir "%OUT%"

"%QT%\bin\moc.exe" "%HERE%..\src\Push\PushWindow.h" -o "%OUT%\moc_PushWindow.cpp"
if errorlevel 1 exit /b 1

cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /MD /Gy /W3 ^
   /Fo"%OUT%\\" /Fe"%OUT%\test-target.exe" ^
   "%HERE%test-target.cpp" "%HERE%..\src\Push\PushWindow.cpp" "%OUT%\moc_PushWindow.cpp" ^
   /I"%HERE%..\src" /I"%HERE%..\src\Push" ^
   /I"%QT%\include" /I"%QT%\include\QtCore" /I"%QT%\include\QtGui" ^
   /I"%QT%\include\QtWidgets" /I"%QT%\include\QtNetwork" ^
   /link /OPT:REF /LIBPATH:"%QT%\lib" Qt6Core.lib Qt6Gui.lib Qt6Widgets.lib Qt6Network.lib
if errorlevel 1 exit /b 1

set PATH=%QT%\bin;%PATH%
"%OUT%\test-target.exe"
