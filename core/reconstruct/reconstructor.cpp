#include "reconstructor.h"
#include "utils/logger.h"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;

const std::vector<std::string> Reconstructor::SUPPORTED_EXTS = {
    "jpg","jpeg","png","bmp","mp4","mov","avi","mkv","webm","m4v","3gp"
};

Reconstructor::Reconstructor() {}

std::string Reconstructor::repairedPath(const std::string& original) {
    fs::path p(original);
    return (p.parent_path() / ("repaired_" + p.filename().string())).string();
}

bool Reconstructor::canRepair(const std::string& ext) const {
    std::string e = ext;
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    return std::find(SUPPORTED_EXTS.begin(), SUPPORTED_EXTS.end(), e) != SUPPORTED_EXTS.end();
}

Reconstructor::RepairResult Reconstructor::repair(const std::string& inputPath,
                                                    const std::string& ext) {
    RepairResult r;
    r.success      = false;
    r.outputPath   = repairedPath(inputPath);
    r.method       = "none";
    r.originalSize = 0;
    r.repairedSize = 0;

    if (!fs::exists(inputPath)) {
        LOG_ERROR("Reconstructor: file not found: " + inputPath);
        return r;
    }
    r.originalSize = fs::file_size(inputPath);

    std::string e = ext;
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);

    if (e == "jpg" || e == "jpeg") {
        r.success = jpegRepair_.repair(inputPath, r.outputPath);
        r.method  = "jpeg_repair";
    } else if (e == "bmp") {
        r.success = bmpRepair_.repair(inputPath, r.outputPath);
        r.method  = "bmp_repair";
    } else if (e == "png") {
        r.success = pngRepair_.repair(inputPath, r.outputPath);
        r.method  = "png_repair";
    } else if (e == "mp4" || e == "mov" || e == "m4v" || e == "3gp") {
        r.success = videoRepair_.repairMP4(inputPath, r.outputPath);
        r.method  = "mp4_repair";
    } else if (e == "avi") {
        r.success = videoRepair_.repairAVI(inputPath, r.outputPath);
        r.method  = "avi_repair";
    } else if (e == "mkv" || e == "webm") {
        r.success = videoRepair_.repairMKV(inputPath, r.outputPath);
        r.method  = "mkv_copy";
    } else {
        // Generic: just copy
        std::ifstream in(inputPath, std::ios::binary);
        std::ofstream out(r.outputPath, std::ios::binary);
        if (in && out) { out << in.rdbuf(); r.success = true; }
        r.method = "generic_copy";
    }

    if (r.success && fs::exists(r.outputPath))
        r.repairedSize = fs::file_size(r.outputPath);

    return r;
}
