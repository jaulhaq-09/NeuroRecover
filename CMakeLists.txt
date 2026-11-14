cmake_minimum_required(VERSION 3.16)
project(NeuroRecover VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /MP /O2)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0601)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

set(UTILS_SRC
    utils/logger.cpp
    utils/progress.cpp
    utils/hash.cpp
    utils/dedup.cpp
    utils/config.cpp
    utils/thread_pool.cpp
)

set(CORE_SRC
    core/scanner/scanner.cpp
    core/scanner/deep_scanner.cpp
    core/carving/file_signatures.cpp
    core/carving/carver.cpp
    core/carving/fragment_stitcher.cpp
    core/ai/ai_engine.cpp
    core/filesystem/fs_analyzer.cpp
    core/filesystem/ntfs_parser.cpp
    core/filesystem/fat_parser.cpp
    core/filesystem/ext_parser.cpp
    core/reconstruct/jpeg_repair.cpp
    core/reconstruct/png_repair.cpp
    core/reconstruct/video_repair.cpp
    core/reconstruct/bmp_repair.cpp
    core/reconstruct/reconstructor.cpp
    core/metadata/metadata_extractor.cpp
    core/preview/preview_generator.cpp
    core/checkpoint/checkpoint_manager.cpp
    core/encryption/encryption_detector.cpp
    core/filter/smart_filter.cpp
    core/sqlite_carver/sqlite_carver.cpp
    core/recovery_engine.cpp
)

set(ALL_SRC ${UTILS_SRC} ${CORE_SRC})

add_executable(neurorecover cli/main.cpp ${ALL_SRC})
target_include_directories(neurorecover PRIVATE ${CMAKE_SOURCE_DIR})

add_executable(test_runner tests/test_runner.cpp ${ALL_SRC})
target_include_directories(test_runner PRIVATE ${CMAKE_SOURCE_DIR})

if(UNIX)
    target_link_libraries(neurorecover pthread stdc++fs)
    target_link_libraries(test_runner  pthread stdc++fs)
endif()

install(TARGETS neurorecover DESTINATION bin)

message(STATUS "NeuroRecover v3.0 | ${CMAKE_BUILD_TYPE} | ${CMAKE_CXX_COMPILER_ID}")
