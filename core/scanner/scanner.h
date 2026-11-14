#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <atomic>
#include "utils/config.h"

struct ScanResult {
    uint64_t offset;
    size_t   length;
    std::string extension;
    std::string filePath;
    bool     repaired = false;
    double   confidence = 1.0;
};

class Scanner {
public:
    explicit Scanner(const RecoveryConfig& config);
    ~Scanner();

    bool open();
    void close();
    bool isOpen() const { return fd_ >= 0 || fileHandle_ != nullptr; }
    uint64_t deviceSize() const { return deviceSize_; }

    bool readAt(uint64_t offset, uint8_t* buf, size_t len, size_t& bytesRead);
    bool readBlock(uint64_t blockIndex, uint8_t* buf, size_t& bytesRead);

    // Callback: called for each buffer read
    using ReadCallback = std::function<void(uint64_t offset, const uint8_t* buf, size_t len)>;
    bool scan(ReadCallback cb);

private:
    const RecoveryConfig& config_;
    int         fd_        = -1;
    void*       fileHandle_ = nullptr; // for Windows HANDLE
    uint64_t    deviceSize_ = 0;
    uint32_t    sectorSize_ = 512;      // real physical sector size, queried at open()
    uint64_t    detectSize();
public:
    static std::atomic<uint64_t> g_readCount;   // DIAGNOSTIC: total readAt() calls, process-wide
};
