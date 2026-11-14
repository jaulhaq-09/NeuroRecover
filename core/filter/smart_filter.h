#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include "core/carving/carver.h"

struct FilterCriteria {
    // Size filters
    size_t minSizeBytes  = 0;
    size_t maxSizeBytes  = 0;   // 0 = unlimited

    // Confidence filter
    double minConfidence = 0.0;

    // Type filter
    std::vector<std::string> includeExtensions; // empty = all
    std::vector<std::string> excludeExtensions;

    // Repair filter
    int repairStatus = -1;  // -1=all, 0=not repaired, 1=repaired only

    // Date range (from EXIF, format "YYYY:MM:DD")
    std::string dateFrom;
    std::string dateTo;

    // Custom predicate
    std::function<bool(const CarvedFile&)> customPredicate;
};

struct FilterStats {
    size_t totalIn     = 0;
    size_t passedCount = 0;
    size_t rejectedSize     = 0;
    size_t rejectedConf     = 0;
    size_t rejectedType     = 0;
    size_t rejectedRepair   = 0;
};

class SmartFilter {
public:
    SmartFilter();

    // Apply filter to a list of carved files, returns passing files
    std::vector<CarvedFile> apply(const std::vector<CarvedFile>& files,
                                   const FilterCriteria& criteria,
                                   FilterStats* stats = nullptr);

    // Apply filter and physically delete non-passing files from disk
    std::vector<CarvedFile> applyAndClean(const std::vector<CarvedFile>& files,
                                           const FilterCriteria& criteria,
                                           FilterStats* stats = nullptr);

    // Build criteria from command-line style strings
    static FilterCriteria fromArgs(const std::string& minSize,
                                   const std::string& maxSize,
                                   double minConf,
                                   const std::string& extensions);

    // Print filter stats summary
    static void printStats(const FilterStats& stats);

private:
    bool passes(const CarvedFile& f, const FilterCriteria& c, FilterStats& stats) const;
    static size_t parseSize(const std::string& s); // "10MB" -> bytes
};
