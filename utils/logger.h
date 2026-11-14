#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <sstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <ctime>

enum class LogLevel { DEBUG, INFO, WARNING, ERROR_LEVEL };

class Logger {
public:
    static Logger& instance();
    void setLevel(LogLevel level);
    void setLogFile(const std::string& path);
    void log(LogLevel level, const std::string& message);
    void debug(const std::string& msg)   { log(LogLevel::DEBUG,   msg); }
    void info(const std::string& msg)    { log(LogLevel::INFO,    msg); }
    void warning(const std::string& msg) { log(LogLevel::WARNING, msg); }
    void error(const std::string& msg)   { log(LogLevel::ERROR_LEVEL,  msg); }
    void setVerbose(bool v) { verbose_ = v; }

private:
    Logger() : level_(LogLevel::INFO), verbose_(false) {}
    std::mutex mutex_;
    LogLevel level_;
    std::ofstream logFile_;
    bool verbose_;
    std::string levelStr(LogLevel l);
    std::string timestamp();
};

#define LOG_DEBUG(msg)   Logger::instance().debug(msg)
#define LOG_INFO(msg)    Logger::instance().info(msg)
#define LOG_WARN(msg)    Logger::instance().warning(msg)
#define LOG_ERROR(msg)   Logger::instance().error(msg)
