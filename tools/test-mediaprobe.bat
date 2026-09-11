@echo off
setlocal EnableDelayedExpansion
REM Builds tools/test-mediaprobe.cpp and opens a file with each Qt Multimedia
REM API in a process of its own, so a plugin fault is an exit code here rather
REM than the client disappearing. Compares against a known-good clip.
REM
REM   test-mediaprobe.bat "C:\CasparCG\media\BLUE Background_.webm" "C:\CasparCG\media\012 INST MORPH GEERT - NETANYAHU.mov"

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set QT=C:\Qt\6.5.3\msvc2019_64
set HERE=%~dp0
set OUT=%TEMP%\casparcg-test-mediaprobe
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /EHsc /std:c++17 /Zc:__cplusplus /permissive- /MD /W3 ^
   /Fo"%OUT%\\" /Fe"%OUT%\test-mediaprobe.exe" ^
   "%HERE%test-mediaprobe.cpp" ^
   /I"%QT%\include" /I"%QT%\include\QtCore" /I"%QT%\include\QtMultimedia" ^
   /link /LIBPATH:"%QT%\lib" Qt6Core.lib Qt6Multimedia.lib
if errorlevel 1 exit /b 1

set PATH=%QT%\bin;%PATH%
set QT_PLUGIN_PATH=%QT%\plugins

for %%F in (%*) do (
    for %%M in (player decoder) do (
        echo.
        echo === %%M  %%~nxF
        "%OUT%\test-mediaprobe.exe" %%M "%%~F"
        call :say %%M "%%~nxF" !errorlevel!
    )
)
exit /b 0

:say
setlocal
set CODE=%~3
if "%CODE%"=="0" (echo   -^> ok) else if "%CODE%"=="2" (echo   -^> reported an error, no crash) else if "%CODE%"=="3" (echo   -^> timed out) else (echo   -^> EXIT %CODE%  -- the plugin faulted)
endlocal
exit /b 0
