@echo off
setlocal

set "SCRIPT_DIR=%~dp0"

where powershell.exe >nul 2>nul
if errorlevel 1 (
    echo [VectorGL] PowerShell is required to run the build helper.
    exit /b 1
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build.ps1" %*
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" (
    echo [VectorGL] Build failed with exit code %RESULT%.
)

exit /b %RESULT%
