@echo off
title NeuroRecover v3.0
color 0B
echo.
echo  ============================================
echo    NeuroRecover v3.0 - Web UI Launcher
echo  ============================================
echo.

:: Check if Node.js is installed
where node >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo  [ERROR] Node.js not installed!
    echo.
    echo  Please install from: https://nodejs.org
    echo  Download LTS version, install, then run this again.
    echo.
    pause
    exit /b 1
)

:: Check if express is installed
if not exist "node_modules\express\package.json" (
    echo  Installing Express...
    call npm install express --silent
    echo  Express installed!
    echo.
)

:: Kill any existing server on port 3000
for /f "tokens=5" %%a in ('netstat -aon ^| findstr ":3000" 2^>nul') do (
    taskkill /PID %%a /F >nul 2>&1
)

:: Check if exe exists
if not exist "build\neurorecover.exe" (
    echo  [WARNING] neurorecover.exe not built yet!
    echo  The UI will run in DEMO MODE.
    echo  To enable real recovery, run build_windows.sh in MSYS2 first.
    echo.
)

:: Open browser after 2 seconds
start "" /b cmd /c "timeout /t 2 >nul && start http://localhost:3000"

:: Start server
echo  Starting server at http://localhost:3000
echo  Press Ctrl+C to stop.
echo.
node server.js