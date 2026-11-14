#include "smart_filter.h"
#include "utils/logger.h"
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <cctype>

namespace fs = std::filesystem;

SmartFilter::SmartFilter() {}

size_t SmartFilter::parseSize(const std::string& s) {
    if (s.empty()) return 0;
    size_t val = 0;
    std::string num, unit;
    for (char c : s) {
        if (std::isdigit(c)) num += c;
        else unit += (char)std::tolower(c);
    }
    try { val = std::stoull(num); } catch(...) { return 0; }
    if (unit == "kb" || unit == "k") return val * 1024;
    if (unit == "mb" || unit == "m") return val * 1024 * 1024;
    if (unit == "gb" || unit == "g") return val * 1024 * 1024 * 1024ULL;
    return val;
}

FilterCriteria SmartFilter::fromArgs(const std::string& minSize,
                                      const std::string& maxSize,
                                      double minConf,
                                      const std::string& extensions) {
    FilterCriteria c;
    c.minSizeBytes  = parseSize(minSize);
    c.maxSizeBytes  = parseSize(maxSize);
    c.minConfidence = minConf;
    // Parse comma-separated extensions
    std::string tok;
    for (char ch : extensions + ",") {
        if (ch == ',') {
            if (!tok.empty()) {
                std::transform(tok.begin(),tok.end(),tok.begin(),::tolower);
                c.includeExtensions.push_back(tok);
                tok.clear();
            }
        } else tok += ch;
    }
    return c;
}

bool SmartFilter::passes(const CarvedFile& f, const FilterCriteria& c,
                          FilterStats& stats) const {
    // Size check
    if (c.minSizeBytes > 0 && f.size < c.minSizeBytes) {
        stats.rejectedSize++; return false;
    }
    if (c.maxSizeBytes > 0 && f.size > c.maxSizeBytes) {
        stats.rejectedSize++; return false;
    }
    // Confidence check
    if (f.confidence < c.minConfidence) {
        stats.rejectedConf++; return false;
    }
    // Extension include
    if (!c.includeExtensions.empty()) {
        bool found = false;
        for (auto& e : c.includeExtensions)
            if (f.extension == e) { found = true; break; }
        if (!found) { stats.rejectedType++; return false; }
    }
    // Extension exclude
    for (auto& e : c.excludeExtensions)
        if (f.extension == e) { stats.rejectedType++; return false; }
    // Repair status
    if (c.repairStatus == 1 && !f.repaired) { stats.rejectedRepair++; return false; }
    if (c.repairStatus == 0 &&  f.repaired) { stats.rejectedRepair++; return false; }
    // Custom predicate
    if (c.customPredicate && !c.customPredicate(f)) return false;
    return true;
}

std::vector<CarvedFile> SmartFilter::apply(const std::vector<CarvedFile>& files,
                                            const FilterCriteria& criteria,
                                            FilterStats* statsOut) {
    FilterStats stats;
    stats.totalIn = files.size();
    std::vector<CarvedFile> result;
    result.reserve(files.size());
    for (auto& f : files) {
        if (passes(f, criteria, stats)) { result.push_back(f); stats.passedCount++; }
    }
    if (statsOut) *statsOut = stats;
    LOG_INFO("SmartFilter: " + std::to_string(stats.passedCount) +
             "/" + std::to_string(stats.totalIn) + " files passed");
    return result;
}

std::vector<CarvedFile> SmartFilter::applyAndClean(const std::vector<CarvedFile>& files,
                                                     const FilterCriteria& criteria,
                                                     FilterStats* statsOut) {
    FilterStats stats;
    stats.totalIn = files.size();
    std::vector<CarvedFile> result;
    for (auto& f : files) {
        if (passes(f, criteria, stats)) {
            result.push_back(f);
            stats.passedCount++;
        } else {
            // Remove file from disk
            if (!f.outputPath.empty() && fs::exists(f.outputPath)) {
                fs::remove(f.outputPath);
                LOG_DEBUG("SmartFilter removed: " + f.outputPath);
            }
        }
    }
    if (statsOut) *statsOut = stats;
    LOG_INFO("SmartFilter cleaned: kept " + std::to_string(result.size()) +
             ", removed " + std::to_string(files.size()-result.size()));
    return result;
}

void SmartFilter::printStats(const FilterStats& stats) {
    std::cout << "\n[SmartFilter] Results:\n";
    std::cout << "  Total input  : " << stats.totalIn     << "\n";
    std::cout << "  Passed       : " << stats.passedCount << "\n";
    std::cout << "  Rejected by size      : " << stats.rejectedSize   << "\n";
    std::cout << "  Rejected by confidence: " << stats.rejectedConf   << "\n";
    std::cout << "  Rejected by type      : " << stats.rejectedType   << "\n";
    std::cout << "  Rejected by repair    : " << stats.rejectedRepair << "\n";
}
