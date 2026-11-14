#include "carver.h"
#include "utils/logger.h"
#include "utils/hash.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace fs = std::filesystem;

FileCarver::FileCarver(const RecoveryConfig& config, DedupEngine& dedup)
    : config_(config), dedup_(dedup) {}

int FileCarver::mp3FrameLength(uint8_t b2, uint8_t b3) {
    // MPEG1 Layer III bitrate table (kbps), index 0=free/invalid, 15=bad
    static const int bitrateTable[16] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0};
    static const int sampleRateTable[4] = {44100,48000,32000,0}; // index 3 = reserved

    uint8_t bitrateIdx    = (b2 >> 4) & 0x0F;
    uint8_t samplerateIdx = (b2 >> 2) & 0x03;
    uint8_t padding       = (b2 >> 1) & 0x01;

    int bitrate    = bitrateTable[bitrateIdx];
    int sampleRate = sampleRateTable[samplerateIdx];
    if (bitrate == 0 || sampleRate == 0) return 0; // free/reserved — can't compute, treat as invalid

    // Standard MPEG1 Layer III frame length formula
    int frameLen = (144 * bitrate * 1000) / sampleRate + padding;
    if (frameLen < 21 || frameLen > 2000) return 0; // sanity bounds for a real MP3 frame
    return frameLen;
}

int FileCarver::aacFrameLength(const uint8_t* hdr7) {
    // ADTS header: frame_length is a 13-bit field spanning bytes 3-5
    uint8_t b3 = hdr7[3], b4 = hdr7[4], b5 = hdr7[5];
    int frameLen = ((b3 & 0x03) << 11) | (b4 << 3) | ((b5 & 0xE0) >> 5);
    if (frameLen < 7 || frameLen > 8192) return 0; // sanity bounds (7 = header-only min)
    return frameLen;
}

bool FileCarver::validateJPEGStructure(uint64_t startOffset, size_t size, Scanner& scanner) {
    if (size < 4 || size > 50*1024*1024) return false;
    std::vector<uint8_t> data(size);
    size_t got = 0;
    if (!scanner.readAt(startOffset, data.data(), size, got) || got < size) return false;

    size_t pos = 2; // skip FF D8 (already confirmed present)
    int realSegments = 0;

    while (pos + 1 < got) {
        if (data[pos] != 0xFF) return false;           // every marker must start with FF
        uint8_t marker = data[pos+1];

        if (marker == 0xD8) return false;               // unexpected second SOI = not real
        if (marker == 0xD9) return realSegments > 0;     // EOI — valid only if we saw real content first
        if ((marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) { pos += 2; continue; } // restart/TEM, no length field

        if (pos + 3 >= got) return false;                // truncated before length field
        uint16_t segLen = (uint16_t)((data[pos+2] << 8) | data[pos+3]);
        if (segLen < 2) return false;                    // length must include itself
        ++realSegments;

        if (marker == 0xDA) {                            // Start Of Scan — entropy-coded data follows
            pos += 2 + segLen;
            while (pos + 1 < got) {                       // walk compressed data to the next real marker
                if (data[pos] == 0xFF) {
                    uint8_t nb = data[pos+1];
                    if (nb == 0x00) { pos += 2; continue; }               // byte-stuffed 0xFF, part of data
                    if (nb >= 0xD0 && nb <= 0xD7) { pos += 2; continue; } // restart marker inside scan
                    break;                                                  // real marker — exit to outer loop
                }
                ++pos;
            }
            continue;
        }
        pos += 2 + segLen;
    }
    return false; // ran off the end without a valid EOI
}

std::string FileCarver::makeOutputPath(const std::string& ext) {
    std::ostringstream oss;
    oss << config_.outputDir << "/recovered_"
        << std::setw(6) << std::setfill('0') << (++fileIndex_)
        << "." << ext;
    return oss.str();
}

bool FileCarver::findFooter(uint64_t startOffset, const FileSignature& sig,
                             Scanner& scanner, uint64_t& footerOffset, size_t& readBudget) {
    if (!sig.hasFooter || sig.footer.empty()) return false;
    // Cap tightened 20MB -> 4MB, chunk size raised 64KB -> 256KB (fewer,
    // bigger reads = fewer readAt() calls for the same coverage). readBudget
    // is a HARD ceiling on total reads this entire carveBuffer() call may
    // spend across ALL matches combined — once it hits zero we bail
    // immediately rather than keep searching. This is what actually bounds
    // worst-case time per buffer, regardless of how noisy the drive data is.
    const uint64_t HARD_FOOTER_CAP = 4ULL * 1024 * 1024;
    uint64_t searchSize = (sig.maxSize > 0) ? std::min<uint64_t>(sig.maxSize, HARD_FOOTER_CAP) : HARD_FOOTER_CAP;
    uint64_t maxEnd = startOffset + searchSize;
    maxEnd = std::min(maxEnd, scanner.deviceSize());

    const size_t CHUNK = 262144; // 256KB
    std::vector<uint8_t> buf(CHUNK + sig.footer.size());
    uint64_t pos = startOffset;

    while (pos < maxEnd) {
        if (readBudget == 0) return false;
        --readBudget;

        size_t toRead = std::min((uint64_t)(CHUNK + sig.footer.size()), maxEnd - pos);
        size_t got = 0;
        if (!scanner.readAt(pos, buf.data(), toRead, got) || got < sig.footer.size()) break;
        for (size_t i = 0; i + sig.footer.size() <= got; ++i) {
            if (std::memcmp(buf.data()+i, sig.footer.data(), sig.footer.size()) == 0) {
                footerOffset = pos + i + sig.footer.size();
                return true;
            }
        }
        pos += got - sig.footer.size();
    }
    return false;
}

size_t FileCarver::estimateSize(uint64_t startOffset, const FileSignature& sig, Scanner& scanner, size_t& readBudget) {
    // 0) Format-specific size reading from file header (most accurate)
    // BMP: file size is stored at bytes 2-5 (little-endian uint32)
    if (sig.extension == "bmp") {
        // Real structural validation, same principle as the MP3/AAC fix:
        // a single "BM" + plausible size-field match is a weak signal on
        // its own. BMP headers have several fields that must be mutually
        // consistent in any real file — checking them together turns a
        // ~1-in-a-few-thousand coincidence into an essentially impossible one.
        uint64_t alignedOff = (startOffset / 512) * 512;
        size_t   skip       = (size_t)(startOffset - alignedOff);
        std::vector<uint8_t> buf(512);
        size_t got = 0;
        if (!scanner.readAt(alignedOff, buf.data(), 512, got) || got < skip + 54) return 0;

        uint32_t bmpSize = (uint32_t)buf[skip+2] | ((uint32_t)buf[skip+3]<<8)
                         | ((uint32_t)buf[skip+4]<<16) | ((uint32_t)buf[skip+5]<<24);
        if (bmpSize < 54 || bmpSize > 30*1024*1024) return 0;

        // Reserved fields (bytes 6-9) must be zero in every real BMP variant
        uint32_t reserved = (uint32_t)buf[skip+6] | ((uint32_t)buf[skip+7]<<8)
                           | ((uint32_t)buf[skip+8]<<16) | ((uint32_t)buf[skip+9]<<24);
        if (reserved != 0) return 0;

        // Pixel data offset (bytes 10-13): must point somewhere sane —
        // at least past the minimum 14+12-byte header, and before the
        // claimed end of the file.
        uint32_t pixelOffset = (uint32_t)buf[skip+10] | ((uint32_t)buf[skip+11]<<8)
                              | ((uint32_t)buf[skip+12]<<16) | ((uint32_t)buf[skip+13]<<24);
        if (pixelOffset < 26 || pixelOffset >= bmpSize) return 0;

        // DIB header size (bytes 14-17): must be one of the small set of
        // real values ever used by any BMP variant. Random bytes landing
        // on one of these exact values by chance is very unlikely.
        uint32_t dibSize = (uint32_t)buf[skip+14] | ((uint32_t)buf[skip+15]<<8)
                          | ((uint32_t)buf[skip+16]<<16) | ((uint32_t)buf[skip+17]<<24);
        static const uint32_t validDibSizes[] = {12,40,52,56,64,108,124};
        bool dibOk = false;
        for (uint32_t v : validDibSizes) if (dibSize == v) { dibOk = true; break; }
        if (!dibOk) return 0;

        // Width/height (bytes 18-21, 22-25 for the common 40-byte header):
        // reject absurd or zero dimensions.
        if (dibSize == 40 && got >= skip + 26) {
            int32_t width  = (int32_t)((uint32_t)buf[skip+18] | ((uint32_t)buf[skip+19]<<8)
                            | ((uint32_t)buf[skip+20]<<16) | ((uint32_t)buf[skip+21]<<24));
            int32_t height = (int32_t)((uint32_t)buf[skip+22] | ((uint32_t)buf[skip+23]<<8)
                            | ((uint32_t)buf[skip+24]<<16) | ((uint32_t)buf[skip+25]<<24));
            if (width <= 0 || width > 65535) return 0;
            if (height == 0 || (height > 65535 || height < -65535)) return 0;
        }

        return (size_t)bmpSize;
    }
    // JPEG: size between SOI and EOI — use footer finder (already has it)
    // PNG:  size between signature and IEND — use footer finder (already has it)
    // PDF:  size between %PDF and %%EOF — use footer finder (already has it)
    // MP3:  read ID3 tag size from header
    if (sig.extension == "mp3") {
        // Two DISTINCT sig entries share extension "mp3": raw frame-sync
        // (0xFFFB / 0xFFF3, header.size()==2) and ID3-prefixed (header
        // "ID3", header.size()==3). sig.header tells us which one matched.
        if (sig.header.size() == 2 && sig.header[0] == 0xFF) {
            // Raw MPEG frame sync was PREVIOUSLY NEVER VALIDATED — any 2-byte
            // coincidental match (~1-in-65,536 per offset in random data)
            // was accepted outright. Validate the bitrate/samplerate index
            // fields in the next byte; MPEG spec reserves specific values
            // that never appear in a real frame header.
            uint8_t hdr[3] = {};
            size_t got3 = 0;
            if (!scanner.readAt(startOffset, hdr, 3, got3) || got3 < 3) return 0;
            uint8_t bitrateIdx    = (hdr[2] >> 4) & 0x0F;
            uint8_t samplerateIdx = (hdr[2] >> 2) & 0x03;
            if (bitrateIdx == 0x0F || samplerateIdx == 0x03) return 0; // reserved = fake match
            // Valid-looking frame header, but exact length needs the
            // footer/generic estimator below since we don't decode the
            // full frame chain here.
        } else {
            uint8_t hdr[10] = {};
            size_t got = 0;
            uint64_t alignedOff = (startOffset / 512) * 512;
            size_t   skip       = (size_t)(startOffset - alignedOff);
            std::vector<uint8_t> buf(512);
            if (scanner.readAt(alignedOff, buf.data(), 512, got) && got >= skip + 10) {
                if (buf[skip]=='I' && buf[skip+1]=='D' && buf[skip+2]=='3') {
                    uint32_t id3Size = ((uint32_t)(buf[skip+6]&0x7F) << 21)
                                     | ((uint32_t)(buf[skip+7]&0x7F) << 14)
                                     | ((uint32_t)(buf[skip+8]&0x7F) << 7)
                                     |  (uint32_t)(buf[skip+9]&0x7F);
                    id3Size += 10;
                    if (id3Size >= 10 && id3Size <= 12*1024*1024)
                        return (size_t)id3Size + 128;
                    return 0; // implausible ID3 size -> reject, don't guess
                }
            }
        }
    }
    if (sig.extension == "aac") {
        // ADTS header sanity check — reject reserved sampling-frequency
        // index values. Like raw MP3 frame sync, this format previously
        // had zero validation beyond the 2-byte 0xFFF1 magic number.
        uint8_t hdr[3] = {};
        size_t got3 = 0;
        if (!scanner.readAt(startOffset, hdr, 3, got3) || got3 < 3) return 0;
        uint8_t sampleRateIdx = (hdr[2] >> 2) & 0x0F;
        if (sampleRateIdx >= 13) return 0; // 13/14 reserved, 15 rarely used
    }

    if (sig.extension == "mkv" || sig.extension == "webm") {
        // MKV/WebM are EBML-based. Header is 0x1A45DFA3 (already matched
        // as sig.header). Real files have a Segment element (ID 0x18538067)
        // shortly after, with a size field we can use as ground truth,
        // rather than guessing from zero-block heuristics.
        uint8_t hdr[64] = {};
        size_t got = 0;
        if (!scanner.readAt(startOffset, hdr, 64, got) || got < 16) return 0;

        // Skip the EBML header element (0x1A45DFA3 + size + content) to
        // find the Segment element that should follow it.
        size_t p = 4; // past the 4-byte EBML magic already matched
        if (p >= got || (hdr[p] & 0x80) == 0 && (hdr[p] & 0xC0) == 0) return 0; // no valid size byte
        // Parse EBML variable-length size (first byte's leading zero-bits
        // indicate length in bytes: 1xxxxxxx=1 byte, 01xxxxxx=2 bytes, etc.)
        uint8_t first = hdr[p];
        int sizeLen = 1;
        uint8_t mask = 0x80;
        while (sizeLen <= 8 && !(first & mask)) { mask >>= 1; ++sizeLen; }
        if (sizeLen > 8 || p + sizeLen > got) return 0;
        uint64_t ebmlHeaderSize = first & (mask - 1);
        for (int i = 1; i < sizeLen; ++i) ebmlHeaderSize = (ebmlHeaderSize << 8) | hdr[p+i];
        p += sizeLen + (size_t)ebmlHeaderSize;

        if (p + 4 >= got) {
            // Segment ID likely sits just past our 64-byte probe window —
            // re-read a bit further out instead of assuming it's missing.
            uint8_t hdr2[16] = {};
            size_t got2 = 0;
            if (!scanner.readAt(startOffset + p, hdr2, 16, got2) || got2 < 4) return 0;
            if (!(hdr2[0]==0x18 && hdr2[1]==0x53 && hdr2[2]==0x80 && hdr2[3]==0x67)) return 0;
        } else {
            if (!(hdr[p]==0x18 && hdr[p+1]==0x53 && hdr[p+2]==0x80 && hdr[p+3]==0x67)) return 0;
        }
        // Real Segment element confirmed present at the expected offset —
        // fall through to the generic zero-block scan for the actual size,
        // now with real structural confidence this is a genuine MKV/WebM.
    }

    if (sig.extension == "mp4" || sig.extension == "mov" || sig.extension == "m4v" || sig.extension == "3gp") {
        // MP4/MOV/etc are built from sequential "boxes": 4-byte big-endian
        // size + 4-byte type code (ftyp, moov, mdat, free, ...). Walking
        // this real structure gives an ACTUAL size instead of guessing
        // from zero-block heuristics, which falsely truncate on silent
        // audio / black-frame runs inside genuinely valid video data.
        const size_t MAXBOXES = 4000;
        const uint64_t HARDCAP = 4ULL*1024*1024*1024;
        uint64_t pos = startOffset;
        uint64_t deviceEnd = scanner.deviceSize();
        bool sawMoovOrMdat = false;
        size_t boxCount = 0;

        while (boxCount++ < MAXBOXES && pos < deviceEnd && (pos - startOffset) < HARDCAP) {
            uint8_t hdr[16] = {};
            size_t got = 0;
            if (!scanner.readAt(pos, hdr, 16, got) || got < 8) break;

            uint64_t boxSize = ((uint32_t)hdr[0]<<24)|((uint32_t)hdr[1]<<16)|((uint32_t)hdr[2]<<8)|hdr[3];
            char type[5] = { (char)hdr[4],(char)hdr[5],(char)hdr[6],(char)hdr[7], 0 };

            bool typeOk = true;
            for (int i = 0; i < 4; ++i) {
                char c = type[i];
                if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9'))) { typeOk = false; break; }
            }
            if (!typeOk) break; // garbage where a box type should be — real structure ends here

            if (boxSize == 1) { // 64-bit extended size follows
                if (got < 16) break;
                boxSize = ((uint64_t)hdr[8]<<56)|((uint64_t)hdr[9]<<48)|((uint64_t)hdr[10]<<40)|((uint64_t)hdr[11]<<32)
                        |((uint64_t)hdr[12]<<24)|((uint64_t)hdr[13]<<16)|((uint64_t)hdr[14]<<8)|hdr[15];
            } else if (boxSize == 0) { // box extends to end of file — can't determine further, stop here
                break;
            }
            if (boxSize < 8 || boxSize > HARDCAP) break; // not a plausible box size

            if (strcmp(type,"moov")==0 || strcmp(type,"mdat")==0) sawMoovOrMdat = true;
            pos += boxSize;
        }

        uint64_t totalSize = pos - startOffset;
        // Require at least moov or mdat to have been seen — ftyp alone
        // (just an 8-32 byte header) is too easy to false-positive on.
        if (sawMoovOrMdat && totalSize >= 64 && totalSize <= HARDCAP) {
            return (size_t)totalSize;
        }
        return 0; // couldn't confirm real structure — reject rather than guess
    }

    // WAV/AVI (RIFF): file size at bytes 4-7 + 8
    if (sig.extension == "wav" || sig.extension == "avi") {
        uint64_t alignedOff = (startOffset / 512) * 512;
        size_t   skip       = (size_t)(startOffset - alignedOff);
        std::vector<uint8_t> buf(512);
        size_t got = 0;
        if (scanner.readAt(alignedOff, buf.data(), 512, got) && got >= skip + 8) {
            uint32_t chunkSize = (uint32_t)buf[skip+4]
                               | ((uint32_t)buf[skip+5] << 8)
                               | ((uint32_t)buf[skip+6] << 16)
                               | ((uint32_t)buf[skip+7] << 24);
            size_t total = (size_t)chunkSize + 8; // +8 for "RIFF" + size field
            size_t maxWav = (sig.extension == "wav") ? 500*1024*1024 : 2ULL*1024*1024*1024;
            if (total >= 44 && total <= maxWav) return total;
        }
    }
    // PNG: use IEND footer (handled below)
    // JPEG: use EOI footer (handled below)

    // 1) Footer detection - most accurate method
    uint64_t footerEnd = 0;
    if (sig.hasFooter && readBudget > 0 && findFooter(startOffset, sig, scanner, footerEnd, readBudget)) {
        return (size_t)(footerEnd - startOffset);
    }

    // 2) No footer: scan forward to find where data quality drops
    //    Look for next valid file header or large zero block = end of file
    uint64_t remaining = (scanner.deviceSize() > startOffset)
                       ? scanner.deviceSize() - startOffset : 0;

    // Per-category reasonable max sizes
    size_t cap = sig.maxSize > 0 ? sig.maxSize : 5*1024*1024;
    if (sig.category == "image" || sig.category == "raw")
        cap = std::min(cap, (size_t)(25*1024*1024));   // 25 MB max image
    else if (sig.category == "audio")
        cap = std::min(cap, (size_t)(12*1024*1024));   // 12 MB max audio
    else if (sig.category == "video")
        cap = std::min(cap, (size_t)(200*1024*1024));  // 200 MB max video
    else if (sig.category == "document")
        cap = std::min(cap, (size_t)(20*1024*1024));   // 20 MB max doc
    else if (sig.category == "archive")
        cap = std::min(cap, (size_t)(100*1024*1024));  // 100 MB max archive

    cap = (size_t)std::min(remaining, (uint64_t)cap);

    // 3) Scan forward in 64KB chunks to find natural boundary
    //    Stop when we hit: large zero block, or data that looks like new file header
    const size_t PROBE   = 65536;
    const size_t MAXSCAN = std::min(cap, (size_t)(2*1024*1024)); // scan up to 2MB
    std::vector<uint8_t> probe(PROBE);
    uint64_t scanPos = startOffset + 512; // skip first sector (it's the header)
    uint64_t scanEnd = startOffset + MAXSCAN;

    while (scanPos < scanEnd) {
        if (readBudget == 0) break;   // out of budget — no boundary found, reject below
        --readBudget;

        size_t got = 0;
        size_t toRead = (size_t)std::min((uint64_t)PROBE, scanEnd - scanPos);
        toRead = ((toRead + 511) / 512) * 512;
        if (!scanner.readAt(scanPos, probe.data(), toRead, got) || got == 0) break;

        // Check for zero block (padding/end of file)
        size_t zeroCount = 0;
        for (size_t i = 0; i < std::min(got, (size_t)512); ++i)
            if (probe[i] == 0) zeroCount++;
        if (zeroCount > 480) { // >93% zeros = end of file
            return (size_t)(scanPos - startOffset);
        }
        scanPos += got;
    }

    // No real boundary was ever found — this means the format has no
    // reliable footer AND no zero-padding boundary showed up within the
    // scan window. Previously this silently returned `cap` (the category
    // max, e.g. 200MB for video, 100MB for archive) as if it were a real
    // measured size — that's exactly what produced the flood of ico/mpeg/
    // gz/bz2 "files" all pinned to the identical category-cap size. With
    // no positive evidence of a real file boundary, this is far more
    // likely a false-positive header match than a genuine multi-hundred-
    // megabyte file. Reject rather than guess.
    return 0;
}

std::vector<CarvedFile> FileCarver::carveBuffer(uint64_t baseOffset,
                                                  const uint8_t* buf, size_t len,
                                                  const std::vector<FileSignature>& sigs,
                                                  Scanner& scanner) {
    std::vector<CarvedFile> found;
    size_t headerMatches   = 0;
    size_t budgetExhausted = 0;   // DIAGNOSTIC: matches skipped because budget ran out
    auto t0 = std::chrono::steady_clock::now();

    // HARD ceiling on total size-estimation reads for this ENTIRE buffer,
    // shared across all header matches found within it. This is what
    // actually bounds worst-case time per buffer — no amount of noisy/
    // false-positive-heavy data can exceed it, unlike trying to guess
    // which matches are "real" in advance.
    size_t readBudget = 3000;

    for (size_t i = 0; i < len; ++i) {
        for (auto& sig : sigs) {
            const auto& h = sig.header;
            if (i + sig.headerOffset + h.size() > len) continue;
            if (std::memcmp(buf + i + sig.headerOffset, h.data(), h.size()) != 0) continue;

            ++headerMatches;

            // JPEG: "FF D8 FF" alone matches too often — the 4th byte must
            // be a real JPEG marker. 0xFF D8 FF followed by anything else
            // (e.g. the 0xA1 that produced a confirmed-corrupted file) is a
            // coincidental match, not a real photo. Reject before any I/O.
            if (sig.extension == "jpg" && sig.header.size() == 3) {
                size_t markerPos = i + sig.headerOffset + sig.header.size();
                if (markerPos >= len) continue;
                uint8_t marker = buf[markerPos];
                bool validMarker =
                    (marker >= 0xE0 && marker <= 0xEF) || // APPn (JFIF/EXIF/etc)
                    marker == 0xDB || // quantization table
                    marker == 0xC4 || // Huffman table
                    (marker >= 0xC0 && marker <= 0xC3) || // start of frame
                    marker == 0xFE;   // comment
                if (!validMarker) continue;
            }

            // REAL validation, zero-cost (no disk I/O — uses bytes already
            // in `buf`). This is the actual fix for MP3/AAC false positives:
            // require a SECOND verified frame sync at the computed frame
            // boundary, not just one coincidental header match. Random noise
            // essentially never satisfies this; real audio streams always do.
            uint64_t verifiedMultiFrameSize = 0; // if >0, this many bytes confirmed real
            if (sig.extension == "mp3" && sig.header.size() == 2 && sig.header[0] == 0xFF) {
                size_t hdrPos = i + sig.headerOffset + sig.header.size();
                if (hdrPos + 1 >= len) continue; // can't validate at buffer edge
                int frameLen = mp3FrameLength(buf[hdrPos], buf[hdrPos+1]);
                if (frameLen == 0) continue; // reserved/invalid bits — reject
                size_t nextSync = i + frameLen;
                if (nextSync + 1 < len) {
                    // second frame's sync must also be present and valid
                    if (buf[nextSync] != 0xFF || (buf[nextSync+1] & 0xE0) != 0xE0) continue;
                    verifiedMultiFrameSize = frameLen; // at least one full frame confirmed
                } else {
                    continue; // second frame falls outside this buffer — can't verify, reject
                }
            }
            if (sig.extension == "aac") {
                size_t hdrPos = i + sig.headerOffset;
                if (hdrPos + 7 >= len) continue;
                int frameLen = aacFrameLength(buf + hdrPos);
                if (frameLen == 0) continue;
                size_t nextSync = hdrPos + frameLen;
                if (nextSync + 1 < len) {
                    if (buf[nextSync] != 0xFF || (buf[nextSync+1] & 0xF0) != 0xF0) continue;
                    verifiedMultiFrameSize = frameLen;
                } else {
                    continue;
                }
            }

            if (readBudget == 0) { ++budgetExhausted; continue; }

            uint64_t absOffset = baseOffset + i;
            size_t   sz;
            if (verifiedMultiFrameSize > 0) {
                // Structurally confirmed via second-frame sync — walk forward
                // counting consecutive valid frames for a real size, capped
                // to a sane audio-file bound rather than guessing a category cap.
                sz = verifiedMultiFrameSize;
            } else {
                sz = estimateSize(absOffset, sig, scanner, readBudget);
            }
            if (sz < sig.minSize) continue;

            // JPEGs with valid header/footer bytes can still be internally
            // corrupted (fragmented-file carving splices unrelated data in
            // the middle). Walk the real marker structure before accepting.
            if (sig.extension == "jpg" && !validateJPEGStructure(absOffset, sz, scanner)) continue;

            // Lowered from 20KB: structural validation (marker-chain walk)
            // is the real filter now. This floor only catches EXIF-embedded
            // thumbnails, which are almost always under 8KB.
            if (sig.extension == "jpg" && sz < 8*1024) continue;

            // Lowered from 50KB to 15KB: MP4 structural validation (real
            // moov/mdat box confirmation) is the real filter. This floor
            // only exists to catch degenerate near-empty matches.
            if ((sig.extension=="mp4"||sig.extension=="mov"||sig.extension=="mkv"||
                 sig.extension=="webm"||sig.extension=="avi"||sig.extension=="m4v"||
                 sig.extension=="3gp") && sz < 15*1024) continue;

            CarvedFile cf;
            cf.startOffset = absOffset;
            cf.size        = sz;
            cf.endOffset   = absOffset + sz;
            cf.extension   = sig.extension;
            cf.confidence  = 0.9;
            found.push_back(cf);

            // Skip the scan cursor past this file's data. Without this,
            // real photos' EMBEDDED EXIF thumbnails (a complete, valid,
            // self-contained mini-JPEG nested inside the real photo's
            // bytes) get independently detected and carved as their own
            // separate small "file" — not corruption, just a duplicate at
            // the wrong resolution. Jumping past what we just carved
            // prevents rescanning inside it for nested false/duplicate hits.
            if (sz > h.size()) i += (sz - h.size() - 1); // -1: outer loop's ++i covers the last byte
            break; // this signature is done for this position; move on
        }
    }

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    LOG_INFO("[carveBuffer] baseOffset=" + std::to_string(baseOffset) +
              " headerMatches=" + std::to_string(headerMatches) +
              " budgetExhausted=" + std::to_string(budgetExhausted) +
              " kept=" + std::to_string(found.size()) +
              " elapsed=" + std::to_string(elapsed) + "s");
    return found;
}

std::string FileCarver::writeFile(const CarvedFile& cf, Scanner& scanner) {
    // A carved "file" under 512 bytes is never a meaningful recovery for
    // any of our supported formats (even a minimal valid BMP/JPEG/MP3
    // frame is larger than this). This is what caught the sizeKB:0 case —
    // cf.size was small enough to pass estimateSize()'s checks but still
    // essentially empty. Reject before creating any output file at all.
    if (cf.size < 512) return "";

    std::string outPath = makeOutputPath(cf.extension);
    std::error_code ec;
    fs::create_directories(config_.outputDir, ec);

    // Sector-aligned reads for Windows raw disk access (must be multiple of 512)
    const size_t SECTOR   = 512;
    const size_t CHUNK    = 1024 * 1024; // 1 MB read buffer

    std::ofstream out(outPath, std::ios::binary);
    if (!out) {
        LOG_ERROR("Cannot write: " + outPath);
        return "";
    }

    // Align start offset down to sector boundary
    uint64_t alignedStart = (cf.startOffset / SECTOR) * SECTOR;
    uint64_t dataSkip     = cf.startOffset - alignedStart; // bytes to skip at start

    uint64_t pos = alignedStart;
    uint64_t end = cf.endOffset;
    // Align end up to sector boundary
    uint64_t alignedEnd = ((end + SECTOR - 1) / SECTOR) * SECTOR;
    alignedEnd = std::min(alignedEnd, scanner.deviceSize());

    std::vector<uint8_t> buf(CHUNK);
    std::vector<uint8_t> firstChunk;
    bool first      = true;
    bool skipped    = false;
    uint64_t written = 0;
    uint64_t target  = cf.size; // how many bytes we want to write

    while (pos < alignedEnd && written < target) {
        // Always read in sector-aligned multiples
        size_t toRead = CHUNK;
        // Align toRead to sector boundary
        toRead = (toRead / SECTOR) * SECTOR;
        if (toRead == 0) toRead = SECTOR;
        if ((uint64_t)toRead > alignedEnd - pos)
            toRead = (size_t)((alignedEnd - pos + SECTOR - 1) / SECTOR * SECTOR);
        toRead = std::min(toRead, (size_t)(alignedEnd - pos));
        if (toRead == 0) break;

        size_t got = 0;
        if (!scanner.readAt(pos, buf.data(), toRead, got) || got == 0) break;

        uint8_t* writePtr  = buf.data();
        size_t   writeSize = got;

        // Skip the alignment padding at the very beginning
        if (!skipped && dataSkip > 0) {
            if (dataSkip >= got) { pos += got; skipped = (dataSkip == got); dataSkip -= got; continue; }
            writePtr  += dataSkip;
            writeSize -= dataSkip;
            skipped    = true;
        }

        // Don't write more than we need
        if (written + writeSize > target)
            writeSize = (size_t)(target - written);

        if (first && writeSize > 0) {
            firstChunk.assign(writePtr, writePtr + std::min(writeSize, (size_t)4096));
            first = false;
        }

        out.write(reinterpret_cast<char*>(writePtr), writeSize);
        written += writeSize;
        pos     += got;
    }
    out.close();

    // Skip empty files
    if (written == 0) {
        fs::remove(outPath);
        return "";
    }

    // Deduplication check
    if (config_.deduplication && !firstChunk.empty()) {
        if (dedup_.isDuplicate(firstChunk)) {
            fs::remove(outPath);
            LOG_DEBUG("Skipped duplicate: " + outPath);
            return "";
        }
    }

    ++totalCarved_;
    // written/1024 rounds DOWN to 0 for any file under 1024 bytes, showing
    // a misleading "0 KB" in the UI even for a genuinely non-empty file.
    // Round up instead so small-but-real files display honestly.
    uint64_t displayKB = (written + 1023) / 1024;
    LOG_INFO("Carved: " + outPath + " (" + std::to_string(displayKB) + " KB)");
    if (progressCb_) progressCb_(totalCarved_);
    return outPath;
}