#include "bmp_repair.h"
#include "utils/logger.h"
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>

void BMPRepair::writeU16LE(uint8_t* p, uint16_t v) const {
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
}
void BMPRepair::writeU32LE(uint8_t* p, uint32_t v) const {
    p[0]=v&0xFF; p[1]=(v>>8)&0xFF; p[2]=(v>>16)&0xFF; p[3]=(v>>24)&0xFF;
}
uint16_t BMPRepair::readU16LE(const uint8_t* p) const {
    return (uint16_t)p[0] | ((uint16_t)p[1]<<8);
}
uint32_t BMPRepair::readU32LE(const uint8_t* p) const {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

BMPRepair::BMPInfo BMPRepair::inspect(const uint8_t* data, size_t len) const {
    BMPInfo info;
    if (len < 54) { info.error = "Too small"; return info; }
    if (data[0] != 'B' || data[1] != 'M') { info.error = "Not BMP"; return info; }

    info.fileSize    = readU32LE(data + 2);
    info.dataOffset  = readU32LE(data + 10);
    // BITMAPINFOHEADER starts at offset 14
    uint32_t headerSize = readU32LE(data + 14);
    info.width       = (int32_t)readU32LE(data + 18);
    info.height      = (int32_t)readU32LE(data + 22);  // can be negative (top-down)
    info.bitsPerPixel= readU16LE(data + 28);
    info.compression = readU32LE(data + 30);

    // Basic sanity
    if (info.width <= 0 || abs(info.height) <= 0) {
        info.error = "Invalid dimensions";
        return info;
    }
    if (info.width > 30000 || abs(info.height) > 30000) {
        info.error = "Dimensions too large";
        return info;
    }
    if (info.bitsPerPixel != 1 && info.bitsPerPixel != 4 &&
        info.bitsPerPixel != 8 && info.bitsPerPixel != 16 &&
        info.bitsPerPixel != 24 && info.bitsPerPixel != 32) {
        info.error = "Invalid bits per pixel: " + std::to_string(info.bitsPerPixel);
        return info;
    }

    info.valid = true;
    return info;
}

bool BMPRepair::isValid(const uint8_t* data, size_t len) const {
    return inspect(data, len).valid;
}

bool BMPRepair::fixHeader(std::vector<uint8_t>& data) {
    if (data.size() < 54) return false;
    if (data[0] != 'B' || data[1] != 'M') return false;

    auto info = inspect(data.data(), data.size());

    // Fix 1: Correct file size field if wrong
    if (info.fileSize != (uint32_t)data.size()) {
        writeU32LE(data.data() + 2, (uint32_t)data.size());
        LOG_INFO("BMP: fixed file size field: " + std::to_string(data.size()));
    }

    // Fix 2: Correct reserved bytes (must be 0)
    writeU16LE(data.data() + 6, 0);
    writeU16LE(data.data() + 8, 0);

    // Fix 3: Recalculate correct data offset
    uint32_t headerSize = readU32LE(data.data() + 14);
    if (headerSize < 12) headerSize = 40; // BITMAPINFOHEADER
    uint16_t bpp        = readU16LE(data.data() + 28);

    // Color table size
    uint32_t colorTableSize = 0;
    if (bpp <= 8) {
        uint32_t colorsUsed = readU32LE(data.data() + 46);
        colorTableSize = (colorsUsed > 0 ? colorsUsed : (1u << bpp)) * 4;
    }

    uint32_t correctOffset = 14 + headerSize + colorTableSize;
    if (correctOffset < 54) correctOffset = 54;
    if (correctOffset < (uint32_t)data.size()) {
        if (readU32LE(data.data() + 10) != correctOffset) {
            writeU32LE(data.data() + 10, correctOffset);
            LOG_INFO("BMP: fixed data offset: " + std::to_string(correctOffset));
        }
    }

    // Fix 4: Recalculate row size and validate
    int32_t  w       = (int32_t)readU32LE(data.data() + 18);
    int32_t  h       = (int32_t)readU32LE(data.data() + 22);
    if (h < 0) h = -h; // top-down BMP
    uint32_t rowSize = ((w * bpp + 31) / 32) * 4;
    uint64_t expectedPixels = (uint64_t)rowSize * h;
    uint64_t available      = data.size() > correctOffset
                            ? data.size() - correctOffset : 0;

    if (expectedPixels > available) {
        // Truncated — pad with zeros to make it openable
        size_t needed = correctOffset + expectedPixels;
        if (needed <= 50*1024*1024) { // max 50MB
            data.resize(needed, 0);
            writeU32LE(data.data() + 2, (uint32_t)data.size());
            LOG_INFO("BMP: padded truncated data from " + std::to_string(available) +
                     " to " + std::to_string(expectedPixels) + " bytes");
        }
    }

    return true;
}

bool BMPRepair::repair(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) { LOG_ERROR("BMP repair: cannot open " + inputPath); return false; }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    in.close();

    if (data.size() < 2 || data[0] != 'B' || data[1] != 'M') {
        LOG_WARN("BMP repair: not a BMP file: " + inputPath);
        return false;
    }

    bool fixed = fixHeader(data);

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) { LOG_ERROR("BMP repair: cannot write " + outputPath); return false; }
    out.write(reinterpret_cast<char*>(data.data()), data.size());
    out.close();

    if (fixed) LOG_INFO("BMP repaired: " + outputPath);
    return true;
}
