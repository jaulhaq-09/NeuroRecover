#pragma once
#include <string>
#include <vector>
#include <cstdint>

class HashUtils {
public:
    // FNV-64 fast hash for deduplication
    static uint64_t fnv64(const uint8_t* data, size_t len);
    static uint64_t fnv64(const std::vector<uint8_t>& data);

    // SHA-256 (simple built-in implementation)
    static std::string sha256(const uint8_t* data, size_t len);
    static std::string sha256(const std::vector<uint8_t>& data);
    static std::string sha256File(const std::string& path);
    static std::string toHex(const uint8_t* data, size_t len);
};
