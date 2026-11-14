#include "deep_scanner.h"
#include <cmath>
#include <array>

DeepScanner::DeepScanner(const RecoveryConfig& config) : config_(config) {}

double DeepScanner::calculateEntropy(const uint8_t* buf, size_t len) {
    if (len == 0) return 0.0;
    std::array<uint64_t, 256> freq{};
    for (size_t i = 0; i < len; ++i) freq[buf[i]]++;
    double h = 0.0;
    for (auto c : freq) {
        if (c == 0) continue;
        double p = (double)c / (double)len;
        h -= p * std::log2(p);
    }
    return h;
}

bool DeepScanner::isHighEntropy(const uint8_t* buf, size_t len, double threshold) {
    return calculateEntropy(buf, len) >= threshold;
}

std::vector<EntropyRegion> DeepScanner::analyzeEntropy(Scanner& scanner, uint64_t offset, size_t len) {
    std::vector<EntropyRegion> regions;
    std::vector<uint8_t> chunk(CHUNK);
    uint64_t pos = offset;
    uint64_t end = offset + len;
    while (pos < end) {
        size_t toRead = std::min((uint64_t)CHUNK, end - pos);
        size_t got = 0;
        scanner.readAt(pos, chunk.data(), toRead, got);
        if (got == 0) break;
        double e = calculateEntropy(chunk.data(), got);
        EntropyRegion r;
        r.offset            = pos;
        r.length            = got;
        r.entropy           = e;
        r.likely_compressed = (e > 7.5);
        r.likely_encrypted  = (e > 7.9);
        regions.push_back(r);
        pos += got;
    }
    return regions;
}
