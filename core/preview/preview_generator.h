#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct PreviewEntry {
    std::string filePath;
    std::string extension;
    size_t      fileSize;
    int         width;
    int         height;
    std::string dateTime;
    std::string sha256;
    double      confidence;
    bool        repaired;
    // Base64-encoded thumbnail (JPEG, max 120x120)
    std::string thumbnailBase64;
    bool        hasThumbnail = false;
};

class PreviewGenerator {
public:
    PreviewGenerator();

    // Generate thumbnail for a single file, store base64 in entry
    bool generateThumbnail(PreviewEntry& entry, int maxDim = 120);

    // Generate full HTML gallery from a list of entries
    bool generateHTMLGallery(const std::vector<PreviewEntry>& entries,
                              const std::string& outputPath,
                              const std::string& devicePath = "");

    // Build a PreviewEntry from a file path (reads metadata)
    PreviewEntry buildEntry(const std::string& filePath, double confidence = 1.0);

private:
    // JPEG thumbnail generation (pure C++, no deps)
    bool makeJPEGThumbnail(const std::string& path, int maxDim,
                            std::string& base64Out);
    bool makePNGThumbnail(const std::string& path, int maxDim,
                           std::string& base64Out);

    // Downscale raw pixels via bilinear interpolation
    std::vector<uint8_t> downscale(const std::vector<uint8_t>& src,
                                    int srcW, int srcH,
                                    int dstW, int dstH, int channels);

    // Base64 encode
    std::string base64Encode(const uint8_t* data, size_t len);

    // Write a minimal JPEG from raw RGB data
    std::vector<uint8_t> encodeMinimalJPEG(const std::vector<uint8_t>& rgb,
                                            int w, int h);

    std::string formatSize(size_t bytes);
    std::string escapeHTML(const std::string& s);
};
