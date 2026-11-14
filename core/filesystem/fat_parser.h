#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "core/scanner/scanner.h"

struct FATEntry {
    std::string name;
    std::string extension;
    uint64_t    size;
    uint32_t    firstCluster;
    bool        isDeleted;
    bool        isDirectory;
};

class FATParser {
public:
    FATParser();
    bool parse(Scanner& scanner, uint64_t partitionOffset = 0);
    const std::vector<FATEntry>& deletedFiles() const { return deleted_; }
    const std::vector<FATEntry>& allFiles()     const { return all_; }
    uint64_t clusterToOffset(uint32_t cluster) const;

public:
    // Walk the FAT chain starting at firstCluster. Returns list of cluster numbers.
    // If the chain is broken/zeroed (common after deletion), returns just {firstCluster}
    // so the caller can fall back to a contiguous-cluster assumption.
    std::vector<uint32_t> readClusterChain(Scanner& scanner, uint32_t firstCluster) const;

private:
    std::vector<FATEntry> deleted_;
    std::vector<FATEntry> all_;

    uint64_t dataStart_     = 0;
    uint32_t clusterSize_   = 0;
    bool     isFAT32_       = false;
    uint32_t rootDirStart_  = 0;
    uint64_t fatStart_      = 0;   // NEW: byte offset of first FAT table
    uint16_t bytesPerSector_= 512; // NEW: needed to size FAT entries correctly

    bool parseDirectory(const uint8_t* buf, size_t len);
    std::string readName83(const uint8_t* entry);
    static std::string toLower(std::string s);
};
