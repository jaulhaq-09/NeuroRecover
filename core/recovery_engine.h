#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include "utils/config.h"
#include "utils/dedup.h"
#include "utils/thread_pool.h"
#include "core/scanner/scanner.h"
#include "core/scanner/deep_scanner.h"
#include "core/carving/file_signatures.h"
#include "core/carving/carver.h"
#include "core/ai/ai_engine.h"
#include "core/filesystem/fs_analyzer.h"
#include "core/reconstruct/reconstructor.h"
#include "core/metadata/metadata_extractor.h"
#include "core/preview/preview_generator.h"
#include "core/checkpoint/checkpoint_manager.h"
#include "core/encryption/encryption_detector.h"
#include "core/filter/smart_filter.h"
#include "core/sqlite_carver/sqlite_carver.h"
#include "core/filesystem/ntfs_parser.h"
#include "core/filesystem/fat_parser.h"

struct RecoveryStats {
    uint64_t bytesScanned        = 0;
    size_t   filesFound          = 0;
    size_t   filesFromFilesystem = 0;   // NEW: recovered via NTFS/FAT metadata (real names)
    size_t   filesRepaired     = 0;
    size_t   filesDeduplicated = 0;
    size_t   filesMetadata     = 0;
    size_t   filesFiltered     = 0;
    size_t   sqliteRecords     = 0;
    double   elapsedSeconds    = 0.0;
    std::string fsType;
    std::string encryptionType;
    std::map<std::string, size_t> byExtension;
    bool resumed = false;
};

class RecoveryEngine {
public:
    explicit RecoveryEngine(const RecoveryConfig& config);
    ~RecoveryEngine();

    RecoveryStats run();

    using ProgressCb = std::function<void(const std::string& phase, double pct)>;
    void setProgressCallback(ProgressCb cb) { progressCb_ = cb; }

    using FileCb = std::function<void(const std::string& path, const std::string& ext)>;
    void setFileCallback(FileCb cb) { fileCb_ = cb; }

private:
    FSInfo           phase1_scan();

    // NEW: filesystem-metadata recovery — runs before raw carving.
    // Recovers deleted files under their real original names using
    // NTFS MFT / FAT directory entries. Fills recoveredRanges with
    // [offset,offset+size) byte ranges so phase2_carve can skip them.
    std::vector<CarvedFile> phase0_filesystemRecover(
        const FSInfo& fsInfo,
        std::vector<std::pair<uint64_t,uint64_t>>& recoveredRanges);

    std::vector<CarvedFile> phase2_carve(const FSInfo& fs,
                                          const std::vector<FileSignature>& sigs,
                                          uint64_t startOffset,
                                          const std::vector<std::pair<uint64_t,uint64_t>>& skipRanges);
    size_t           phase3_repair(std::vector<CarvedFile>& carved);
    void             phase4_report(const RecoveryStats& stats,
                                   const std::vector<CarvedFile>& carved);
    void             phase5_preview(const std::vector<CarvedFile>& carved);
    void             phase6_sqliteCarve(const std::vector<CarvedFile>& carved);

    void writeReport(const RecoveryStats& stats,
                     const std::vector<CarvedFile>& carved,
                     const std::string& path);

    const RecoveryConfig& config_;
    DedupEngine           dedup_;
    ProgressCb            progressCb_;
    FileCb                fileCb_;
    RecoveryStats         stats_;
};
