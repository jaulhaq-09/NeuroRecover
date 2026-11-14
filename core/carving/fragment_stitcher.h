#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "carver.h"

// Attempts to join fragmented files that were split across disk sectors
class FragmentStitcher {
public:
    struct Fragment {
        uint64_t offset;
        size_t   size;
        std::string extension;
    };

    explicit FragmentStitcher(const std::string& outputDir);
    // Try to stitch adjacent or nearby fragments of the same type
    bool stitch(const std::vector<Fragment>& fragments,
                const std::string& extension,
                const std::string& outPath);

    // Group fragments by extension
    static std::vector<std::vector<Fragment>> group(const std::vector<Fragment>& all);

private:
    std::string outputDir_;
    static constexpr uint64_t MAX_GAP = 4096; // bytes between fragments
};
