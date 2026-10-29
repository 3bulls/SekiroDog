@echo off
rem Configure and build with Ninja in the VS 2026 dev environment (Debug: build.cmd x64-debug)
setlocal
set NoDefaultCurrentDirectoryInExePath=
set PRESET=%1
if "%PRESET%"=="" set PRESET=x64-release
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
cmake --preset %PRESET% || exit /b 1
cmake --build --preset %PRESET%
