# NeuroRecover v2.0 — Project Structure

```
NeuroRecover/
│
├── CMakeLists.txt              CMake build configuration
├── Makefile                    Convenience make targets
├── Dockerfile                  Docker containerization
├── build.sh                    Linux/macOS build script
├── build.bat                   Windows build script
│
├── README.md                   Full feature overview
├── QUICKSTART.md               5-minute quick-start
├── INSTALLATION.md             Platform installation guide
├── PROJECT_STRUCTURE.md        This file
│
├── cli/
│   └── main.cpp                CLI entry point, argument parsing (400 LOC)
│
├── core/
│   ├── recovery_engine.h       4-phase orchestrator interface
│   ├── recovery_engine.cpp     Pipeline: Scan→Carve→Repair→Report (300 LOC)
│   │
│   ├── scanner/
│   │   ├── scanner.h/.cpp      Cross-platform raw disk reader (200 LOC)
│   │   └── deep_scanner.h/.cpp Entropy-based analysis (80 LOC)
│   │
│   ├── carving/
│   │   ├── file_signatures.h/.cpp  50+ format magic bytes (200 LOC)
│   │   ├── carver.h/.cpp           Streaming file carver (250 LOC)
│   │   └── fragment_stitcher.h/.cpp Fragment reassembly (60 LOC)
│   │
│   ├── ai/
│   │   └── ai_engine.h/.cpp    Entropy analysis + type inference (120 LOC)
│   │
│   ├── filesystem/
│   │   ├── fs_analyzer.h/.cpp  Filesystem detection (150 LOC)
│   │   ├── ntfs_parser.h/.cpp  NTFS MFT parser (200 LOC)
│   │   ├── fat_parser.h/.cpp   FAT32/exFAT parser (150 LOC)
│   │   └── ext_parser.h/.cpp   ext2/3/4 parser (150 LOC)
│   │
│   ├── reconstruct/
│   │   ├── jpeg_repair.h/.cpp  JPEG marker repair (180 LOC)
│   │   ├── png_repair.h/.cpp   PNG chunk repair (160 LOC)
│   │   ├── video_repair.h/.cpp MP4/AVI/MKV repair (150 LOC)
│   │   └── reconstructor.h/.cpp Generic repair dispatcher (80 LOC)
│   │
│   └── metadata/
│       └── metadata_extractor.h/.cpp  EXIF + GPS extraction (300 LOC)
│
├── utils/
│   ├── logger.h/.cpp           Thread-safe logging (80 LOC)
│   ├── progress.h/.cpp         Progress bar with ETA (70 LOC)
│   ├── hash.h/.cpp             SHA-256 + FNV-64 (150 LOC)
│   ├── dedup.h/.cpp            Deduplication engine (60 LOC)
│   ├── config.h/.cpp           RecoveryConfig struct (40 LOC)
│   └── thread_pool.h/.cpp      Work-stealing thread pool (80 LOC)
│
└── tests/
    └── test_runner.cpp         37 unit tests, 150+ assertions (350 LOC)
```

## File Counts

| Category | Files | Lines |
|----------|-------|-------|
| Headers (.h) | 22 | ~700 |
| Implementations (.cpp) | 23 | ~2,750 |
| Documentation (.md) | 4 | ~600 |
| Build files | 5 | ~200 |
| **Total** | **54** | **~4,250** |

## Key Design Decisions

- **Zero dependencies**: SHA-256 implemented from scratch (FIPS 180-4), no OpenSSL required
- **Cross-platform scanner**: Uses `pread()` on POSIX, `ReadFile()` on Windows
- **Streaming carver**: Processes the device in chunks — handles drives larger than RAM
- **Thread pool**: Each read buffer is dispatched to the pool; carving is fully parallel
- **Dedup by FNV-64**: Fast 64-bit hash on first chunk; avoids writing duplicate files
- **AI confidence**: Entropy analysis scores each carved file 0.0–1.0
- **Footer-aware carving**: JPEG/PNG/PDF use footer markers for accurate file boundaries
