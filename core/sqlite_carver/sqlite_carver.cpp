#include "sqlite_carver.h"
#include "utils/logger.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

const uint8_t SQLiteCarver::SQLITE_MAGIC[16] = {
    0x53,0x51,0x4C,0x69,0x74,0x65,0x20,0x66,
    0x6F,0x72,0x6D,0x61,0x74,0x20,0x33,0x00
};

SQLiteCarver::SQLiteCarver() {}

bool SQLiteCarver::isValidSQLitePage(const uint8_t* buf, size_t len) const {
    if (len < 8) return false;
    // B-tree page types: 0x02,0x05,0x0A,0x0D
    uint8_t t = buf[0];
    return (t==0x02||t==0x05||t==0x0A||t==0x0D);
}

uint64_t SQLiteCarver::decodeVarint(const uint8_t* buf, size_t len,
                                     size_t& bytesRead) const {
    uint64_t val = 0;
    bytesRead = 0;
    for (size_t i = 0; i < 9 && i < len; ++i) {
        val = (val << 7) | (buf[i] & 0x7F);
        bytesRead++;
        if (!(buf[i] & 0x80)) break;
    }
    return val;
}

std::string SQLiteCarver::payloadToJSON(const uint8_t* payload, size_t len) const {
    // Simplified: dump raw bytes as hex values
    std::ostringstream j;
    j << "{\"hex\":\"";
    for (size_t i = 0; i < std::min(len,(size_t)256); ++i)
        j << std::hex << std::setw(2) << std::setfill('0') << (int)payload[i];
    j << "\",\"len\":" << std::dec << len << "}";
    return j.str();
}

std::vector<SQLiteRecord> SQLiteCarver::parseLeafPage(const uint8_t* page,
                                                        size_t pageSize,
                                                        uint32_t pageNum) const {
    std::vector<SQLiteRecord> records;
    if (!isValidSQLitePage(page, pageSize)) return records;

    uint8_t pageType = page[0];
    bool isLeaf = (pageType == 0x0D || pageType == 0x0A);
    if (!isLeaf) return records;

    // Number of cells at offset 3
    uint16_t numCells = (page[3]<<8)|page[4];
    // Cell pointer array starts at offset 8 (or 12 for interior pages)
    size_t cellPtrStart = 8;
    if (numCells == 0 || numCells > 1000) return records;

    for (uint16_t i = 0; i < numCells && cellPtrStart + i*2 + 2 <= pageSize; ++i) {
        uint16_t cellOffset = (page[cellPtrStart + i*2]<<8) |
                               page[cellPtrStart + i*2 + 1];
        if (cellOffset < 8 || cellOffset >= pageSize) continue;

        // Parse cell: varint(payload_len) + varint(rowid) + payload
        size_t br = 0;
        uint64_t payloadLen = decodeVarint(page+cellOffset,
                                           pageSize-cellOffset, br);
        if (br == 0 || payloadLen > pageSize) continue;
        size_t pos = cellOffset + br;

        uint64_t rowid = decodeVarint(page+pos, pageSize-pos, br);
        pos += br;

        if (pos + payloadLen > pageSize) payloadLen = pageSize - pos;
        if (payloadLen < 2) continue;

        SQLiteRecord rec;
        rec.pageOffset  = (uint64_t)pageNum * pageSize;
        rec.pageNumber  = pageNum;
        rec.rawData     = payloadToJSON(page+pos, (size_t)payloadLen);
        rec.isDeleted   = false; // Free page entries = deleted
        records.push_back(rec);
    }
    return records;
}

SQLiteCarveResult SQLiteCarver::carve(const std::string& dbPath) {
    SQLiteCarveResult result;
    result.dbPath = dbPath;

    std::ifstream f(dbPath, std::ios::binary);
    if (!f) { LOG_ERROR("SQLiteCarver: cannot open " + dbPath); return result; }

    // Verify SQLite magic
    uint8_t header[100];
    f.read((char*)header, 100);
    if (f.gcount() < 16 || std::memcmp(header, SQLITE_MAGIC, 16)!=0) {
        LOG_WARN("SQLiteCarver: not a valid SQLite file: " + dbPath);
        return result;
    }

    // Page size at offset 16 (big-endian)
    uint32_t pageSize = (header[16]<<8)|header[17];
    if (pageSize == 1) pageSize = 65536;
    if (pageSize < 512 || pageSize > 65536) pageSize = PAGE_SIZE_DEFAULT;

    size_t fileSize = fs::file_size(dbPath);
    result.totalPages = fileSize / pageSize;

    std::vector<uint8_t> page(pageSize);
    f.seekg(0, std::ios::beg);

    for (uint32_t pn = 0; pn < result.totalPages; ++pn) {
        f.seekg((std::streamoff)pn * pageSize);
        f.read((char*)page.data(), pageSize);
        if ((size_t)f.gcount() < pageSize) break;

        auto recs = parseLeafPage(page.data(), pageSize, pn);
        for (auto& r : recs) result.records.push_back(r);
    }

    // Also scan free pages (deleted data)
    // Free page list starts at header offset 32
    uint32_t freePage = ((uint32_t)header[32]<<24)|((uint32_t)header[33]<<16)|
                        ((uint32_t)header[34]<<8)|header[35];
    while (freePage > 0 && freePage <= result.totalPages) {
        f.seekg((std::streamoff)(freePage-1) * pageSize);
        f.read((char*)page.data(), pageSize);
        if ((size_t)f.gcount() < pageSize) break;
        auto recs = parseLeafPage(page.data(), pageSize, freePage-1);
        for (auto& r : recs) { r.isDeleted = true; result.records.push_back(r); }
        result.deletedPages++;
        // Next free page pointer at offset 0 of free page
        freePage = ((uint32_t)page[0]<<24)|((uint32_t)page[1]<<16)|
                   ((uint32_t)page[2]<<8)|page[3];
    }

    result.success = true;
    LOG_INFO("SQLiteCarver: " + dbPath + " pages=" + std::to_string(result.totalPages) +
             " records=" + std::to_string(result.records.size()) +
             " deleted_pages=" + std::to_string(result.deletedPages));
    return result;
}

bool SQLiteCarver::writeRecords(const SQLiteCarveResult& result,
                                 const std::string& outputPath) {
    std::ofstream f(outputPath);
    if (!f) return false;
    f << "{\n";
    f << "  \"source_db\": \"" << result.dbPath << "\",\n";
    f << "  \"total_pages\": " << result.totalPages << ",\n";
    f << "  \"deleted_pages\": " << result.deletedPages << ",\n";
    f << "  \"records\": [\n";
    for (size_t i=0; i<result.records.size(); ++i) {
        auto& r = result.records[i];
        if (i) f << ",\n";
        f << "    {\"page\":" << r.pageNumber
          << ",\"deleted\":" << (r.isDeleted?"true":"false")
          << ",\"data\":" << r.rawData << "}";
    }
    f << "\n  ]\n}\n";
    LOG_INFO("SQLite records written: " + outputPath);
    return true;
}

std::string SQLiteCarver::identifyDatabase(const std::string& dbPath) {
    std::string name = fs::path(dbPath).filename().string();
    std::transform(name.begin(),name.end(),name.begin(),::tolower);
    if (name.find("whatsapp")!=std::string::npos||name=="msgstore.db") return "WhatsApp Messages";
    if (name=="contacts2.db"||name.find("contact")!=std::string::npos) return "Contacts";
    if (name.find("sms")!=std::string::npos||name=="mmssms.db") return "SMS/MMS";
    if (name.find("history")!=std::string::npos) return "Browser History";
    if (name.find("places")!=std::string::npos) return "Firefox History";
    if (name.find("chrome")!=std::string::npos) return "Chrome Data";
    if (name=="calendar.db"||name.find("calendar")!=std::string::npos) return "Calendar";
    if (name.find("photo")!=std::string::npos) return "Photo Library";
    return "Unknown SQLite Database";
}
