#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "core/scanner/scanner.h"

enum class FSType { UNKNOWN, NTFS, FAT32, EXFAT, EXT2, EXT3, EXT4, HFS_PLUS, APFS, RAW };

struct FSInfo {
    FSType      type = FSType::UNKNOWN;
    std::string name;
    uint64_t    partitionOffset = 0;
    uint64_t    partitionSize   = 0;
    uint32_t    clusterSize     = 0;
    uint32_t    sectorSize      = 512;
    bool        detected        = false;
};

class FSAnalyzer {
public:
    FSAnalyzer();
    FSInfo detect(Scanner& scanner);
    static std::string fsTypeName(FSType t);

private:
    bool detectNTFS(Scanner& scanner, FSInfo& info);
    bool detectFAT(Scanner& scanner, FSInfo& info);
    bool detectExt(Scanner& scanner, FSInfo& info);
    bool detectHFS(Scanner& scanner, FSInfo& info);
    bool detectAPFS(Scanner& scanner, FSInfo& info);
};
