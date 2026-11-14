#include "fat_parser.h"
#include "utils/logger.h"
#include <cstring>
#include <algorithm>

FATParser::FATParser() {}

bool FATParser::parse(Scanner& scanner, uint64_t partitionOffset) {
    uint8_t boot[512];
    size_t got = 0;
    if (!scanner.readAt(partitionOffset, boot, 512, got) || got < 512) return false;
    if (boot[510] != 0x55 || boot[511] != 0xAA) return false;

    uint16_t bytesPerSector    = boot[11] | (boot[12]<<8);
    uint8_t  sectorsPerCluster = boot[13];
    uint16_t reservedSectors   = boot[14] | (boot[15]<<8);
    uint8_t  numFATs           = boot[16];
    uint32_t fatSize;

    // FAT32 specific
    uint32_t fatSize16 = boot[22] | (boot[23]<<8);
    uint32_t fatSize32 = boot[36] | (boot[37]<<8) | (boot[38]<<16) | (boot[39]<<24);
    isFAT32_  = (fatSize16 == 0);
    fatSize   = isFAT32_ ? fatSize32 : fatSize16;

    clusterSize_ = bytesPerSector * sectorsPerCluster;
    uint64_t fatStart  = partitionOffset + reservedSectors * bytesPerSector;
    uint64_t rootStart = fatStart + numFATs * fatSize * bytesPerSector;

    fatStart_       = fatStart;        // NEW: remember for chain walking later
    bytesPerSector_ = bytesPerSector;  // NEW

    if (isFAT32_) {
        uint32_t rootCluster = boot[44] | (boot[45]<<8) | (boot[46]<<16) | (boot[47]<<24);
        dataStart_ = rootStart;
        rootDirStart_ = rootCluster;
    } else {
        uint16_t rootEntries = boot[17] | (boot[18]<<8);
        dataStart_ = rootStart + (rootEntries * 32 + bytesPerSector - 1) / bytesPerSector * bytesPerSector;
        rootDirStart_ = 0;
    }

    // Read root directory (FAT16: fixed size; FAT32: follow cluster chain)
    size_t rootSize = isFAT32_ ? clusterSize_ : ((boot[17]|(boot[18]<<8)) * 32);
    std::vector<uint8_t> root(rootSize);
    if (!scanner.readAt(rootStart, root.data(), rootSize, got)) return false;
    parseDirectory(root.data(), got);

    LOG_INFO("FAT: parsed, " + std::to_string(all_.size()) + " entries, "
             + std::to_string(deleted_.size()) + " deleted");
    return true;
}

bool FATParser::parseDirectory(const uint8_t* buf, size_t len) {
    for (size_t i = 0; i + 32 <= len; i += 32) {
        const uint8_t* e = buf + i;
        if (e[0] == 0x00) break;         // End of directory
        if (e[11] == 0x0F) continue;      // LFN entry, skip
        if (e[0] == 0xE5 || e[0] != 0x00) { // 0xE5 = deleted
            FATEntry entry;
            entry.isDeleted   = (e[0] == 0xE5);
            entry.isDirectory = (e[11] & 0x10) != 0;
            entry.name        = readName83(e);
            entry.size        = e[28] | (e[29]<<8) | (e[30]<<16) | (uint32_t)(e[31]<<24);
            entry.firstCluster= (e[26] | (e[27]<<8)) |
                                ((uint32_t)(e[20] | (e[21]<<8)) << 16);
            auto dot = entry.name.rfind('.');
            entry.extension   = (dot != std::string::npos) ? entry.name.substr(dot+1) : "";
            std::transform(entry.extension.begin(), entry.extension.end(),
                           entry.extension.begin(), ::tolower);
            all_.push_back(entry);
            if (entry.isDeleted) deleted_.push_back(entry);
        }
    }
    return true;
}

std::string FATParser::readName83(const uint8_t* e) {
    std::string name(reinterpret_cast<const char*>(e), 8);
    std::string ext(reinterpret_cast<const char*>(e+8), 3);
    // Trim spaces
    name.erase(name.find_last_not_of(' ')+1);
    ext.erase(ext.find_last_not_of(' ')+1);
    if (e[0] == 0x05) name[0] = (char)0xE5; // restore deleted marker
    if (!ext.empty()) return name + "." + ext;
    return name;
}

uint64_t FATParser::clusterToOffset(uint32_t cluster) const {
    return dataStart_ + (uint64_t)(cluster - 2) * clusterSize_;
}

std::vector<uint32_t> FATParser::readClusterChain(Scanner& scanner, uint32_t firstCluster) const {
    std::vector<uint32_t> chain;
    if (firstCluster < 2) return chain;
    chain.push_back(firstCluster);

    uint32_t cluster   = firstCluster;
    const uint32_t MAX_CLUSTERS = 200000; // safety cap against corrupt/looping chains

    for (uint32_t i = 0; i < MAX_CLUSTERS; ++i) {
        uint32_t next = 0;
        size_t   got  = 0;

        if (isFAT32_) {
            uint64_t entryOff = fatStart_ + (uint64_t)cluster * 4;
            uint8_t  e[4];
            if (!scanner.readAt(entryOff, e, 4, got) || got < 4) break;
            next = (e[0] | (e[1]<<8) | (e[2]<<16) | (e[3]<<24)) & 0x0FFFFFFF;
            // End-of-chain: 0x0FFFFFF8–0x0FFFFFFF. Free/broken: 0x00000000.
            if (next == 0 || next >= 0x0FFFFFF8) break;
        } else {
            uint64_t entryOff = fatStart_ + (uint64_t)cluster * 2;
            uint8_t  e[2];
            if (!scanner.readAt(entryOff, e, 2, got) || got < 2) break;
            next = e[0] | (e[1]<<8);
            // End-of-chain: 0xFFF8–0xFFFF. Free/broken: 0x0000.
            if (next == 0 || next >= 0xFFF8) break;
        }

        // Deleted files almost always have a zeroed/free FAT chain — if the
        // "next" cluster isn't the very next number, the chain is unreliable
        // post-deletion. Stop here rather than follow garbage.
        if (next != cluster + 1) break;

        chain.push_back(next);
        cluster = next;
    }
    return chain;
}