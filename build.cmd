@echo off
rem 用 VS 2026 开发环境配置并编译 Release（Debug: build.cmd x64-debug）
setlocal
set NoDefaultCurrentDirectoryInExePath=
set PRESET=%1
if "%PRESET%"=="" set PRESET=x64-release
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
cmake --preset %PRESET% || exit /b 1
cmake --build --preset %PRESET%
