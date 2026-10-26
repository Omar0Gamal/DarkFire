@echo off
setlocal enabledelayedexpansion

pushd "%~dp0.."
set ROOT_DIR=%CD%

set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% (
    echo [ERROR] vswhere.exe not found at %VSWHERE%
    popd
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -property installationVersion`) do (
    set VS_VERSION=%%i
)

set PREMAKE_ACTION=vs2022
for /f "tokens=1 delims=." %%a in ("%VS_VERSION%") do (
    if %%a geq 18 (
        set PREMAKE_ACTION=vs2026
    ) else (
        set PREMAKE_ACTION=vs2022
    )
)

echo [DarkFire] Regenerating solution using %PREMAKE_ACTION%...
"%ROOT_DIR%\ThirdParty\Premake\Windows\premake5.exe" --file="%ROOT_DIR%\premake5.lua" %PREMAKE_ACTION%

popd
exit /b %ERRORLEVEL%
