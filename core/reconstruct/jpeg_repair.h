#pragma once
#include <string>
#include <vector>
#include <cstdint>

class JPEGRepair {
public:
    JPEGRepair();
    bool repair(const std::string& inputPath, const std::string& outputPath);
    bool isValid(const uint8_t* data, size_t len) const;
    bool hasValidMarkers(const uint8_t* data, size_t len) const;

    struct RepairResult {
        bool    success;
        bool    truncated;
        bool    markerFixed;
        size_t  bytesRemoved;
        std::string error;
    };

    RepairResult repairData(std::vector<uint8_t>& data);

private:
    bool fixSOIMarker(std::vector<uint8_t>& data);
    bool fixEOIMarker(std::vector<uint8_t>& data);
    bool removeCorruptedBlocks(std::vector<uint8_t>& data);
    size_t findNextValidMarker(const uint8_t* data, size_t len, size_t startPos);
};
