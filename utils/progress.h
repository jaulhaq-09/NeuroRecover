#pragma once
#include <string>
#include <chrono>
#include <atomic>
#include <cstdint>

class ProgressTracker {
public:
    ProgressTracker();
    void start(uint64_t total, const std::string& label = "");
    void update(uint64_t current);
    void finish();
    void increment(uint64_t by = 1);
    double percent() const;
    std::string eta() const;
    void print() const;

private:
    std::atomic<uint64_t> current_;
    uint64_t total_;
    std::string label_;
    std::chrono::steady_clock::time_point startTime_;
    bool active_;
};
