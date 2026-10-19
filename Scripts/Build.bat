@echo off
setlocal enabledelayedexpansion

pushd "%~dp0.."
set ROOT_DIR=%CD%

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% (
    echo [ERROR] vswhere.exe not found at %VSWHERE%
    popd
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    set MSBUILD=%%i
)

if not exist "!MSBUILD!" (
    echo [ERROR] MSBuild.exe not found!
    popd
    exit /b 1
)

set SLN_FILE="%ROOT_DIR%\DarkFire.slnx"
if not exist %SLN_FILE% set SLN_FILE="%ROOT_DIR%\DarkFire.sln"

echo [DarkFire] Building !SLN_FILE! (%CONFIG% ^| x64)...
"!MSBUILD!" !SLN_FILE! /p:Configuration=%CONFIG% /p:Platform=x64 /m /v:m

set BUILD_RESULT=%ERRORLEVEL%
if %BUILD_RESULT% neq 0 (
    echo [ERROR] Build failed with exit code %BUILD_RESULT%
) else (
    echo [DarkFire] Build succeeded!
)

popd
exit /b %BUILD_RESULT%
