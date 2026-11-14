#pragma once
#include <string>
#include <vector>
#include <cstdint>

class PNGRepair {
public:
    PNGRepair();
    bool repair(const std::string& inputPath, const std::string& outputPath);
    bool isValid(const uint8_t* data, size_t len) const;

    struct ChunkInfo {
        uint32_t    length;
        std::string type;
        uint32_t    crc;
        size_t      offset;
    };

    std::vector<ChunkInfo> parseChunks(const uint8_t* data, size_t len) const;

private:
    static const uint8_t PNG_SIG[8];
    bool fixSignature(std::vector<uint8_t>& data);
    bool fixIENDChunk(std::vector<uint8_t>& data);
    uint32_t crc32(const uint8_t* buf, size_t len) const;
    uint32_t readUint32BE(const uint8_t* p) const;
    void     writeUint32BE(uint8_t* p, uint32_t v) const;
};
