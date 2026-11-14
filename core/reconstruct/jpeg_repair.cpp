#include "jpeg_repair.h"
#include "utils/logger.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <vector>

JPEGRepair::JPEGRepair() {}

bool JPEGRepair::isValid(const uint8_t* data, size_t len) const {
    if (len < 4) return false;
    if (data[0] != 0xFF || data[1] != 0xD8) return false;
    // Check for EOI somewhere in last 10 bytes
    for (int i = (int)len-2; i >= (int)len-10 && i >= 0; --i) {
        if (data[i] == 0xFF && data[i+1] == 0xD9) return true;
    }
    return false;
}

bool JPEGRepair::hasValidMarkers(const uint8_t* data, size_t len) const {
    return len >= 2 && data[0] == 0xFF && data[1] == 0xD8;
}

size_t JPEGRepair::findNextValidMarker(const uint8_t* data, size_t len, size_t startPos) {
    for (size_t i = startPos; i + 1 < len; ++i) {
        if (data[i] == 0xFF && data[i+1] != 0x00 && data[i+1] != 0xFF)
            return i;
    }
    return len;
}

bool JPEGRepair::fixSOIMarker(std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;
    if (data[0] == 0xFF && data[1] == 0xD8) return true;
    data.insert(data.begin(), {0xFF, 0xD8});
    LOG_INFO("JPEG: inserted SOI marker");
    return true;
}

bool JPEGRepair::fixEOIMarker(std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;
    // Check last 10 bytes for EOI
    size_t checkFrom = (data.size() >= 10) ? data.size()-10 : 0;
    for (size_t i = checkFrom; i+1 < data.size(); ++i) {
        if (data[i] == 0xFF && data[i+1] == 0xD9) return true; // already has EOI
    }
    // Remove trailing zeros before appending EOI
    size_t lastNonZero = data.size();
    while (lastNonZero > 4 && data[lastNonZero-1] == 0x00) --lastNonZero;
    data.resize(lastNonZero);
    data.push_back(0xFF);
    data.push_back(0xD9);
    LOG_INFO("JPEG: appended EOI marker");
    return true;
}

bool JPEGRepair::removeCorruptedBlocks(std::vector<uint8_t>& data) {
    size_t i = 2; // skip SOI
    size_t originalSize = data.size();
    int fixCount = 0;
    while (i + 1 < data.size()) {
        if (data[i] != 0xFF) {
            size_t next = findNextValidMarker(data.data(), data.size(), i+1);
            if (next >= data.size()) break;
            data.erase(data.begin()+i, data.begin()+next);
            ++fixCount;
            continue;
        }
        uint8_t marker = data[i+1];
        if (marker == 0xD9) break; // EOI
        if (marker == 0xD8 || marker == 0x01 ||
            (marker >= 0xD0 && marker <= 0xD7)) {
            i += 2; continue;
        }
        if (i + 4 > data.size()) break;
        uint16_t segLen = ((uint16_t)data[i+2] << 8) | data[i+3];
        if (segLen < 2 || i + 2 + segLen > data.size()) {
            size_t next = findNextValidMarker(data.data(), data.size(), i+2);
            if (next >= data.size()) { data.resize(i); break; }
            data.erase(data.begin()+i, data.begin()+next);
            ++fixCount;
            continue;
        }
        i += 2 + segLen;
    }
    return fixCount > 0;
}

JPEGRepair::RepairResult JPEGRepair::repairData(std::vector<uint8_t>& data) {
    RepairResult r{false, false, false, 0, ""};
    if (data.empty()) { r.error = "empty data"; return r; }
    if (data.size() < 4) { r.error = "too small"; return r; }
    size_t before = data.size();
    fixSOIMarker(data);
    r.markerFixed = removeCorruptedBlocks(data);
    fixEOIMarker(data);
    r.bytesRemoved = (before > data.size()) ? (before - data.size()) : 0;
    r.success = true;
    return r;
}

bool JPEGRepair::repair(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) { LOG_ERROR("JPEG repair: cannot open " + inputPath); return false; }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    in.close();
    if (data.empty()) return false;

    // Already valid - just copy
    if (isValid(data.data(), data.size())) {
        if (outputPath != inputPath) {
            std::ofstream out(outputPath, std::ios::binary);
            if (!out) return false;
            out.write(reinterpret_cast<char*>(data.data()), data.size());
        }
        return true;
    }

    auto r = repairData(data);
    if (!r.success) return false;

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<char*>(data.data()), data.size());
    LOG_INFO("JPEG repaired: " + outputPath +
             " (removed " + std::to_string(r.bytesRemoved) + " bytes)");
    return true;
}