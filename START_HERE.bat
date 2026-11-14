@echo off
setlocal
cd /d "%~dp0"

title NeuroRecover v3.0 Launcher
color 0B
echo.
echo  ============================================
echo    NeuroRecover v3.0 - Web UI Launcher
echo  ============================================
echo.

:: Check for Administrator privileges
net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo  [INFO] Requesting Administrator privileges for raw disk access...
    powershell -NoProfile -Command "Start-Process cmd.exe -ArgumentList '/k', ('\"\"' + '%~f0' + '\"\"') -WorkingDirectory '%~dp0' -Verb RunAs"
    exit /b 0
)

:: Check if Node.js is installed
where node >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo  [ERROR] Node.js is not installed!
    echo.
    echo  Please install Node.js from: https://nodejs.org
    echo  Download the LTS version, install it, then run this file again.
    echo.
    pause
    exit /b 1
)

:: Check if dependencies are installed
if not exist "node_modules\express\package.json" (
    echo  [INFO] Installing Express and dependencies...
    call npm install
    echo.
)

:: Check and build neurorecover.exe if needed
if not exist "build\neurorecover.exe" (
    echo  [INFO] Engine binary not found. Building now...
    call build.bat
    echo.
)

:: Free port 3000 if occupied
for /f "tokens=5" %%a in ('netstat -aon ^| findstr ":3000" 2^>nul') do (
    taskkill /PID %%a /F >nul 2>&1
)

:: Open browser after 2 seconds
start "" /b cmd /c "timeout /t 2 >nul && start http://localhost:3000"

:: Start server
echo  Starting server at http://localhost:3000
echo  Press Ctrl+C to stop.
echo.
node server.js
