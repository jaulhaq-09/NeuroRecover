@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ============================================
echo   Building NeuroRecover v3.0 for Windows
echo ============================================

mkdir build 2>nul

set "GXX="
if exist "C:\msys64\mingw64\bin\g++.exe" set "GXX=C:\msys64\mingw64\bin\g++.exe"
if not defined GXX (
    if exist "C:\msys2\mingw64\bin\g++.exe" set "GXX=C:\msys2\mingw64\bin\g++.exe"
)
if not defined GXX (
    where g++ >nul 2>&1
    if !ERRORLEVEL! EQU 0 set "GXX=g++"
)

if defined GXX (
    echo Using compiler: !GXX!
    echo Compiling C++ source files...
    "!GXX!" -std=c++17 -O2 -I. ^
      cli/main.cpp ^
      utils/logger.cpp utils/progress.cpp utils/hash.cpp ^
      utils/dedup.cpp utils/config.cpp utils/thread_pool.cpp ^
      core/scanner/scanner.cpp core/scanner/deep_scanner.cpp ^
      core/carving/file_signatures.cpp core/carving/carver.cpp ^
      core/carving/fragment_stitcher.cpp core/ai/ai_engine.cpp ^
      core/filesystem/fs_analyzer.cpp core/filesystem/ntfs_parser.cpp ^
      core/filesystem/fat_parser.cpp core/filesystem/ext_parser.cpp ^
      core/reconstruct/jpeg_repair.cpp core/reconstruct/png_repair.cpp ^
      core/reconstruct/video_repair.cpp core/reconstruct/bmp_repair.cpp ^
      core/reconstruct/reconstructor.cpp ^
      core/metadata/metadata_extractor.cpp ^
      core/preview/preview_generator.cpp ^
      core/checkpoint/checkpoint_manager.cpp ^
      core/encryption/encryption_detector.cpp ^
      core/filter/smart_filter.cpp ^
      core/sqlite_carver/sqlite_carver.cpp ^
      core/recovery_engine.cpp ^
      -o build/neurorecover.exe ^
      -static -static-libgcc -static-libstdc++ ^
      -lpthread -lstdc++fs -lws2_32 -lwinpthread

    if !ERRORLEVEL! EQU 0 (
        echo.
        echo ============================================
        echo   BUILD SUCCESS! build\neurorecover.exe ready
        echo ============================================
        exit /b 0
    ) else (
        echo [ERROR] Direct g++ compilation failed.
    )
)

echo Trying CMake fallback...
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release 2>nul
cmake --build . --config Release 2>nul
cd ..

if exist "build\neurorecover.exe" (
    echo Build complete: build\neurorecover.exe
    exit /b 0
)
if exist "build\Release\neurorecover.exe" (
    copy /Y "build\Release\neurorecover.exe" "build\neurorecover.exe" >nul 2>&1
    echo Build complete: build\neurorecover.exe
    exit /b 0
)

echo [ERROR] Build failed! Please install MSYS2 MINGW64 (https://www.msys2.org)
exit /b 1
