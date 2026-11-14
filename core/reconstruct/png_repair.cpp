#include "png_repair.h"
#include "utils/logger.h"
#include <fstream>
#include <cstring>
#include <algorithm>

const uint8_t PNGRepair::PNG_SIG[8] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A};

PNGRepair::PNGRepair() {}

uint32_t PNGRepair::readUint32BE(const uint8_t* p) const {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
void PNGRepair::writeUint32BE(uint8_t* p, uint32_t v) const {
    p[0]=(v>>24)&0xFF; p[1]=(v>>16)&0xFF; p[2]=(v>>8)&0xFF; p[3]=v&0xFF;
}

uint32_t PNGRepair::crc32(const uint8_t* buf, size_t len) const {
    static uint32_t table[256] = {};
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j)
                c = (c&1) ? (0xEDB88320^(c>>1)) : (c>>1);
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) c = table[(c^buf[i])&0xFF] ^ (c>>8);
    return c ^ 0xFFFFFFFF;
}

bool PNGRepair::isValid(const uint8_t* data, size_t len) const {
    if (len < 8) return false;
    if (std::memcmp(data, PNG_SIG, 8) != 0) return false;
    // Check for IEND chunk near end
    if (len < 12) return false;
    const uint8_t* end = data + len - 12;
    return end[4]=='I' && end[5]=='E' && end[6]=='N' && end[7]=='D';
}

bool PNGRepair::fixSignature(std::vector<uint8_t>& data) {
    if (data.size() < 8) return false;
    bool fixed = false;
    for (int i = 0; i < 8; ++i) {
        if (data[i] != PNG_SIG[i]) { data[i] = PNG_SIG[i]; fixed = true; }
    }
    if (fixed) LOG_INFO("PNG: fixed signature bytes");
    return true;
}

bool PNGRepair::fixIENDChunk(std::vector<uint8_t>& data) {
    // Check if IEND already present
    if (data.size() >= 12) {
        const uint8_t* end = data.data() + data.size() - 12;
        if (end[4]=='I' && end[5]=='E' && end[6]=='N' && end[7]=='D') return true;
    }
    // Append IEND chunk: 0 length + "IEND" + CRC
    const uint8_t IEND_TYPE[4] = {'I','E','N','D'};
    uint32_t iendCRC = crc32(IEND_TYPE, 4);
    // length=0
    data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(0);
    // type
    for (auto b : IEND_TYPE) data.push_back(b);
    // CRC
    data.push_back((iendCRC>>24)&0xFF); data.push_back((iendCRC>>16)&0xFF);
    data.push_back((iendCRC>>8)&0xFF);  data.push_back(iendCRC&0xFF);
    LOG_INFO("PNG: appended IEND chunk");
    return true;
}

std::vector<PNGRepair::ChunkInfo> PNGRepair::parseChunks(const uint8_t* data, size_t len) const {
    std::vector<ChunkInfo> chunks;
    size_t pos = 8; // skip signature
    while (pos + 12 <= len) {
        ChunkInfo c;
        c.offset = pos;
        c.length = readUint32BE(data+pos);
        c.type   = std::string(reinterpret_cast<const char*>(data+pos+4), 4);
        if (pos + 12 + c.length > len) break;
        c.crc    = readUint32BE(data + pos + 8 + c.length);
        chunks.push_back(c);
        pos += 12 + c.length;
        if (c.type == "IEND") break;
    }
    return chunks;
}

bool PNGRepair::repair(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) return false;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    in.close();

    if (isValid(data.data(), data.size())) {
        std::ofstream out(outputPath, std::ios::binary);
        out.write(reinterpret_cast<char*>(data.data()), data.size());
        return true;
    }
    fixSignature(data);
    fixIENDChunk(data);

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<char*>(data.data()), data.size());
    LOG_INFO("PNG repaired: " + outputPath);
    return true;
}
