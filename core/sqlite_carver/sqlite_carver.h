#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct SQLiteRecord {
    uint64_t    pageOffset;
    uint32_t    pageNumber;
    std::string tableName;
    std::string rawData;    // JSON representation of row data
    bool        isDeleted;
};

struct SQLiteCarveResult {
    std::string              dbPath;
    size_t                   totalPages   = 0;
    size_t                   deletedPages = 0;
    std::vector<SQLiteRecord> records;
    bool                     success      = false;
};

// Deep SQLite carver - recovers deleted rows from SQLite databases
// Works on WhatsApp, SMS, contacts, browser history databases
class SQLiteCarver {
public:
    SQLiteCarver();

    // Carve a SQLite file for deleted records
    SQLiteCarveResult carve(const std::string& dbPath);

    // Write recovered records to output
    bool writeRecords(const SQLiteCarveResult& result,
                      const std::string& outputPath);

    // Identify what app the database belongs to
    static std::string identifyDatabase(const std::string& dbPath);

private:
    static const uint32_t PAGE_SIZE_DEFAULT = 4096;
    static const uint8_t  SQLITE_MAGIC[16];

    bool isValidSQLitePage(const uint8_t* buf, size_t len) const;
    std::vector<SQLiteRecord> parseLeafPage(const uint8_t* page,
                                             size_t pageSize,
                                             uint32_t pageNum) const;
    std::string extractPayload(const uint8_t* page, size_t pageSize,
                                size_t offset, size_t payloadLen) const;
    uint64_t    decodeVarint(const uint8_t* buf, size_t len,
                              size_t& bytesRead) const;
    std::string payloadToJSON(const uint8_t* payload, size_t len) const;
};
