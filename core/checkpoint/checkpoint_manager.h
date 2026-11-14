#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <map>

// Saves and restores scan progress so interrupted sessions can resume
struct CheckpointData {
    std::string  devicePath;
    std::string  outputDir;
    uint64_t     bytesScanned   = 0;
    uint64_t     deviceSize     = 0;
    uint64_t     lastOffset     = 0;      // resume from here
    size_t       filesFound     = 0;
    size_t       filesWritten   = 0;
    double       elapsedSeconds = 0.0;
    std::string  timestamp;
    std::string  fsType;
    std::map<std::string,size_t> byExtension;
    std::vector<std::string> recoveredPaths; // files already written
};

class CheckpointManager {
public:
    explicit CheckpointManager(const std::string& checkpointFile);

    bool save(const CheckpointData& data);
    bool load(CheckpointData& data);
    bool exists() const;
    void remove();

    // Returns true if a previous interrupted session exists for this device
    bool hasResumable(const std::string& devicePath) const;

    static std::string defaultPath(const std::string& outputDir);

private:
    std::string path_;
    std::string escapeJSON(const std::string& s) const;
    std::string readAll() const;
    std::string parseField(const std::string& json, const std::string& key) const;
    uint64_t    parseUInt64(const std::string& json, const std::string& key) const;
};
