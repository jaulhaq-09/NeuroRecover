#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "core/scanner/scanner.h"

struct ExtEntry {
    uint32_t    inode;
    std::string name;
    std::string extension;
    uint64_t    size;
    bool        isDeleted;
    bool        isDirectory;
    uint64_t    diskOffset; // estimated disk offset
};

class ExtParser {
public:
    ExtParser();
    bool parse(Scanner& scanner, uint64_t partitionOffset = 0);
    const std::vector<ExtEntry>& deletedFiles() const { return deleted_; }
    const std::vector<ExtEntry>& allFiles()     const { return all_; }

private:
    std::vector<ExtEntry> deleted_;
    std::vector<ExtEntry> all_;

    uint32_t blockSize_    = 4096;
    uint32_t inodesPerGroup_ = 0;
    uint32_t inodeSize_    = 256;
    uint64_t partOff_      = 0;

    bool parseGroupDescriptor(Scanner& scanner, uint32_t group, uint64_t& inodeTableBlock);
    bool parseInodeTable(Scanner& scanner, uint64_t tableBlock, uint32_t groupInodeBase);
    std::string getExtension(const std::string& name);
};
