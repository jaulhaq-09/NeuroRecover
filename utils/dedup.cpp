#include "dedup.h"
#include "hash.h"
#include <fstream>

DedupEngine::DedupEngine() : duplicates_(0) {}

bool DedupEngine::isDuplicate(const std::vector<uint8_t>& data) {
    return isDuplicate(HashUtils::fnv64(data));
}

bool DedupEngine::isDuplicate(uint64_t hash) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (seen_.count(hash)) {
        ++duplicates_;
        return true;
    }
    seen_.insert(hash);
    return false;
}

bool DedupEngine::isDuplicateFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    return isDuplicate(buf);
}

void DedupEngine::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    seen_.clear();
    duplicates_ = 0;
}

size_t DedupEngine::uniqueCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return seen_.size();
}

size_t DedupEngine::duplicateCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return duplicates_;
}
