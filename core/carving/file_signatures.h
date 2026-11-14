#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

struct FileSignature {
    std::string  extension;
    std::string  mimeType;
    std::vector<uint8_t> header;       // Magic bytes
    std::vector<uint8_t> footer;       // End marker (optional)
    size_t       headerOffset = 0;     // Where in file the header starts
    size_t       maxSize      = 0;     // 0 = unlimited
    size_t       minSize      = 0;
    bool         hasFooter    = false;
    std::string  category;             // image / video / audio / document / archive
};

class FileSignatureDB {
public:
    static FileSignatureDB& instance();
    const std::vector<FileSignature>& all() const { return sigs_; }
    std::vector<FileSignature> forTypes(const std::vector<std::string>& types) const;
    bool matchHeader(const uint8_t* buf, size_t len, const FileSignature& sig) const;
    bool matchFooter(const uint8_t* buf, size_t len, const FileSignature& sig) const;

private:
    FileSignatureDB();
    std::vector<FileSignature> sigs_;
    void add(const FileSignature& s) { sigs_.push_back(s); }
};
