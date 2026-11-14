#include "core/recovery_engine.h"
#include <cstring>
#include "utils/logger.h"
#include "utils/progress.h"
#include "core/filesystem/ntfs_parser.h"
#include "core/filesystem/fat_parser.h"
#include "core/filesystem/ext_parser.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <mutex>
#include <iostream>

namespace fs = std::filesystem;

RecoveryEngine::RecoveryEngine(const RecoveryConfig& config) : config_(config) {}
RecoveryEngine::~RecoveryEngine() {}

RecoveryStats RecoveryEngine::run() {
    auto t0 = std::chrono::steady_clock::now();

    LOG_INFO("═══════════════════════════════════════════");
    LOG_INFO("  NeuroRecover v3.0 — Starting Recovery");
    LOG_INFO("  Device  : " + config_.devicePath);
    LOG_INFO("  Output  : " + config_.outputDir);
    LOG_INFO("  Threads : " + std::to_string(config_.numThreads));
    LOG_INFO("═══════════════════════════════════════════");

    try {
        fs::create_directories(config_.outputDir);
    } catch (const std::exception& e) {
        LOG_WARN("Could not create output directory " + config_.outputDir + " (" + e.what() + "), falling back to ./recovered_files");
        const_cast<RecoveryConfig&>(config_).outputDir = "./recovered_files";
        fs::create_directories(config_.outputDir);
    }

    // ── Checkpoint resume ────────────────────────────────────────────────────
    uint64_t startOffset = config_.startOffset;
    std::string ckptPath = CheckpointManager::defaultPath(config_.outputDir);
    CheckpointManager ckpt(ckptPath);

    if (config_.resumeSession && ckpt.hasResumable(config_.devicePath)) {
        CheckpointData cd;
        ckpt.load(cd);
        startOffset        = cd.lastOffset;
        stats_.filesFound  = cd.filesFound;
        stats_.byExtension = cd.byExtension;
        stats_.resumed     = true;
        LOG_INFO("Resuming from offset=" + std::to_string(startOffset));
    }

    // ── Phase 1 ──────────────────────────────────────────────────────────────
    if (progressCb_) progressCb_("Scanning", 0.0);
    FSInfo fsInfo = phase1_scan();
    stats_.fsType = FSAnalyzer::fsTypeName(fsInfo.type);

    std::vector<FileSignature> sigs;
    if (config_.recoverAll || config_.fileTypes.empty())
        sigs = FileSignatureDB::instance().all();
    else
        sigs = FileSignatureDB::instance().forTypes(config_.fileTypes);
    LOG_INFO("Signatures: " + std::to_string(sigs.size()));

    // ── Phase 0.5 — Filesystem-metadata deleted-file recovery ─────────────────
    // Runs before raw carving so files keep their real original names where possible.
    std::vector<std::pair<uint64_t,uint64_t>> recoveredRanges;
    if (progressCb_) progressCb_("Filesystem Recovery", 0.0);
    auto carved = phase0_filesystemRecover(fsInfo, recoveredRanges);
    stats_.filesFromFilesystem = carved.size();
    stats_.filesFound += carved.size();
    LOG_INFO("Filesystem recovery: " + std::to_string(carved.size()) + " files with original names");

    // ── Phase 2 ──────────────────────────────────────────────────────────────
    if (progressCb_) progressCb_("Carving", 0.0);
    auto rawCarved = phase2_carve(fsInfo, sigs, startOffset, recoveredRanges);
    stats_.filesFound += rawCarved.size();
    carved.insert(carved.end(), rawCarved.begin(), rawCarved.end());

    // ── Smart Filter ─────────────────────────────────────────────────────────
    if (config_.smartFilter) {
        SmartFilter sf;
        FilterCriteria fc = SmartFilter::fromArgs(
            config_.filterMinSize, config_.filterMaxSize,
            config_.filterMinConf, "");
        FilterStats fstats;
        carved = config_.filterClean
               ? sf.applyAndClean(carved, fc, &fstats)
               : sf.apply(carved, fc, &fstats);
        SmartFilter::printStats(fstats);
        stats_.filesFiltered = fstats.totalIn - fstats.passedCount;
    }

    // ── Phase 3 ──────────────────────────────────────────────────────────────
    if (config_.repairFiles) {
        if (progressCb_) progressCb_("Repairing", 0.0);
        stats_.filesRepaired = phase3_repair(carved);
    }

    // ── Timing ───────────────────────────────────────────────────────────────
    auto t1 = std::chrono::steady_clock::now();
    stats_.elapsedSeconds = std::chrono::duration<double>(t1-t0).count();

    // ── Phase 4 ──────────────────────────────────────────────────────────────
    if (config_.generateReport) {
        if (progressCb_) progressCb_("Report", 0.9);
        phase4_report(stats_, carved);
    }

    // ── Phase 5 ──────────────────────────────────────────────────────────────
    if (config_.generatePreview) {
        if (progressCb_) progressCb_("Preview", 0.95);
        phase5_preview(carved);
    }

    // ── Phase 6 ──────────────────────────────────────────────────────────────
    if (config_.deepSQLite) {
        if (progressCb_) progressCb_("SQLite", 0.97);
        phase6_sqliteCarve(carved);
    }

    if (config_.enableCheckpoint) ckpt.remove();

    LOG_INFO("═══════════════════════════════════════════");
    LOG_INFO("  Files found    : " + std::to_string(stats_.filesFound));
    LOG_INFO("  Files repaired : " + std::to_string(stats_.filesRepaired));
    LOG_INFO("  Filtered out   : " + std::to_string(stats_.filesFiltered));
    LOG_INFO("  SQLite records : " + std::to_string(stats_.sqliteRecords));
    LOG_INFO("  Time           : " + std::to_string((int)stats_.elapsedSeconds) + "s");
    LOG_INFO("═══════════════════════════════════════════");

    if (progressCb_) progressCb_("Done", 1.0);
    return stats_;
}

FSInfo RecoveryEngine::phase1_scan() {
    Scanner scanner(config_);
    if (!scanner.open()) { LOG_ERROR("Cannot open: " + config_.devicePath); return {}; }
    stats_.bytesScanned = scanner.deviceSize();

    if (config_.detectEncryption) {
        std::vector<uint8_t> buf(4096, 0);
        size_t got = 0;
        scanner.readAt(0, buf.data(), 4096, got);
        EncryptionDetector ed;
        auto enc = ed.detect(buf.data(), got, 0);
        if (enc.isEncrypted) {
            stats_.encryptionType = enc.name;
            LOG_WARN("ENCRYPTED: " + enc.name);
            std::cout << "\n⚠️  " << enc.name << " detected!\n"
                      << "   " << EncryptionDetector::recoveryAdvice(enc.type) << "\n\n";
        }
    }

    FSAnalyzer analyzer;
    FSInfo info = analyzer.detect(scanner);
    scanner.close();
    return info;
}

std::vector<CarvedFile> RecoveryEngine::phase0_filesystemRecover(
        const FSInfo& fsInfo,
        std::vector<std::pair<uint64_t,uint64_t>>& recoveredRanges) {

    std::vector<CarvedFile> recovered;

    // Only NTFS and FAT32 metadata recovery are supported right now.
    if (fsInfo.type != FSType::NTFS && fsInfo.type != FSType::FAT32) {
        LOG_INFO("Filesystem recovery: skipped (fs type not NTFS/FAT32)");
        return recovered;
    }

    Scanner scanner(config_);
    if (!scanner.open()) { LOG_ERROR("Filesystem recovery: cannot open device"); return recovered; }

    try {
        fs::create_directories(config_.outputDir);
    } catch (const std::exception& e) {
        LOG_WARN("Could not create output directory " + config_.outputDir + " (" + e.what() + "), falling back to ./recovered_files");
        const_cast<RecoveryConfig&>(config_).outputDir = "./recovered_files";
        fs::create_directories(config_.outputDir);
    }
    const uint64_t MAX_REASONABLE_SIZE = 4ULL * 1024 * 1024 * 1024; // 4GB safety cap

    auto wantExt = [&](const std::string& ext) {
        if (config_.recoverAll || config_.fileTypes.empty()) return true;
        for (auto& w : config_.fileTypes) if (w == ext) return true;
        return false;
    };
    auto sanitize = [](std::string n) {
        for (auto& c : n)
            if (c=='/'||c=='\\'||c==':'||c=='*'||c=='?'||c=='"'||c=='<'||c=='>'||c=='|') c = '_';
        return n;
    };

    if (fsInfo.type == FSType::NTFS) {
        NTFSParser ntfs;
        if (!ntfs.parse(scanner, 0)) {
            LOG_WARN("Filesystem recovery: NTFS MFT parse failed");
            scanner.close(); return recovered;
        }
        LOG_INFO("Filesystem recovery: " + std::to_string(ntfs.deletedFiles().size()) + " deleted NTFS entries");

        for (auto& e : ntfs.deletedFiles()) {
            if (e.isDirectory || e.size == 0 || e.size > MAX_REASONABLE_SIZE) continue;
            if (e.dataRuns.empty()) continue;              // resident or run not decoded
            if (!wantExt(e.extension)) continue;

            // Read every data run in order and concatenate — required for fragmented files.
            std::vector<uint8_t> data;
            data.reserve(e.size);
            for (auto& run : e.dataRuns) {
                if (data.size() >= e.size) break;
                uint64_t remaining = e.size - data.size();
                uint64_t toRead = std::min<uint64_t>(run.length, remaining);
                if (run.offset == UINT64_MAX) {
                    data.insert(data.end(), (size_t)toRead, 0);   // sparse hole
                    continue;
                }
                std::vector<uint8_t> chunk((size_t)toRead);
                size_t got = 0;
                if (!scanner.readAt(run.offset, chunk.data(), (size_t)toRead, got) || got == 0) break;
                chunk.resize(got);
                data.insert(data.end(), chunk.begin(), chunk.end());
            }
            if (data.empty()) continue;
            size_t got = data.size();

            std::string name = sanitize(e.name.empty()
                ? ("mft_" + std::to_string(e.mftRef) + "." + e.extension) : e.name);
            std::string outPath = config_.outputDir + "/" + name;
            if (fs::exists(outPath))
                outPath = config_.outputDir + "/" + std::to_string(e.mftRef) + "_" + name;

            std::ofstream f(outPath, std::ios::binary);
            if (!f) continue;
            f.write(reinterpret_cast<char*>(data.data()), data.size());
            f.close();

            CarvedFile cf;
            cf.startOffset = e.dataRuns[0].offset;
            cf.endOffset   = cf.startOffset + got;
            cf.size        = got;
            cf.extension   = e.extension;
            cf.outputPath  = outPath;
            cf.confidence  = (e.dataRuns.size() > 1) ? 0.90 : 0.95; // slightly lower for fragmented files
            recovered.push_back(cf);

            // Mark each individual run as recovered so phase2 doesn't re-carve it,
            // without wrongly skipping unrelated data sitting between fragments.
            uint64_t accounted = 0;
            for (auto& run : e.dataRuns) {
                if (accounted >= got) break;
                uint64_t remaining = got - accounted;
                uint64_t len = std::min<uint64_t>(run.length, remaining);
                if (run.offset != UINT64_MAX)
                    recoveredRanges.push_back({run.offset, run.offset + len});
                accounted += len;
            }
            if (fileCb_) fileCb_(outPath, e.extension);
        }
    }
    else if (fsInfo.type == FSType::FAT32) {
        FATParser fat;
        if (!fat.parse(scanner, 0)) {
            LOG_WARN("Filesystem recovery: FAT parse failed");
            scanner.close(); return recovered;
        }
        LOG_INFO("Filesystem recovery: " + std::to_string(fat.deletedFiles().size()) + " deleted FAT entries");

        for (auto& e : fat.deletedFiles()) {
            if (e.isDirectory || e.size == 0 || e.size > MAX_REASONABLE_SIZE) continue;
            if (e.firstCluster < 2) continue;
            if (!wantExt(e.extension)) continue;

            auto chain = fat.readClusterChain(scanner, e.firstCluster);
            if (chain.empty()) continue;

            std::vector<uint8_t> data;
            data.reserve(e.size);
            double conf = 0.95;

            if (chain.size() > 1) {
                for (auto c : chain) {
                    uint64_t off = fat.clusterToOffset(c);
                    std::vector<uint8_t> clusterBuf(fsInfo.clusterSize);
                    size_t got = 0;
                    if (!scanner.readAt(off, clusterBuf.data(), clusterBuf.size(), got)) break;
                    data.insert(data.end(), clusterBuf.begin(), clusterBuf.begin()+got);
                    if (data.size() >= e.size) break;
                }
            } else {
                // Broken/zeroed FAT chain (typical after deletion) — assume
                // contiguous clusters from firstCluster, like PhotoRec/EaseUS do.
                uint64_t off = fat.clusterToOffset(e.firstCluster);
                data.resize(e.size);
                size_t got = 0;
                if (!scanner.readAt(off, data.data(), e.size, got)) continue;
                data.resize(got);
                conf = 0.80; // lower confidence — tail bytes may belong to another file
            }
            if (data.empty()) continue;
            if (data.size() > e.size) data.resize(e.size);

            std::string name = sanitize(e.name.empty()
                ? ("fat_" + std::to_string(e.firstCluster) + "." + e.extension) : e.name);
            std::string outPath = config_.outputDir + "/" + name;
            if (fs::exists(outPath))
                outPath = config_.outputDir + "/" + std::to_string(e.firstCluster) + "_" + name;

            std::ofstream f(outPath, std::ios::binary);
            if (!f) continue;
            f.write(reinterpret_cast<char*>(data.data()), data.size());
            f.close();

            uint64_t startOff = fat.clusterToOffset(e.firstCluster);
            CarvedFile cf;
            cf.startOffset = startOff;
            cf.endOffset   = startOff + data.size();
            cf.size        = data.size();
            cf.extension   = e.extension;
            cf.outputPath  = outPath;
            cf.confidence  = conf;
            recovered.push_back(cf);
            recoveredRanges.push_back({startOff, startOff + data.size()});
            if (fileCb_) fileCb_(outPath, e.extension);
        }
    }

    scanner.close();
    LOG_INFO("Filesystem recovery: wrote " + std::to_string(recovered.size()) + " files with original names");
    return recovered;
}

std::vector<CarvedFile> RecoveryEngine::phase2_carve(
        const FSInfo& /*fsInfo*/,
        const std::vector<FileSignature>& sigs,
        uint64_t startOffset,
        const std::vector<std::pair<uint64_t,uint64_t>>& skipRanges) {

    Scanner scanner(config_);
    if (!scanner.open()) return {};

    std::vector<CarvedFile> allCarved;
    std::mutex mu;
    ProgressTracker progress;
    progress.start(scanner.deviceSize(), "Carving");

    FileCarver  carver(config_, dedup_);
    DeepScanner deepScanner(config_);
    AIEngine    ai;
    ThreadPool  pool(config_.numThreads);

    CheckpointManager ckpt(CheckpointManager::defaultPath(config_.outputDir));
    uint64_t lastCkpt = startOffset;
    const uint64_t CKPT_INTERVAL = 500ULL * 1024 * 1024;

    // Use 64MB buffer for maximum USB throughput (fewer read syscalls)
    const size_t BUFFER = std::max((size_t)config_.readBuffer, (size_t)(64*1024*1024));
    const size_t SECTOR = 512;
    uint64_t pos = startOffset;
    uint64_t end = (config_.endOffset > 0) ? config_.endOffset : scanner.deviceSize();
    uint64_t lastPrint = startOffset;
    uint64_t lastSync  = startOffset;
    const uint64_t PRINT_INTERVAL = 8ULL * 1024 * 1024;   // update UI every ~8MB progressed
    const uint64_t SYNC_INTERVAL  = BUFFER * 3;            // throttle memory growth periodically

    // Pre-allocate buffer to avoid repeated allocation
    std::vector<uint8_t> buf(BUFFER);

    while (pos < end) {
        // Skip regions already recovered via filesystem metadata (phase0) —
        // avoids re-carving/duplicating the same photo/video.
        bool inSkipRange = false;
        for (auto& r : skipRanges) {
            if (pos >= r.first && pos < r.second) {
                pos = r.second;
                inSkipRange = true;
                break;
            }
        }
        if (inSkipRange) { progress.update(pos); continue; }

        size_t toRead = (size_t)std::min((uint64_t)BUFFER, end - pos);
        size_t got = 0;
        if (!scanner.readAt(pos, buf.data(), toRead, got) || got == 0) {
            pos += toRead; continue;
        }

        // SPEED OPTIMIZATION: Skip zero-filled blocks (empty USB space)
        // Check first 64 bytes - if all zero, skip entire 512-byte sector
        bool allZero = true;
        for (size_t zi = 0; zi < std::min(got, (size_t)64); ++zi) {
            if (buf[zi] != 0) { allZero = false; break; }
        }
        if (allZero && got >= SECTOR) {
            // Skip forward to next non-zero sector
            size_t skipSectors = 0;
            for (size_t si = 0; si < got; si += SECTOR) {
                bool sectorZero = true;
                for (size_t bi = si; bi < std::min(si+16, got); ++bi) {
                    if (buf[bi] != 0) { sectorZero = false; break; }
                }
                if (sectorZero) skipSectors++;
                else break;
            }
            size_t skipBytes = skipSectors * SECTOR;
            if (skipBytes >= got) { pos += got; progress.update(pos); continue; }
            // Adjust to start at first non-zero sector
            pos += skipBytes;
            got -= skipBytes;
            memmove(buf.data(), buf.data() + skipBytes, got);
        }

        if (config_.fastMode) {
            double e = deepScanner.calculateEntropy(buf.data(), std::min(got,(size_t)256));
            if (e > 7.95) { pos += got; progress.update(pos); continue; }
        }

        uint64_t capturedPos = pos;
        std::vector<uint8_t> capturedBuf = buf;

        pool.enqueue([&, capturedPos, capturedBuf]() mutable {
            LOG_INFO("[carve-task] start pos=" + std::to_string(capturedPos) +
                      " len=" + std::to_string(capturedBuf.size()));
            auto found = carver.carveBuffer(capturedPos, capturedBuf.data(),
                                            capturedBuf.size(), sigs, scanner);
            LOG_INFO("[carve-task] carveBuffer done pos=" + std::to_string(capturedPos) +
                      " found=" + std::to_string(found.size()));
            for (auto& cf : found) {
                std::string outPath = carver.writeFile(cf, scanner);
                if (outPath.empty()) {
                    std::lock_guard<std::mutex> lk(mu);
                    stats_.filesDeduplicated++;
                    continue;
                }
                CarvedFile copy = cf;
                copy.outputPath = outPath;
                copy.confidence = ai.classifyBuffer(
                    capturedBuf.data(),
                    std::min(capturedBuf.size(),(size_t)4096),
                    cf.extension);

                if (config_.extractMetadata) {
                    MetadataExtractor mx;
                    auto meta = mx.extract(outPath);
                    mx.writeJSON(meta, outPath + ".meta.json");
                    std::lock_guard<std::mutex> lk(mu);
                    stats_.filesMetadata++;
                }
                {
                    std::lock_guard<std::mutex> lk(mu);
                    stats_.byExtension[cf.extension]++;
                    allCarved.push_back(copy);
                }
                if (fileCb_) fileCb_(outPath, cf.extension);
            }
            LOG_INFO("[carve-task] end pos=" + std::to_string(capturedPos));
        });

        pos += got;
        progress.update(pos);
        LOG_INFO("[main-loop] advanced to pos=" + std::to_string(pos) +
                  " (" + std::to_string(pos*100/std::max<uint64_t>(end,1)) + "%) pending=" +
                  std::to_string(pool.pendingTasks()));

        if (config_.enableCheckpoint && pos - lastCkpt > CKPT_INTERVAL) {
            pool.waitAll();
            std::lock_guard<std::mutex> lk(mu);
            CheckpointData cd;
            cd.devicePath   = config_.devicePath;
            cd.outputDir    = config_.outputDir;
            cd.lastOffset   = pos;
            cd.deviceSize   = scanner.deviceSize();
            cd.filesFound   = allCarved.size();
            cd.byExtension  = stats_.byExtension;
            ckpt.save(cd);
            lastCkpt = pos;
        }

        if (pos - lastPrint >= PRINT_INTERVAL) { progress.print(); lastPrint = pos; }
        if (pos - lastSync  >= SYNC_INTERVAL)  {
            LOG_INFO("[main-loop] syncing — waiting for " + std::to_string(pool.pendingTasks()) + " pending tasks");
            pool.waitAll();
            LOG_INFO("[main-loop] sync complete");
            lastSync = pos;
        }
    }
    pool.waitAll();
    pool.stop();
    progress.finish();
    scanner.close();
    return allCarved;
}

size_t RecoveryEngine::phase3_repair(std::vector<CarvedFile>& carved) {
    Reconstructor recon;
    size_t repaired = 0;
    for (auto& cf : carved) {
        if (!recon.canRepair(cf.extension)) continue;
        if (cf.outputPath.empty() || !fs::exists(cf.outputPath)) continue;
        auto r = recon.repair(cf.outputPath, cf.extension);
        if (r.success) {
            cf.repaired = true;
            ++repaired;
            // Replace original with repaired version if different
            if (r.outputPath != cf.outputPath && fs::exists(r.outputPath)) {
                try {
                    fs::remove(cf.outputPath);           // delete broken original
                    fs::rename(r.outputPath, cf.outputPath); // rename repaired to original name
                } catch (...) {
                    // Keep repaired file with repaired_ prefix if rename fails
                }
            }
        }
    }
    return repaired;
}

void RecoveryEngine::phase4_report(const RecoveryStats& stats,
                                    const std::vector<CarvedFile>& carved) {
    writeReport(stats, carved, config_.outputDir + "/recovery_report.txt");
}

void RecoveryEngine::phase5_preview(const std::vector<CarvedFile>& carved) {
    PreviewGenerator pg;
    std::vector<PreviewEntry> entries;
    for (auto& cf : carved) {
        if (cf.outputPath.empty()) continue;
        PreviewEntry e = pg.buildEntry(cf.outputPath, cf.confidence);
        e.repaired = cf.repaired;
        pg.generateThumbnail(e, 120);
        entries.push_back(e);
    }
    std::string gp = config_.outputDir + "/gallery.html";
    pg.generateHTMLGallery(entries, gp, config_.devicePath);
    std::cout << "\n🖼  Gallery: " << gp << "\n";
}

void RecoveryEngine::phase6_sqliteCarve(const std::vector<CarvedFile>& carved) {
    SQLiteCarver sc;
    size_t total = 0;
    for (auto& cf : carved) {
        if (cf.extension != "sqlite" && cf.extension != "db") continue;
        if (!fs::exists(cf.outputPath)) continue;
        auto r = sc.carve(cf.outputPath);
        if (!r.success || r.records.empty()) continue;
        sc.writeRecords(r, cf.outputPath + "_records.json");
        total += r.records.size();
    }
    stats_.sqliteRecords = total;
}

void RecoveryEngine::writeReport(const RecoveryStats& stats,
                                  const std::vector<CarvedFile>& carved,
                                  const std::string& reportPath) {
    std::ofstream f(reportPath);
    if (!f) return;
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);

    f << "═══════════════════════════════════════════════════════\n";
    f << "       NeuroRecover v3.0  —  Recovery Report\n";
    f << "═══════════════════════════════════════════════════════\n\n";
    f << "Generated  : " << std::put_time(std::localtime(&t),"%Y-%m-%d %H:%M:%S") << "\n";
    f << "Device     : " << config_.devicePath << "\n";
    f << "Output     : " << config_.outputDir  << "\n";
    f << "Filesystem : " << stats.fsType       << "\n";
    if (!stats.encryptionType.empty())
        f << "Encryption : " << stats.encryptionType << " ⚠️\n";
    if (stats.resumed) f << "Session    : Resumed\n";

    f << "\n─── Summary ─────────────────────────────────────────\n";
    f << "Bytes scanned  : " << stats.bytesScanned      << "\n";
    f << "Files found    : " << stats.filesFound        << "\n";
    f << "Files repaired : " << stats.filesRepaired     << "\n";
    f << "Dups skipped   : " << stats.filesDeduplicated << "\n";
    f << "Filtered out   : " << stats.filesFiltered     << "\n";
    f << "SQLite records : " << stats.sqliteRecords     << "\n";
    f << "Elapsed        : " << std::fixed << std::setprecision(1)
                             << stats.elapsedSeconds << "s\n\n";

    f << "─── By Type ─────────────────────────────────────────\n";
    std::vector<std::pair<std::string,size_t>> sv(stats.byExtension.begin(),
                                                   stats.byExtension.end());
    std::sort(sv.begin(),sv.end(),[](auto& a,auto& b){return a.second>b.second;});
    for (auto& [e,c]:sv)
        f << "  " << std::setw(10) << std::left << e << ": " << c << "\n";

    f << "\n─── Files ───────────────────────────────────────────\n";
    for (auto& cf:carved) {
        if (cf.outputPath.empty()) continue;
        f << "  " << cf.outputPath
          << "  [" << cf.extension << "]"
          << "  " << (cf.size/1024) << "KB"
          << "  off=0x" << std::hex << cf.startOffset << std::dec
          << "  conf=" << std::fixed << std::setprecision(2) << cf.confidence
          << (cf.repaired?" [repaired]":"") << "\n";
    }
    f << "\n═══════════════════════════════════════════════════════\n";
}