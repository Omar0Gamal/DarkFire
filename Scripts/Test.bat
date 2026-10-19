@echo off
setlocal enabledelayedexpansion

pushd "%~dp0.."
set ROOT_DIR=%CD%

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

set TEST_EXE="%ROOT_DIR%\bin\%CONFIG%-windows-x86_64\Tests\Tests.exe"

if not exist %TEST_EXE% (
    echo [ERROR] Tests executable not found at %TEST_EXE%!
    echo Please run Scripts\Build.bat %CONFIG% first.
    popd
    exit /b 1
)

set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist %VSWHERE% (
    for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Tools\MSVC\**\bin\Hostx64\x64`) do (
        set "PATH=%%i;!PATH!"
    )
)

echo [DarkFire] Running Tests (%CONFIG%)...
set "PATH=%ROOT_DIR%\bin\%CONFIG%-windows-x86_64\DarkFire;!PATH!"
%TEST_EXE%
set TEST_RESULT=%ERRORLEVEL%

if %TEST_RESULT% neq 0 (
    echo [ERROR] Tests failed with code %TEST_RESULT%!
) else (
    echo [DarkFire] All tests passed!
)

popd
exit /b %TEST_RESULT%
