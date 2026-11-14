#pragma once
#include <string>
#include <vector>
#include <cstdint>

class BMPRepair {
public:
    BMPRepair() = default;

    // Returns true if the file is a valid openable BMP
    bool isValid(const uint8_t* data, size_t len) const;

    // Fix BMP header fields so Windows Photo Viewer can open it
    bool repair(const std::string& inputPath, const std::string& outputPath);

    struct BMPInfo {
        bool     valid       = false;
        uint32_t fileSize    = 0;
        uint32_t dataOffset  = 0;
        int32_t  width       = 0;
        int32_t  height      = 0;
        uint16_t bitsPerPixel= 0;
        uint32_t compression = 0;
        std::string error;
    };

    BMPInfo inspect(const uint8_t* data, size_t len) const;

private:
    bool fixHeader(std::vector<uint8_t>& data);
    void writeU16LE(uint8_t* p, uint16_t v) const;
    void writeU32LE(uint8_t* p, uint32_t v) const;
    uint16_t readU16LE(const uint8_t* p) const;
    uint32_t readU32LE(const uint8_t* p) const;
};
