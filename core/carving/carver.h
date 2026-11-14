#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include "file_signatures.h"
#include "core/scanner/scanner.h"
#include "utils/config.h"
#include "utils/dedup.h"

struct CarvedFile {
    uint64_t    startOffset;
    uint64_t    endOffset;
    size_t      size;
    std::string extension;
    std::string outputPath;
    double      confidence;
    bool        truncated = false;
    bool        repaired  = false;
};

class FileCarver {
public:
    FileCarver(const RecoveryConfig& config, DedupEngine& dedup);

    // Carve from a memory buffer (sliding window over disk reads)
    std::vector<CarvedFile> carveBuffer(uint64_t baseOffset,
                                        const uint8_t* buf, size_t len,
                                        const std::vector<FileSignature>& sigs,
                                        Scanner& scanner);

    // Write a carved file to disk, return output path
    std::string writeFile(const CarvedFile& cf, Scanner& scanner);

    // Progress callback
    using ProgressCb = std::function<void(uint64_t found)>;
    void setProgressCallback(ProgressCb cb) { progressCb_ = cb; }

    size_t totalCarved() const { return totalCarved_; }

private:
    const RecoveryConfig& config_;
    DedupEngine&          dedup_;
    ProgressCb            progressCb_;
    size_t                totalCarved_ = 0;
    size_t                fileIndex_   = 0;

    size_t estimateSize(uint64_t startOffset, const FileSignature& sig, Scanner& scanner, size_t& readBudget);
    bool   findFooter(uint64_t startOffset, const FileSignature& sig,
                      Scanner& scanner, uint64_t& footerOffset, size_t& readBudget);
    std::string makeOutputPath(const std::string& ext);
    // Returns MP3 frame length in bytes from a valid MPEG1 Layer III header,
    // or 0 if the header bits don't map to a real bitrate/samplerate combo.
    int mp3FrameLength(uint8_t b2, uint8_t b3);
    // Returns AAC ADTS frame length in bytes from the 7-byte header, or 0
    // if fields look invalid.
    int aacFrameLength(const uint8_t* hdr7);
    // Walks the actual JPEG marker chain from SOI to EOI and verifies it's
    // internally consistent. Catches fragmentation-corrupted files that
    // have valid-looking start/end bytes but garbage spliced in the middle
    // (undetectable by header/footer checks alone).
    bool validateJPEGStructure(uint64_t startOffset, size_t size, Scanner& scanner);
};
