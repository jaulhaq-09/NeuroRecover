#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "jpeg_repair.h"
#include "png_repair.h"
#include "video_repair.h"
#include "bmp_repair.h"

class Reconstructor {
public:
    Reconstructor();

    struct RepairResult {
        bool        success;
        std::string outputPath;
        std::string method;
        size_t      originalSize;
        size_t      repairedSize;
    };

    RepairResult repair(const std::string& inputPath, const std::string& ext);
    bool         canRepair(const std::string& ext) const;
    static std::string repairedPath(const std::string& original);

private:
    JPEGRepair  jpegRepair_;
    PNGRepair   pngRepair_;
    VideoRepair videoRepair_;

    static const std::vector<std::string> SUPPORTED_EXTS;
    BMPRepair   bmpRepair_;
};
