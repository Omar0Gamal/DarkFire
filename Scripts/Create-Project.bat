@echo off
setlocal enabledelayedexpansion

set ROOT_DIR=%~dp0..
set DFNEW_EXE=%ROOT_DIR%\bin\Release-windows-x86_64\Tools\dfnew\dfnew.exe
if not exist "%DFNEW_EXE%" set DFNEW_EXE=%ROOT_DIR%\bin\Debug-windows-x86_64\Tools\dfnew\dfnew.exe

if not exist "%DFNEW_EXE%" (
    echo [DarkFire] dfnew tool not found. Building tools...
    call "%~dp0Build.bat" Debug
    set DFNEW_EXE=%ROOT_DIR%\bin\Debug-windows-x86_64\Tools\dfnew\dfnew.exe
)

if not exist "%DFNEW_EXE%" (
    echo [ERROR] Could not build or find dfnew.exe
    exit /b 1
)

"%DFNEW_EXE%" %*
exit /b %ERRORLEVEL%
