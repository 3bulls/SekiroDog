@echo off
rem Command-line build of SekiroDog.sln with MSBuild.
rem Usage: build.cmd [Debug|Release]   (default: Release)
setlocal
set NoDefaultCurrentDirectoryInExePath=
cd /d "%~dp0"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "MSBUILD="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -version [18.0^,19.0^) -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
    echo Visual Studio 2026 with the "Desktop development with C++" workload was not found.
    exit /b 1
)

"%MSBUILD%" SekiroDog.sln /m /nologo /v:minimal /p:Configuration=%CONFIG% /p:Platform=x64 || exit /b 1
echo.
echo Built: %~dp0bin\x64\%CONFIG%\SekiroDog.exe
