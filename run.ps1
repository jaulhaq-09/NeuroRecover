# NeuroRecover v3.0 - PowerShell Launcher
[CmdletBinding()]
param(
    [switch]$NoBrowser
)

Set-Location $PSScriptRoot

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  NeuroRecover v3.0 - PowerShell Launcher   " -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

if (-not $isAdmin) {
    Write-Warning "PowerShell is NOT running as Administrator!"
    Write-Warning "Raw physical disk scanning (//./PhysicalDriveX) requires Administrator privileges."
    Write-Warning "Tip: For raw drives, restart PowerShell by right-clicking -> 'Run as Administrator'."
    Write-Host ""
} else {
    Write-Host "Administrator privileges: ACTIVE [OK]" -ForegroundColor Green
}

# Check Node.js
$nodeCmd = Get-Command node -ErrorAction SilentlyContinue
if (-not $nodeCmd) {
    Write-Error "Node.js is not installed! Please install LTS version from https://nodejs.org"
    exit 1
}

# Install dependencies if missing
if (-not (Test-Path "node_modules\express\package.json")) {
    Write-Host "Installing npm dependencies..." -ForegroundColor Yellow
    npm install
}

# Build engine if missing
if (-not (Test-Path "build\neurorecover.exe")) {
    Write-Host "Engine binary not found. Running build.ps1..." -ForegroundColor Yellow
    & "$PSScriptRoot\build.ps1"
}

# Open browser and start server
Write-Host ""
Write-Host "Starting Web Server at http://localhost:3000" -ForegroundColor Cyan
Write-Host "Press Ctrl+C in this terminal anytime to stop." -ForegroundColor Gray
Write-Host ""

if (-not $NoBrowser) {
    Start-Process "http://localhost:3000"
}

node server.js
