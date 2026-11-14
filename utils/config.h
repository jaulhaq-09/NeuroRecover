#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct RecoveryConfig {
    // Input/Output
    std::string devicePath;
    std::string outputDir;

    // File types
    std::vector<std::string> fileTypes;
    bool recoverAll = false;

    // Scan options
    bool deepScan    = false;
    bool fastMode    = false;
    uint64_t startOffset = 0;
    uint64_t endOffset   = 0;

    // Core features
    bool repairFiles     = false;
    bool generateReport  = false;
    bool extractMetadata = false;
    bool deduplication   = true;

    // v3.0 new features
    bool generatePreview  = false;  // HTML gallery
    bool enableCheckpoint = false;  // resume interrupted sessions
    bool resumeSession    = false;  // explicitly resume
    bool deepSQLite       = false;  // carve SQLite deleted rows
    bool detectEncryption = true;   // warn about encrypted volumes
    bool smartFilter      = false;  // post-recovery filter

    // Smart filter options
    std::string filterMinSize;      // e.g. "10KB"
    std::string filterMaxSize;      // e.g. "500MB"
    double      filterMinConf = 0.0;
    bool        filterClean   = false; // remove filtered files from disk

    // Performance
    int  numThreads  = 8;  // 8 threads by default for speed

    // Logging
    bool verbose     = false;
    std::string logFile = "recovery.log";

    // Internal
    uint32_t blockSize  = 512;
    uint32_t readBuffer = 64 * 1024 * 1024;  // 64MB chunks = fewer syscalls = faster
};