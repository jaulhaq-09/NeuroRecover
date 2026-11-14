@echo off
title NeuroRecover Web UI
echo.
echo  NeuroRecover v3.0 - Starting Web UI...
echo.

:: Check Node.js
where node >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Node.js not found!
    echo.
    echo Please install Node.js from: https://nodejs.org
    echo Download the LTS version and install it.
    echo Then run this script again.
    echo.
    pause
    exit /b 1
)

:: Check if express is installed
if not exist "node_modules\express" (
    echo Installing required packages...
    npm install express
    echo.
)

:: Build neurorecover if needed
if not exist "build\neurorecover.exe" (
    echo Building neurorecover.exe...
    call build.bat Release
    echo.
)

:: Start server
echo Starting server at http://localhost:3000
echo.
echo Press Ctrl+C to stop the server.
echo.
start "" http://localhost:3000
node server.js