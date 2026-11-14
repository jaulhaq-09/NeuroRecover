#pragma once
#include "scanner.h"
#include "utils/config.h"
#include <vector>
#include <cstdint>

struct EntropyRegion {
    uint64_t offset;
    size_t   length;
    double   entropy;     // 0.0-8.0
    bool     likely_compressed;
    bool     likely_encrypted;
};

class DeepScanner {
public:
    explicit DeepScanner(const RecoveryConfig& config);
    std::vector<EntropyRegion> analyzeEntropy(Scanner& scanner, uint64_t offset, size_t len);
    bool isHighEntropy(const uint8_t* buf, size_t len, double threshold = 7.0);
    double calculateEntropy(const uint8_t* buf, size_t len);

private:
    const RecoveryConfig& config_;
    static constexpr size_t CHUNK = 4096;
};
