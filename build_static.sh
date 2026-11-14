#!/bin/bash
# Build neurorecover.exe with STATIC linking
# This bundles all DLLs into the exe so it works anywhere on Windows
# Run in MSYS2 MINGW64 as Administrator

export PATH="/mingw64/bin:/usr/bin:/bin:$PATH"

echo "============================================"
echo "  NeuroRecover - STATIC BUILD"
echo "  (bundles all DLLs - works everywhere)"  
echo "============================================"

if ! command -v g++ &>/dev/null; then
    echo "ERROR: g++ not found. Run: pacman -S mingw-w64-x86_64-gcc"
    exit 1
fi

echo "Compiler: $(g++ --version | head -1)"
mkdir -p build

g++ -std=c++17 -O2 -I. \
  cli/main.cpp \
  utils/logger.cpp utils/progress.cpp utils/hash.cpp \
  utils/dedup.cpp utils/config.cpp utils/thread_pool.cpp \
  core/scanner/scanner.cpp core/scanner/deep_scanner.cpp \
  core/carving/file_signatures.cpp core/carving/carver.cpp \
  core/carving/fragment_stitcher.cpp core/ai/ai_engine.cpp \
  core/filesystem/fs_analyzer.cpp core/filesystem/ntfs_parser.cpp \
  core/filesystem/fat_parser.cpp core/filesystem/ext_parser.cpp \
  core/reconstruct/jpeg_repair.cpp core/reconstruct/png_repair.cpp \
  core/reconstruct/video_repair.cpp core/reconstruct/bmp_repair.cpp \
  core/reconstruct/reconstructor.cpp \
  core/metadata/metadata_extractor.cpp \
  core/preview/preview_generator.cpp \
  core/checkpoint/checkpoint_manager.cpp \
  core/encryption/encryption_detector.cpp \
  core/filter/smart_filter.cpp \
  core/sqlite_carver/sqlite_carver.cpp \
  core/recovery_engine.cpp \
  -o build/neurorecover.exe \
  -static -static-libgcc -static-libstdc++ \
  -lpthread -lstdc++fs -lws2_32 -lwinpthread

if [ $? -eq 0 ]; then
    echo ""
    echo "============================================"
    echo "  SUCCESS! Static build complete."
    echo "  Size: $(du -h build/neurorecover.exe | cut -f1)"
    echo "  This exe works WITHOUT MSYS2 DLLs"
    echo "============================================"
else
    echo "Static build failed, trying dynamic..."
    g++ -std=c++17 -O2 -I. \
      cli/main.cpp \
      utils/logger.cpp utils/progress.cpp utils/hash.cpp \
      utils/dedup.cpp utils/config.cpp utils/thread_pool.cpp \
      core/scanner/scanner.cpp core/scanner/deep_scanner.cpp \
      core/carving/file_signatures.cpp core/carving/carver.cpp \
      core/carving/fragment_stitcher.cpp core/ai/ai_engine.cpp \
      core/filesystem/fs_analyzer.cpp core/filesystem/ntfs_parser.cpp \
      core/filesystem/fat_parser.cpp core/filesystem/ext_parser.cpp \
      core/reconstruct/jpeg_repair.cpp core/reconstruct/png_repair.cpp \
      core/reconstruct/video_repair.cpp core/reconstruct/bmp_repair.cpp \
      core/reconstruct/reconstructor.cpp \
      core/metadata/metadata_extractor.cpp \
      core/preview/preview_generator.cpp \
      core/checkpoint/checkpoint_manager.cpp \
      core/encryption/encryption_detector.cpp \
      core/filter/smart_filter.cpp \
      core/sqlite_carver/sqlite_carver.cpp \
      core/recovery_engine.cpp \
      -o build/neurorecover.exe \
      -lpthread -lstdc++fs
    echo "Dynamic build done: $(du -h build/neurorecover.exe | cut -f1)"
fi