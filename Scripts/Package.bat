@echo off
setlocal enabledelayedexpansion

pushd "%~dp0.."
set ROOT_DIR=%CD%

set PROJECT_PATH=%1
if "%PROJECT_PATH%"=="" set PROJECT_PATH=Templates\Playground3D\Project.dfproj

for %%F in ("%PROJECT_PATH%") do (
    set PROJECT_DIR=%%~dpF
    set PROJECT_NAME=%%~nF
)
if /I "%PROJECT_NAME%"=="Project" (
    for %%D in ("%PROJECT_DIR:~0,-1%") do set PROJECT_NAME=%%~nD
)

echo ========================================================
echo [DarkFire] Packaging Project: %PROJECT_NAME%
echo [DarkFire] Project File:     %PROJECT_PATH%
echo ========================================================

:: 1. Build Dist configuration
call "%ROOT_DIR%\Scripts\Build.bat" Dist
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Dist build failed!
    popd
    exit /b %ERRORLEVEL%
)

:: 2. Locate dfcook binary
set DFCOOK_EXE=%ROOT_DIR%\bin\Dist-windows-x86_64\Tools\dfcook\dfcook.exe
if not exist "%DFCOOK_EXE%" (
    set DFCOOK_EXE=%ROOT_DIR%\bin\Release-windows-x86_64\Tools\dfcook\dfcook.exe
)
if not exist "%DFCOOK_EXE%" (
    set DFCOOK_EXE=%ROOT_DIR%\bin\Debug-windows-x86_64\Tools\dfcook\dfcook.exe
)
if not exist "%DFCOOK_EXE%" (
    echo [ERROR] dfcook.exe not found!
    popd
    exit /b 1
)

:: 3. Setup package destination directory
set PACKAGE_DIR=%ROOT_DIR%\Package\%PROJECT_NAME%
if exist "%PACKAGE_DIR%" (
    echo [DarkFire] Cleaning previous package directory: %PACKAGE_DIR%
    rmdir /S /Q "%PACKAGE_DIR%"
)
mkdir "%PACKAGE_DIR%"

:: 4. Run asset cooker to produce cooked shaders, textures, and .pak files
echo [DarkFire] Cooking assets and creating .pak archives...
"%DFCOOK_EXE%" --project "%PROJECT_PATH%" --output "%PACKAGE_DIR%" --all
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Asset cooking failed!
    popd
    exit /b %ERRORLEVEL%
)

:: 5. Copy Dist executable and module
set PLAYER_EXE=%ROOT_DIR%\bin\Dist-windows-x86_64\Player\Player.exe
if not exist "%PLAYER_EXE%" (
    set PLAYER_EXE=%ROOT_DIR%\bin\Release-windows-x86_64\Player\Player.exe
)
if exist "%PLAYER_EXE%" (
    copy /Y "%PLAYER_EXE%" "%PACKAGE_DIR%\%PROJECT_NAME%.exe" >nul
    echo [DarkFire] Packaged executable: %PROJECT_NAME%.exe
)

:: Copy game module DLL if dynamic
set MODULE_DLL=%ROOT_DIR%\bin\Dist-windows-x86_64\Templates\%PROJECT_NAME%\%PROJECT_NAME%.dll
if not exist "%MODULE_DLL%" (
    set MODULE_DLL=%ROOT_DIR%\bin\Release-windows-x86_64\Templates\%PROJECT_NAME%\%PROJECT_NAME%.dll
)
if not exist "%MODULE_DLL%" (
    set MODULE_DLL=%ROOT_DIR%\bin\Debug-windows-x86_64\Templates\%PROJECT_NAME%\%PROJECT_NAME%.dll
)
if exist "%MODULE_DLL%" (
    copy /Y "%MODULE_DLL%" "%PACKAGE_DIR%\%PROJECT_NAME%.dll" >nul
    echo [DarkFire] Packaged module: %PROJECT_NAME%.dll
)

:: Copy DarkFire core DLL if dynamic build
set ENGINE_DLL=%ROOT_DIR%\bin\Dist-windows-x86_64\DarkFire\DarkFire.dll
if not exist "%ENGINE_DLL%" set ENGINE_DLL=%ROOT_DIR%\bin\Release-windows-x86_64\DarkFire\DarkFire.dll
if not exist "%ENGINE_DLL%" set ENGINE_DLL=%ROOT_DIR%\bin\Debug-windows-x86_64\DarkFire\DarkFire.dll
if exist "%ENGINE_DLL%" (
    copy /Y "%ENGINE_DLL%" "%PACKAGE_DIR%\DarkFire.dll" >nul
    echo [DarkFire] Packaged engine core: DarkFire.dll
)

:: 6. Copy Project.dfproj into package root for player discovery
copy /Y "%PROJECT_PATH%" "%PACKAGE_DIR%\Project.dfproj" >nul

:: 7. Copy ThirdParty licenses
if exist "%ROOT_DIR%\Docs\Licenses" (
    mkdir "%PACKAGE_DIR%\Docs\Licenses" 2>nul
    xcopy /E /I /Y "%ROOT_DIR%\Docs\Licenses" "%PACKAGE_DIR%\Docs\Licenses" >nul
    echo [DarkFire] Packaged Third-Party Licenses.
)

:: 8. Create version.txt
if exist "%ROOT_DIR%\VERSION" (
    copy /Y "%ROOT_DIR%\VERSION" "%PACKAGE_DIR%\version.txt" >nul
) else (
    echo 2.0.0-phase6 > "%PACKAGE_DIR%\version.txt"
)

:: Ensure no d3dcompiler_47.dll or PDB files are in distribution
if exist "%PACKAGE_DIR%\d3dcompiler_47.dll" del "%PACKAGE_DIR%\d3dcompiler_47.dll"
del /Q "%PACKAGE_DIR%\*.pdb" 2>nul

echo ========================================================
echo [DarkFire] Package successfully created at:
echo           %PACKAGE_DIR%
echo ========================================================
popd
exit /b 0
