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

echo [DarkFire] Detected Visual Studio Version: %VS_VERSION%

set PREMAKE_ACTION=vs2022
for /f "tokens=1 delims=." %%a in ("%VS_VERSION%") do (
    if %%a geq 18 (
        set PREMAKE_ACTION=vs2026
    ) else (
        set PREMAKE_ACTION=vs2022
    )
)

if not exist "%ROOT_DIR%\ThirdParty\DirectXTex\DirectXTex\Shaders\Compiled\BC7Encode_EncodeBlockCS.inc" (
    echo [DarkFire] Compiling DirectXTex shaders...
    pushd "%ROOT_DIR%\ThirdParty\DirectXTex\DirectXTex\Shaders"
    set CompileShadersOutput=%ROOT_DIR%\ThirdParty\DirectXTex\DirectXTex\Shaders\Compiled
    call CompileShaders.cmd
    popd
)

if not exist "%ROOT_DIR%\ThirdParty\DirectXTK\Src\Shaders\Compiled\SpriteEffect_SpriteVertexShader.inc" (
    echo [DarkFire] Compiling DirectXTK shaders...
    pushd "%ROOT_DIR%\ThirdParty\DirectXTK\Src\Shaders"
    set CompileShadersOutput=%ROOT_DIR%\ThirdParty\DirectXTK\Src\Shaders\Compiled
    call CompileShaders.cmd
    popd
)

echo [DarkFire] Running Premake with action: %PREMAKE_ACTION%
"%ROOT_DIR%\ThirdParty\Premake\Windows\premake5.exe" --file="%ROOT_DIR%\premake5.lua" %PREMAKE_ACTION%

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Premake generation failed!
    popd
    exit /b %ERRORLEVEL%
)

echo [DarkFire] Setup completed successfully!
popd
exit /b 0
