# NeuroRecover v3.0 - PowerShell Build Script
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Building NeuroRecover v3.0 (PowerShell)   " -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

$gxx = $null
$candidates = @(
    "C:\msys64\mingw64\bin\g++.exe",
    "C:\msys2\mingw64\bin\g++.exe",
    "C:\mingw64\bin\g++.exe"
)

foreach ($c in $candidates) {
    if (Test-Path $c) {
        $gxx = $c
        break
    }
}

if (-not $gxx) {
    $cmd = Get-Command g++ -ErrorAction SilentlyContinue
    if ($cmd) {
        $gxx = $cmd.Source
    }
}

if (-not $gxx) {
    Write-Error "g++ compiler not found! Please install MSYS2 MINGW64 (https://www.msys2.org) or install via 'winget install MSYS2.MSYS2'."
    exit 1
}

Write-Host "Using compiler: $gxx" -ForegroundColor Green
Write-Host "Compiling C++ source files..." -ForegroundColor Yellow

$sources = @(
    "cli/main.cpp",
    "utils/logger.cpp", "utils/progress.cpp", "utils/hash.cpp",
    "utils/dedup.cpp", "utils/config.cpp", "utils/thread_pool.cpp",
    "core/scanner/scanner.cpp", "core/scanner/deep_scanner.cpp",
    "core/carving/file_signatures.cpp", "core/carving/carver.cpp",
    "core/carving/fragment_stitcher.cpp", "core/ai/ai_engine.cpp",
    "core/filesystem/fs_analyzer.cpp", "core/filesystem/ntfs_parser.cpp",
    "core/filesystem/fat_parser.cpp", "core/filesystem/ext_parser.cpp",
    "core/reconstruct/jpeg_repair.cpp", "core/reconstruct/png_repair.cpp",
    "core/reconstruct/video_repair.cpp", "core/reconstruct/bmp_repair.cpp",
    "core/reconstruct/reconstructor.cpp",
    "core/metadata/metadata_extractor.cpp",
    "core/preview/preview_generator.cpp",
    "core/checkpoint/checkpoint_manager.cpp",
    "core/encryption/encryption_detector.cpp",
    "core/filter/smart_filter.cpp",
    "core/sqlite_carver/sqlite_carver.cpp",
    "core/recovery_engine.cpp"
)

$argsList = @(
    "-std=c++17", "-O2", "-I."
) + $sources + @(
    "-o", "build/neurorecover.exe",
    "-static", "-static-libgcc", "-static-libstdc++",
    "-lpthread", "-lstdc++fs", "-lws2_32", "-lwinpthread"
)

$proc = Start-Process -FilePath $gxx -ArgumentList $argsList -NoNewWindow -Wait -PassThru

if ($proc.ExitCode -eq 0 -and (Test-Path "build\neurorecover.exe")) {
    $size = (Get-Item "build\neurorecover.exe").Length / 1MB
    Write-Host ""
    Write-Host "============================================" -ForegroundColor Green
    Write-Host ("  BUILD SUCCESS! build\neurorecover.exe ready ({0:N1} MB)" -f $size) -ForegroundColor Green
    Write-Host "============================================" -ForegroundColor Green
    exit 0
} else {
    Write-Error "Build failed with exit code $($proc.ExitCode)"
    exit 1
}
