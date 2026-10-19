@echo off
if "%~1"=="" (
    echo [ERROR] Project name required!
    echo Usage: Create-Project.bat ^<ProjectName^> [WindowedApp^|ConsoleApp]
    echo Example: Create-Project.bat KartGame
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Create-Project.ps1" %*
