#include "video_repair.h"
#include "utils/logger.h"
#include <fstream>
#include <cstring>
#include <algorithm>

VideoRepair::VideoRepair() {}

uint32_t VideoRepair::readUint32BE(const uint8_t* p) const {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
uint32_t VideoRepair::readUint32LE(const uint8_t* p) const {
    return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
void VideoRepair::writeUint32BE(uint8_t* p, uint32_t v) const {
    p[0]=(v>>24)&0xFF; p[1]=(v>>16)&0xFF; p[2]=(v>>8)&0xFF; p[3]=v&0xFF;
}

bool VideoRepair::isValidMP4(const uint8_t* data, size_t len) const {
    if (len < 8) return false;
    // Check for ftyp box
    return (data[4]=='f' && data[5]=='t' && data[6]=='y' && data[7]=='p');
}

bool VideoRepair::isValidAVI(const uint8_t* data, size_t len) const {
    if (len < 12) return false;
    return (data[0]=='R'&&data[1]=='I'&&data[2]=='F'&&data[3]=='F') &&
           (data[8]=='A'&&data[9]=='V'&&data[10]=='I'&&data[11]==' ');
}

bool VideoRepair::rebuildMP4Moov(std::vector<uint8_t>& data) {
    // Scan for moov atom; if missing, log warning
    size_t i = 0;
    bool   hasMoov = false;
    while (i + 8 <= data.size()) {
        uint32_t boxSize = readUint32BE(data.data()+i);
        if (boxSize < 8 || i + boxSize > data.size()) break;
        if (std::memcmp(data.data()+i+4, "moov", 4) == 0) { hasMoov = true; break; }
        i += boxSize;
    }
    if (!hasMoov) {
        LOG_WARN("MP4: no moov atom found – file may not be playable but data preserved");
    }
    return true; // We preserve raw data even if moov is missing
}

bool VideoRepair::rebuildAVIIndex(std::vector<uint8_t>& data) {
    // Check for idx1 chunk; if missing, note it
    bool hasIndex = false;
    for (size_t i = 0; i + 4 <= data.size(); ++i) {
        if (data[i]=='i'&&data[i+1]=='d'&&data[i+2]=='x'&&data[i+3]=='1') {
            hasIndex = true; break;
        }
    }
    if (!hasIndex) {
        LOG_WARN("AVI: no idx1 index – video may not seek correctly");
    }
    // Fix RIFF size field
    if (data.size() >= 8) {
        uint32_t sz = (uint32_t)(data.size() - 8);
        data[4] = sz&0xFF; data[5]=(sz>>8)&0xFF;
        data[6]=(sz>>16)&0xFF; data[7]=(sz>>24)&0xFF;
    }
    return true;
}

bool VideoRepair::repairMP4(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) return false;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    in.close();
    rebuildMP4Moov(data);
    std::ofstream out(outputPath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<char*>(data.data()), data.size());
    LOG_INFO("MP4 processed: " + outputPath);
    return true;
}

bool VideoRepair::repairAVI(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) return false;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    in.close();
    rebuildAVIIndex(data);
    std::ofstream out(outputPath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<char*>(data.data()), data.size());
    LOG_INFO("AVI processed: " + outputPath);
    return true;
}

bool VideoRepair::repairMKV(const std::string& inputPath, const std::string& outputPath) {
    // MKV (Matroska): copy as-is, preserve EBML structure
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) return false;
    std::ofstream out(outputPath, std::ios::binary);
    out << in.rdbuf();
    LOG_INFO("MKV copied: " + outputPath);
    return true;
}

bool VideoRepair::repair(const std::string& inputPath, const std::string& outputPath,
                          const std::string& ext) {
    if (ext == "mp4" || ext == "mov" || ext == "m4v") return repairMP4(inputPath, outputPath);
    if (ext == "avi")                                  return repairAVI(inputPath, outputPath);
    if (ext == "mkv" || ext == "webm")                 return repairMKV(inputPath, outputPath);
    // Generic: copy
    std::ifstream in(inputPath, std::ios::binary);
    std::ofstream out(outputPath, std::ios::binary);
    if (!in || !out) return false;
    out << in.rdbuf();
    return true;
}
