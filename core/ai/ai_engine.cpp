#include "ai_engine.h"
#include <cmath>
#include <array>
#include <algorithm>
#include <cstring>

AIEngine::AIEngine() : entropyThreshold_(6.5) {}

double AIEngine::entropy(const uint8_t* buf, size_t len) {
    if (len == 0) return 0.0;
    std::array<uint64_t,256> freq{};
    for (size_t i = 0; i < len; ++i) freq[buf[i]]++;
    double h = 0.0;
    for (auto c : freq) {
        if (c == 0) continue;
        double p = (double)c / len;
        h -= p * std::log2(p);
    }
    return h;
}

double AIEngine::classifyBuffer(const uint8_t* buf, size_t len, const std::string& ext) const {
    if (len < 4) return 0.0;
    double e = entropy(buf, std::min(len, (size_t)4096));

    // JPEG: high entropy media data
    if (ext == "jpg" || ext == "jpeg") {
        if (buf[0]==0xFF && buf[1]==0xD8 && buf[2]==0xFF) return 0.95;
        return 0.1;
    }
    // PNG: well-defined 8-byte signature
    if (ext == "png") {
        const uint8_t PNG_SIG[8] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A};
        if (len >= 8 && std::memcmp(buf, PNG_SIG, 8) == 0) return 0.99;
        return 0.05;
    }
    // MP4/MOV: ftyp box
    if (ext == "mp4" || ext == "mov") {
        if (len >= 8 && buf[4]=='f' && buf[5]=='t' && buf[6]=='y' && buf[7]=='p') return 0.92;
        return 0.15;
    }
    // PDF
    if (ext == "pdf") {
        if (len >= 4 && buf[0]=='%' && buf[1]=='P' && buf[2]=='D' && buf[3]=='F') return 0.98;
        return 0.05;
    }
    // Generic: use entropy score
    return e / 8.0 * 0.6;
}

size_t AIEngine::detectBoundary(const uint8_t* buf, size_t len, const std::string& ext) const {
    if (len < WINDOW * 2) return len;
    // Scan forward; when entropy drops significantly, treat as boundary
    double baseEntropy = entropy(buf, WINDOW);
    for (size_t i = WINDOW; i + WINDOW < len; i += WINDOW) {
        double e = entropy(buf + i, WINDOW);
        // Sudden entropy drop from high (compressed media) to low (zeros/text)
        if (baseEntropy > 6.0 && e < 1.0) return i;
        // Sustained zero block = end of file padding
        bool allZero = true;
        for (size_t j = i; j < i + 512 && j < len; ++j)
            if (buf[j] != 0) { allZero = false; break; }
        if (allZero) return i;
    }
    return len;
}

std::string AIEngine::inferType(const uint8_t* buf, size_t len) const {
    if (len < 4) return "bin";
    // Check common signatures
    if (buf[0]==0xFF && buf[1]==0xD8 && buf[2]==0xFF) return "jpg";
    if (buf[0]==0x89 && buf[1]==0x50 && buf[2]==0x4E && buf[3]==0x47) return "png";
    if (buf[0]==0x47 && buf[1]==0x49 && buf[2]==0x46) return "gif";
    if (len>=8 && buf[4]=='f' && buf[5]=='t' && buf[6]=='y' && buf[7]=='p') return "mp4";
    if (buf[0]==0x1A && buf[1]==0x45 && buf[2]==0xDF && buf[3]==0xA3) return "mkv";
    if (buf[0]==0x52 && buf[1]==0x49 && buf[2]==0x46 && buf[3]==0x46) return "riff"; // wav or avi
    if (buf[0]==0x25 && buf[1]==0x50 && buf[2]==0x44 && buf[3]==0x46) return "pdf";
    if (buf[0]==0x50 && buf[1]==0x4B && buf[2]==0x03 && buf[3]==0x04) return "zip";
    if (buf[0]==0xFF && (buf[1]==0xFB||buf[1]==0xF3||buf[1]==0xF2)) return "mp3";
    if (buf[0]==0x66 && buf[1]==0x4C && buf[2]==0x61 && buf[3]==0x43) return "flac";
    return "bin";
}

bool AIEngine::isValidMediaData(const uint8_t* buf, size_t len) const {
    if (len < 16) return false;
    double e = entropy(buf, std::min(len, (size_t)4096));
    // Valid media should have moderate-to-high entropy (not all zeros, not random)
    return e > 3.0 && e < 7.95;
}
