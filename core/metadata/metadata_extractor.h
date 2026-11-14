#pragma once
#include <string>
#include <map>
#include <vector>
#include <cstdint>

struct FileMetadata {
    std::string              filePath;
    std::string              extension;
    size_t                   fileSize   = 0;
    std::string              sha256;

    // EXIF / image metadata
    int         width        = 0;
    int         height       = 0;
    int         bitsPerSample= 0;
    std::string make;           // Camera make
    std::string model;          // Camera model
    std::string software;
    std::string dateTimeOriginal;
    std::string dateTimeDigitized;
    double      gpsLat       = 0.0;
    double      gpsLon       = 0.0;
    bool        hasGPS       = false;

    // General
    std::string mimeType;
    std::map<std::string,std::string> extra; // any additional tags
};

class MetadataExtractor {
public:
    MetadataExtractor();
    FileMetadata extract(const std::string& filePath);
    std::string  toJSON(const FileMetadata& meta) const;
    bool         writeJSON(const FileMetadata& meta, const std::string& outPath) const;

private:
    FileMetadata extractJPEG(const std::string& path);
    FileMetadata extractPNG(const std::string& path);
    FileMetadata extractMP4(const std::string& path);
    FileMetadata extractGeneric(const std::string& path);

    // EXIF parsing helpers
    bool     parseEXIF(const uint8_t* data, size_t len, FileMetadata& meta);
    uint16_t readU16(const uint8_t* p, bool bigEndian) const;
    uint32_t readU32(const uint8_t* p, bool bigEndian) const;
    std::string readString(const uint8_t* data, size_t dataLen, uint32_t offset, uint32_t len) const;
    double   readRational(const uint8_t* data, size_t dataLen, uint32_t offset, bool bigEndian) const;
};
