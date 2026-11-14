#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "core/scanner/scanner.h"

struct NTFSDataRun {
    uint64_t offset;    // absolute disk offset for this run (UINT64_MAX = sparse/zero-fill run)
    uint64_t length;    // length of this run in bytes
};

struct MFTEntry {
    uint64_t    mftRef = 0;
    std::string name;
    uint64_t    size = 0;
    uint64_t    allocatedSize = 0;
    bool        isDirectory = false;
    bool        isDeleted = false;
    uint64_t    dataOffset = 0;    // offset on disk to FIRST data run (0 = not decoded / resident)
    std::vector<NTFSDataRun> dataRuns;  // ALL data runs in order — needed for fragmented files
    std::string extension;
};

class NTFSParser {
public:
    NTFSParser();
    bool parse(Scanner& scanner, uint64_t partitionOffset = 0);
    const std::vector<MFTEntry>& deletedFiles() const { return deleted_; }
    const std::vector<MFTEntry>& allFiles()     const { return all_; }

private:
    std::vector<MFTEntry> deleted_;
    std::vector<MFTEntry> all_;

    bool parseMFT(Scanner& scanner, uint64_t mftOffset, uint32_t clusterSize,
                  uint64_t partitionOffset);
    bool parseMFTEntry(const uint8_t* buf, size_t len, MFTEntry& entry,
                        uint64_t partitionOffset, uint32_t clusterSize);
    std::string extractName(const uint8_t* attr, size_t len);
    std::vector<NTFSDataRun> extractDataRuns(const uint8_t* attr, size_t attrLen,
                                  uint64_t partOff, uint32_t clusterSize);
    std::string getExtension(const std::string& name);
};
