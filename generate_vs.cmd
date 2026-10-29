@echo off
rem Generate the Visual Studio 2026 solution: build\vs2026\SekiroDog.sln(x)
rem Usage: generate_vs.cmd        generate only
rem        generate_vs.cmd open   generate, then open it in Visual Studio
setlocal
set NoDefaultCurrentDirectoryInExePath=
cd /d "%~dp0"

if not exist "ThirdParty\DirectXTK\CMakeLists.txt" (
    echo Fetching submodules...
    git submodule update --init --recursive || exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -version [18.0^,19.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo Visual Studio 2026 with the "Desktop development with C++" workload was not found.
    exit /b 1
)

rem Use the CMake bundled with VS (it knows the "Visual Studio 18 2026" generator)
set "CMAKE=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
"%CMAKE%" --preset vs2026 || exit /b 1

set "SLN="
for %%f in (build\vs2026\SekiroDog.sln build\vs2026\SekiroDog.slnx) do if exist "%%f" set "SLN=%%f"
echo.
echo Generated: %~dp0%SLN%
if /i "%~1"=="open" start "" "%SLN%"
