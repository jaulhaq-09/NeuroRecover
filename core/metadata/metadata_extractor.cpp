#include "metadata_extractor.h"
#include "utils/logger.h"
#include "utils/hash.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

MetadataExtractor::MetadataExtractor() {}

uint16_t MetadataExtractor::readU16(const uint8_t* p, bool be) const {
    return be ? ((uint16_t)p[0]<<8|p[1]) : (uint16_t)p[0]|(uint16_t)p[1]<<8;
}
uint32_t MetadataExtractor::readU32(const uint8_t* p, bool be) const {
    return be ? ((uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3])
              : ((uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24);
}
std::string MetadataExtractor::readString(const uint8_t* data, size_t dLen,
                                           uint32_t offset, uint32_t len) const {
    if (offset + len > dLen) return "";
    return std::string(reinterpret_cast<const char*>(data+offset), len);
}
double MetadataExtractor::readRational(const uint8_t* data, size_t dLen,
                                        uint32_t offset, bool be) const {
    if (offset + 8 > dLen) return 0.0;
    uint32_t num = readU32(data+offset,   be);
    uint32_t den = readU32(data+offset+4, be);
    return (den == 0) ? 0.0 : (double)num / den;
}

bool MetadataExtractor::parseEXIF(const uint8_t* data, size_t len, FileMetadata& meta) {
    // Find Exif marker 0xFFE1
    for (size_t i = 0; i + 10 < len; ++i) {
        if (data[i] != 0xFF || data[i+1] != 0xE1) continue;
        size_t exifLen = ((uint16_t)data[i+2]<<8)|data[i+3];
        if (i + 4 + exifLen > len) break;
        // "Exif\0\0"
        if (std::memcmp(data+i+4, "Exif\0\0", 6) != 0) continue;
        const uint8_t* tiff = data + i + 10;
        size_t         tLen = exifLen - 6;
        if (tLen < 8) break;

        bool be = (tiff[0]=='M' && tiff[1]=='M');
        uint32_t ifdOffset = readU32(tiff+4, be);
        if (ifdOffset + 2 > tLen) break;
        uint16_t numEntries = readU16(tiff+ifdOffset, be);
        size_t   pos = ifdOffset + 2;

        for (uint16_t e = 0; e < numEntries && pos + 12 <= tLen; ++e, pos += 12) {
            uint16_t tag    = readU16(tiff+pos,   be);
            uint16_t type   = readU16(tiff+pos+2, be);
            uint32_t count  = readU32(tiff+pos+4, be);
            uint32_t valOff = readU32(tiff+pos+8, be);

            auto getString = [&]() -> std::string {
                if (count <= 4) return readString(tiff, tLen, pos+8, count);
                return readString(tiff, tLen, valOff, count);
            };

            switch (tag) {
                case 0x010F: meta.make  = getString(); break; // Make
                case 0x0110: meta.model = getString(); break; // Model
                case 0x0131: meta.software = getString(); break; // Software
                case 0x9003: meta.dateTimeOriginal   = getString(); break;
                case 0x9004: meta.dateTimeDigitized  = getString(); break;
                case 0xA002: meta.width  = (type==3)?readU16(tiff+pos+8,be):readU32(tiff+pos+8,be); break;
                case 0xA003: meta.height = (type==3)?readU16(tiff+pos+8,be):readU32(tiff+pos+8,be); break;
                case 0x0100: meta.width  = (type==3)?readU16(tiff+pos+8,be):readU32(tiff+pos+8,be); break;
                case 0x0101: meta.height = (type==3)?readU16(tiff+pos+8,be):readU32(tiff+pos+8,be); break;
                case 0x8825: { // GPS IFD pointer
                    uint32_t gpsOff = valOff;
                    if (gpsOff + 2 > tLen) break;
                    uint16_t gpsNum = readU16(tiff+gpsOff, be);
                    size_t   gpos   = gpsOff + 2;
                    double   lat = 0, lon = 0;
                    for (int g = 0; g < gpsNum && gpos + 12 <= tLen; ++g, gpos += 12) {
                        uint16_t gtag = readU16(tiff+gpos, be);
                        uint32_t gvo  = readU32(tiff+gpos+8, be);
                        if (gtag == 2 && gvo + 24 <= tLen) { // GPSLatitude
                            double d = readRational(tiff,tLen,gvo,  be);
                            double m = readRational(tiff,tLen,gvo+8,be);
                            double s = readRational(tiff,tLen,gvo+16,be);
                            lat = d + m/60.0 + s/3600.0;
                        }
                        if (gtag == 4 && gvo + 24 <= tLen) { // GPSLongitude
                            double d = readRational(tiff,tLen,gvo,  be);
                            double m = readRational(tiff,tLen,gvo+8,be);
                            double s = readRational(tiff,tLen,gvo+16,be);
                            lon = d + m/60.0 + s/3600.0;
                        }
                    }
                    if (lat != 0 || lon != 0) {
                        meta.gpsLat = lat; meta.gpsLon = lon; meta.hasGPS = true;
                    }
                    break;
                }
            }
        }
        return true;
    }
    return false;
}

FileMetadata MetadataExtractor::extractJPEG(const std::string& path) {
    FileMetadata meta;
    meta.filePath  = path;
    meta.extension = "jpg";
    meta.mimeType  = "image/jpeg";
    meta.fileSize  = fs::exists(path) ? fs::file_size(path) : 0;
    meta.sha256    = HashUtils::sha256File(path);

    std::ifstream f(path, std::ios::binary);
    if (!f) return meta;
    std::vector<uint8_t> data(std::min((size_t)1024*1024, meta.fileSize));
    f.read(reinterpret_cast<char*>(data.data()), data.size());
    data.resize(f.gcount());

    // Scan for SOF marker (width/height)
    for (size_t i = 0; i + 8 < data.size(); ++i) {
        if (data[i] != 0xFF) continue;
        uint8_t m = data[i+1];
        if ((m >= 0xC0 && m <= 0xC3) || (m >= 0xC5 && m <= 0xC7) ||
            (m >= 0xC9 && m <= 0xCB) || (m >= 0xCD && m <= 0xCF)) {
            if (i + 9 < data.size()) {
                meta.bitsPerSample = data[i+4];
                meta.height = (data[i+5]<<8)|data[i+6];
                meta.width  = (data[i+7]<<8)|data[i+8];
                break;
            }
        }
    }
    parseEXIF(data.data(), data.size(), meta);
    return meta;
}

FileMetadata MetadataExtractor::extractPNG(const std::string& path) {
    FileMetadata meta;
    meta.filePath  = path;
    meta.extension = "png";
    meta.mimeType  = "image/png";
    meta.fileSize  = fs::exists(path) ? fs::file_size(path) : 0;
    meta.sha256    = HashUtils::sha256File(path);

    std::ifstream f(path, std::ios::binary);
    if (!f) return meta;
    uint8_t buf[33];
    f.read(reinterpret_cast<char*>(buf), 33);
    if (f.gcount() >= 24) {
        // IHDR chunk starts at byte 8
        meta.width  = (buf[16]<<24)|(buf[17]<<16)|(buf[18]<<8)|buf[19];
        meta.height = (buf[20]<<24)|(buf[21]<<16)|(buf[22]<<8)|buf[23];
        meta.bitsPerSample = buf[24];
    }
    return meta;
}

FileMetadata MetadataExtractor::extractMP4(const std::string& path) {
    FileMetadata meta;
    meta.filePath  = path;
    meta.extension = "mp4";
    meta.mimeType  = "video/mp4";
    meta.fileSize  = fs::exists(path) ? fs::file_size(path) : 0;
    meta.sha256    = HashUtils::sha256File(path);
    // Basic MP4: scan for 'tkhd' box for width/height
    std::ifstream f(path, std::ios::binary);
    if (!f) return meta;
    std::vector<uint8_t> buf(std::min((size_t)4*1024*1024, meta.fileSize));
    f.read(reinterpret_cast<char*>(buf.data()), buf.size());
    buf.resize(f.gcount());

    for (size_t i = 0; i + 8 < buf.size(); ++i) {
        if (buf[i+4]=='t'&&buf[i+5]=='k'&&buf[i+6]=='h'&&buf[i+7]=='d') {
            if (i + 84 < buf.size()) {
                // width at offset 76, height at 80 (fixed-point 16.16)
                uint32_t w = readU32(buf.data()+i+76, true) >> 16;
                uint32_t h = readU32(buf.data()+i+80, true) >> 16;
                if (w > 0 && h > 0) { meta.width = w; meta.height = h; }
            }
            break;
        }
    }
    return meta;
}

FileMetadata MetadataExtractor::extractGeneric(const std::string& path) {
    FileMetadata meta;
    meta.filePath = path;
    meta.fileSize = fs::exists(path) ? fs::file_size(path) : 0;
    meta.sha256   = HashUtils::sha256File(path);
    auto dot = path.rfind('.');
    if (dot != std::string::npos) meta.extension = path.substr(dot+1);
    return meta;
}

FileMetadata MetadataExtractor::extract(const std::string& filePath) {
    auto dot = filePath.rfind('.');
    std::string ext = (dot != std::string::npos) ? filePath.substr(dot+1) : "";
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext=="jpg"||ext=="jpeg") return extractJPEG(filePath);
    if (ext=="png")              return extractPNG(filePath);
    if (ext=="mp4"||ext=="mov"||ext=="m4v") return extractMP4(filePath);
    return extractGeneric(filePath);
}

std::string MetadataExtractor::toJSON(const FileMetadata& m) const {
    std::ostringstream j;
    auto esc = [](const std::string& s) {
        std::string r;
        for (char c : s) { if (c=='"'||c=='\\') r+='\\'; r+=c; }
        return r;
    };
    j << "{\n"
      << "  \"file\": \""        << esc(m.filePath)  << "\",\n"
      << "  \"extension\": \""   << esc(m.extension) << "\",\n"
      << "  \"size\": "          << m.fileSize        << ",\n"
      << "  \"sha256\": \""      << esc(m.sha256)     << "\",\n"
      << "  \"mime_type\": \""   << esc(m.mimeType)   << "\",\n"
      << "  \"width\": "         << m.width           << ",\n"
      << "  \"height\": "        << m.height          << ",\n"
      << "  \"make\": \""        << esc(m.make)        << "\",\n"
      << "  \"model\": \""       << esc(m.model)       << "\",\n"
      << "  \"software\": \""    << esc(m.software)    << "\",\n"
      << "  \"date_original\": \""  << esc(m.dateTimeOriginal)  << "\",\n"
      << "  \"date_digitized\": \"" << esc(m.dateTimeDigitized) << "\",\n"
      << "  \"has_gps\": "       << (m.hasGPS?"true":"false") << ",\n"
      << "  \"gps_lat\": "       << std::fixed << std::setprecision(6) << m.gpsLat << ",\n"
      << "  \"gps_lon\": "       << std::fixed << std::setprecision(6) << m.gpsLon << "\n"
      << "}";
    return j.str();
}

bool MetadataExtractor::writeJSON(const FileMetadata& meta, const std::string& outPath) const {
    std::ofstream f(outPath);
    if (!f) return false;
    f << toJSON(meta);
    return true;
}
