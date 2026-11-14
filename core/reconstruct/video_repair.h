#pragma once
#include <string>
#include <vector>
#include <cstdint>

class VideoRepair {
public:
    VideoRepair();
    bool repairMP4(const std::string& inputPath, const std::string& outputPath);
    bool repairAVI(const std::string& inputPath, const std::string& outputPath);
    bool repairMKV(const std::string& inputPath, const std::string& outputPath);
    bool repair(const std::string& inputPath, const std::string& outputPath,
                const std::string& ext);
    bool isValidMP4(const uint8_t* data, size_t len) const;
    bool isValidAVI(const uint8_t* data, size_t len) const;

private:
    bool rebuildMP4Moov(std::vector<uint8_t>& data);
    bool rebuildAVIIndex(std::vector<uint8_t>& data);
    uint32_t readUint32BE(const uint8_t* p) const;
    uint32_t readUint32LE(const uint8_t* p) const;
    void     writeUint32BE(uint8_t* p, uint32_t v) const;
};
