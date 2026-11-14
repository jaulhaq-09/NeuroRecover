#include "checkpoint_manager.h"
#include "utils/logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <ctime>

namespace fs = std::filesystem;

CheckpointManager::CheckpointManager(const std::string& checkpointFile)
    : path_(checkpointFile) {}

std::string CheckpointManager::defaultPath(const std::string& outputDir) {
    return outputDir + "/.neurorecover_checkpoint.json";
}

bool CheckpointManager::exists() const { return fs::exists(path_); }
void CheckpointManager::remove()       { if (exists()) fs::remove(path_); }

bool CheckpointManager::hasResumable(const std::string& devicePath) const {
    if (!exists()) return false;
    CheckpointData d;
    CheckpointManager tmp(path_);
    if (!tmp.load(d)) return false;
    return d.devicePath == devicePath && d.lastOffset < d.deviceSize;
}

std::string CheckpointManager::escapeJSON(const std::string& s) const {
    std::string r;
    for (char c : s) {
        if (c=='"')  r += "\\\"";
        else if (c=='\\') r += "\\\\";
        else if (c=='\n') r += "\\n";
        else r += c;
    }
    return r;
}

bool CheckpointManager::save(const CheckpointData& d) {
    fs::create_directories(fs::path(path_).parent_path());
    std::ofstream f(path_);
    if (!f) return false;

    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ts;
    ts << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");

    f << "{\n";
    f << "  \"device\": \""       << escapeJSON(d.devicePath)  << "\",\n";
    f << "  \"output\": \""       << escapeJSON(d.outputDir)   << "\",\n";
    f << "  \"bytes_scanned\": "  << d.bytesScanned             << ",\n";
    f << "  \"device_size\": "    << d.deviceSize               << ",\n";
    f << "  \"last_offset\": "    << d.lastOffset               << ",\n";
    f << "  \"files_found\": "    << d.filesFound               << ",\n";
    f << "  \"files_written\": "  << d.filesWritten             << ",\n";
    f << "  \"elapsed\": "        << d.elapsedSeconds           << ",\n";
    f << "  \"fs_type\": \""      << escapeJSON(d.fsType)       << "\",\n";
    f << "  \"timestamp\": \""    << ts.str()                   << "\",\n";

    // Extension counts
    f << "  \"by_extension\": {";
    bool first = true;
    for (auto& [ext,cnt] : d.byExtension) {
        if (!first) f << ",";
        f << "\n    \"" << escapeJSON(ext) << "\": " << cnt;
        first = false;
    }
    f << "\n  },\n";

    // Recovered paths (last 1000 to keep file small)
    f << "  \"recovered\": [";
    size_t start = d.recoveredPaths.size() > 1000
                 ? d.recoveredPaths.size() - 1000 : 0;
    for (size_t i = start; i < d.recoveredPaths.size(); ++i) {
        if (i != start) f << ",";
        f << "\n    \"" << escapeJSON(d.recoveredPaths[i]) << "\"";
    }
    f << "\n  ]\n}\n";

    LOG_DEBUG("Checkpoint saved: " + path_ + " offset=" + std::to_string(d.lastOffset));
    return true;
}

std::string CheckpointManager::readAll() const {
    std::ifstream f(path_);
    return std::string((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
}

std::string CheckpointManager::parseField(const std::string& json,
                                           const std::string& key) const {
    std::string search = "\"" + key + "\": \"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    auto end = json.find("\"", pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

uint64_t CheckpointManager::parseUInt64(const std::string& json,
                                         const std::string& key) const {
    std::string search = "\"" + key + "\": ";
    auto pos = json.find(search);
    if (pos == std::string::npos) return 0;
    pos += search.size();
    std::string num;
    while (pos < json.size() && (std::isdigit(json[pos]) || json[pos]=='.'))
        num += json[pos++];
    if (num.empty()) return 0;
    try { return (uint64_t)std::stoull(num); } catch(...) { return 0; }
}

bool CheckpointManager::load(CheckpointData& d) {
    if (!exists()) return false;
    std::string json = readAll();
    if (json.empty()) return false;

    d.devicePath     = parseField(json, "device");
    d.outputDir      = parseField(json, "output");
    d.fsType         = parseField(json, "fs_type");
    d.timestamp      = parseField(json, "timestamp");
    d.bytesScanned   = parseUInt64(json, "bytes_scanned");
    d.deviceSize     = parseUInt64(json, "device_size");
    d.lastOffset     = parseUInt64(json, "last_offset");
    d.filesFound     = (size_t)parseUInt64(json, "files_found");
    d.filesWritten   = (size_t)parseUInt64(json, "files_written");
    d.elapsedSeconds = (double)parseUInt64(json, "elapsed");

    LOG_INFO("Checkpoint loaded: offset=" + std::to_string(d.lastOffset) +
             " files=" + std::to_string(d.filesFound) +
             " timestamp=" + d.timestamp);
    return true;
}
