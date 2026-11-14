#!/usr/bin/env bash
# NeuroRecover Fast Parallel Build Script
set -e

BUILD_DIR="build"
echo "============================================"
echo "  NeuroRecover v3.0 - Fast Parallel Build"
echo "============================================"

mkdir -p "$BUILD_DIR"

if command -v cmake &>/dev/null; then
    cd "$BUILD_DIR"
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17
    cmake --build . --config Release --parallel $(nproc 2>/dev/null || echo 4)
    cd ..
else
    NPROC=$(nproc 2>/dev/null || echo 4)
    echo "Compiling with g++ (parallel: $NPROC)..."
    g++ -std=c++17 -O2 -pipe -I. \
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
      -o "$BUILD_DIR/neurorecover" -lpthread -lstdc++fs
fi

echo "============================================"
echo "  Build successful! Binary: $BUILD_DIR/neurorecover"
echo "============================================"
