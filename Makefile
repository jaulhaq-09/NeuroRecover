CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pthread -I.
LDFLAGS  := -pthread -lstdc++fs

UTILS_SRC := utils/logger.cpp utils/progress.cpp utils/hash.cpp \
             utils/dedup.cpp utils/config.cpp utils/thread_pool.cpp

CORE_SRC  := core/scanner/scanner.cpp core/scanner/deep_scanner.cpp \
             core/carving/file_signatures.cpp core/carving/carver.cpp \
             core/carving/fragment_stitcher.cpp \
             core/ai/ai_engine.cpp \
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
             core/recovery_engine.cpp

ALL_SRC   := $(UTILS_SRC) $(CORE_SRC)
BUILD_DIR := build

.PHONY: all release debug test clean install help

all: release

release:
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/neurorecover cli/main.cpp $(ALL_SRC) $(LDFLAGS)
	@echo "Build complete: $(BUILD_DIR)/neurorecover"

debug:
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -g -o $(BUILD_DIR)/neurorecover cli/main.cpp $(ALL_SRC) $(LDFLAGS)
	$(CXX) $(CXXFLAGS) -g -o $(BUILD_DIR)/test_runner tests/test_runner.cpp $(ALL_SRC) $(LDFLAGS)

test:
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/test_runner tests/test_runner.cpp $(ALL_SRC) $(LDFLAGS)
	./$(BUILD_DIR)/test_runner

install: release
	install -m 755 $(BUILD_DIR)/neurorecover /usr/local/bin/neurorecover

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "Targets: all release debug test install clean"
