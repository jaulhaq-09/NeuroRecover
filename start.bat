@echo off
setlocal
cd /d "%~dp0"

title NeuroRecover Web UI
echo.
echo  NeuroRecover v3.0 - Starting Web UI...
echo.

:: Check for Administrator privileges
net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo  [INFO] Requesting Administrator privileges for raw disk access...
    powershell -NoProfile -Command "Start-Process cmd.exe -ArgumentList '/k', ('\"\"' + '%~f0' + '\"\"') -WorkingDirectory '%~dp0' -Verb RunAs"
    exit /b 0
)

:: Check Node.js
where node >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Node.js not found!
    echo Please install Node.js from: https://nodejs.org
    pause
    exit /b 1
)

:: Check if express is installed
if not exist "node_modules\express\package.json" (
    echo [INFO] Installing required packages...
    call npm install
    echo.
)

:: Build neurorecover if needed
if not exist "build\neurorecover.exe" (
    echo [INFO] Building neurorecover.exe...
    call build.bat
    echo.
)

:: Free port 3000 if occupied
for /f "tokens=5" %%a in ('netstat -aon ^| findstr ":3000" 2^>nul') do (
    taskkill /PID %%a /F >nul 2>&1
)

:: Start server
echo Starting server at http://localhost:3000
echo Press Ctrl+C to stop the server.
echo.
start "" /b cmd /c "timeout /t 2 >nul && start http://localhost:3000"
node server.js
