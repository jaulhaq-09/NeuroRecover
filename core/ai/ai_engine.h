#pragma once
#include <vector>
#include <cstdint>
#include <string>

// AI Engine: entropy-based classification & boundary detection
class AIEngine {
public:
    AIEngine();

    // Returns 0.0-1.0 confidence that the buffer starts a file of given type
    double classifyBuffer(const uint8_t* buf, size_t len, const std::string& ext) const;

    // Detects where a file likely ends based on entropy drop
    size_t detectBoundary(const uint8_t* buf, size_t len, const std::string& ext) const;

    // Returns estimated file type from byte patterns
    std::string inferType(const uint8_t* buf, size_t len) const;

    // Shannon entropy
    static double entropy(const uint8_t* buf, size_t len);

    // Returns true if buffer looks like valid media data
    bool isValidMediaData(const uint8_t* buf, size_t len) const;

private:
    double entropyThreshold_;
    static constexpr size_t WINDOW = 512;
};
