#include "logger.h"

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::setLevel(LogLevel level) { level_ = level; }

void Logger::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    logFile_.open(path, std::ios::app);
}

std::string Logger::levelStr(LogLevel l) {
    switch(l) {
        case LogLevel::DEBUG:       return "DEBUG";
        case LogLevel::INFO:        return "INFO ";
        case LogLevel::WARNING:     return "WARN ";
        case LogLevel::ERROR_LEVEL: return "ERROR";
        default:                    return "?????";
    }
}

std::string Logger::timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < level_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    std::string line = "[" + timestamp() + "] [" + levelStr(level) + "] " + message;
    if (verbose_ || level >= LogLevel::INFO) {
        std::cout << line << "\n";
        std::cout.flush(); // CRITICAL: flush immediately for Node.js pipe reading
    }
    if (logFile_.is_open()) {
        logFile_ << line << "\n";
        logFile_.flush();
    }
}