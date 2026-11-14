#include "progress.h"
#include <iostream>
#include <sstream>
#include <iomanip>

ProgressTracker::ProgressTracker() : current_(0), total_(0), active_(false) {}

void ProgressTracker::start(uint64_t total, const std::string& label) {
    total_     = total;
    label_     = label;
    current_   = 0;
    active_    = true;
    startTime_ = std::chrono::steady_clock::now();
}

void ProgressTracker::update(uint64_t current) { current_ = current; }
void ProgressTracker::increment(uint64_t by)   { current_ += by; }

void ProgressTracker::finish() {
    current_ = total_;
    active_  = false;
    print();
    std::cout << "\n";
}

double ProgressTracker::percent() const {
    if (total_ == 0) return 0.0;
    return (double)current_.load() / (double)total_ * 100.0;
}

std::string ProgressTracker::eta() const {
    uint64_t cur = current_.load();
    if (cur == 0) return "calculating...";
    auto now     = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - startTime_).count();
    double rate    = (double)cur / elapsed;
    if (rate <= 0) return "unknown";
    double remaining = (double)(total_ - cur) / rate;
    int mins = (int)(remaining / 60);
    int secs = (int)remaining % 60;
    std::ostringstream oss;
    oss << mins << "m " << secs << "s";
    return oss.str();
}

void ProgressTracker::print() const {
    double pct = percent();
    int    bar = (int)(pct / 2);  // 50-char bar
    std::cout << "\r" << label_ << " [";
    for (int i = 0; i < 50; ++i) std::cout << (i < bar ? '#' : '-');
    std::cout << "] " << std::fixed << std::setprecision(1) << pct
              << "% ETA:" << eta() << "   " << std::flush;
}
