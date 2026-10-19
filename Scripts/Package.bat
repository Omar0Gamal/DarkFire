@echo off
setlocal enabledelayedexpansion

pushd "%~dp0.."
set ROOT_DIR=%CD%

set PROJECT_PATH=%1
if "%PROJECT_PATH%"=="" set PROJECT_PATH=Templates\Playground3D\Project.dfproj

echo [DarkFire] Packaging project: %PROJECT_PATH%...

call "%ROOT_DIR%\Scripts\Build.bat" Dist
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Dist build failed!
    popd
    exit /b %ERRORLEVEL%
)

echo [DarkFire] Packaging pipeline ready.
popd
exit /b 0
