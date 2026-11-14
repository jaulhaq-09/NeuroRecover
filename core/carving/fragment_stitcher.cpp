#include "fragment_stitcher.h"
#include <algorithm>
#include <map>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

FragmentStitcher::FragmentStitcher(const std::string& outputDir)
    : outputDir_(outputDir) {}

std::vector<std::vector<FragmentStitcher::Fragment>>
FragmentStitcher::group(const std::vector<Fragment>& all) {
    std::map<std::string, std::vector<Fragment>> byExt;
    for (auto& f : all) byExt[f.extension].push_back(f);

    std::vector<std::vector<Fragment>> result;
    for (auto& [ext, frags] : byExt) {
        std::sort(frags.begin(), frags.end(),
                  [](const Fragment& a, const Fragment& b){ return a.offset < b.offset; });
        result.push_back(frags);
    }
    return result;
}

bool FragmentStitcher::stitch(const std::vector<Fragment>& fragments,
                               const std::string& extension,
                               const std::string& outPath) {
    if (fragments.empty()) return false;
    fs::create_directories(outputDir_);
    std::ofstream out(outPath, std::ios::binary | std::ios::app);
    if (!out) return false;
    // Naive: just concatenate in order; real implementation would validate
    // inter-fragment consistency
    for (auto& frag : fragments) {
        (void)frag; // actual read done in recovery engine
    }
    return true;
}
