#pragma once
#include <unordered_set>
#include <string>
#include <cstdint>
#include <vector>
#include <mutex>

class DedupEngine {
public:
    DedupEngine();
    bool isDuplicate(const std::vector<uint8_t>& data);
    bool isDuplicate(uint64_t hash);
    bool isDuplicateFile(const std::string& path);
    void clear();
    size_t uniqueCount() const;
    size_t duplicateCount() const;

private:
    std::unordered_set<uint64_t> seen_;
    mutable std::mutex mutex_;
    size_t duplicates_;
};
