#include "file_signatures.h"
#include <algorithm>
#include <cstring>

FileSignatureDB& FileSignatureDB::instance() {
    static FileSignatureDB db;
    return db;
}

FileSignatureDB::FileSignatureDB() {
    // ─── IMAGES ──────────────────────────────────────────────────────────────
    add({"jpg",  "image/jpeg",  {0xFF,0xD8,0xFF},             {0xFF,0xD9}, 0,  50*1024*1024, 100, true, "image"});
    add({"png",  "image/png",   {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A}, {0x49,0x45,0x4E,0x44,0xAE,0x42,0x60,0x82}, 0, 50*1024*1024, 8, true, "image"});
    add({"gif",  "image/gif",   {0x47,0x49,0x46,0x38,0x37,0x61}, {0x00,0x3B}, 0, 10*1024*1024, 6, true, "image"});
    add({"gif",  "image/gif",   {0x47,0x49,0x46,0x38,0x39,0x61}, {0x00,0x3B}, 0, 10*1024*1024, 6, true, "image"});
    add({"bmp",  "image/bmp",   {0x42,0x4D},                  {},           0,  30*1024*1024, 2, false,"image"});
    add({"tiff", "image/tiff",  {0x49,0x49,0x2A,0x00},        {},           0, 100*1024*1024, 4, false,"image"});
    add({"tiff", "image/tiff",  {0x4D,0x4D,0x00,0x2A},        {},           0, 100*1024*1024, 4, false,"image"});
    add({"webp", "image/webp",  {0x52,0x49,0x46,0x46},        {},           0,  20*1024*1024, 4, false,"image"}); // RIFF....WEBP
    add({"ico",  "image/x-ico", {0x00,0x00,0x01,0x00},        {},           0,   1*1024*1024, 4, false,"image"});
    add({"heic", "image/heic",  {0x00,0x00,0x00,0x18,0x66,0x74,0x79,0x70},{},0,50*1024*1024,8,false,"image"});
    add({"avif", "image/avif",  {0x00,0x00,0x00,0x1C,0x66,0x74,0x79,0x70},{},0,50*1024*1024,8,false,"image"});
    add({"psd",  "image/vnd.adobe.photoshop",{0x38,0x42,0x50,0x53},{},0,500*1024*1024,4,false,"image"});

    // ─── RAW CAMERA ──────────────────────────────────────────────────────────
    add({"cr2",  "image/x-canon-cr2",    {0x49,0x49,0x2A,0x00,0x10,0x00,0x00,0x00,0x43,0x52},{},0,50*1024*1024,10,false,"raw"});
    add({"cr3",  "image/x-canon-cr3",    {0x00,0x00,0x00,0x18,0x66,0x74,0x79,0x70,0x63,0x72},{},0,50*1024*1024,10,false,"raw"});
    add({"nef",  "image/x-nikon-nef",    {0x4D,0x4D,0x00,0x2A},                              {},0,50*1024*1024,4,false,"raw"});
    add({"arw",  "image/x-sony-arw",     {0x49,0x49,0x2A,0x00},                              {},0,50*1024*1024,4,false,"raw"});
    add({"raf",  "image/x-fujifilm-raf", {0x46,0x55,0x4A,0x49,0x46,0x49,0x4C,0x4D,0x43,0x43},{},0,50*1024*1024,10,false,"raw"});
    add({"dng",  "image/x-adobe-dng",    {0x49,0x49,0x2A,0x00},                              {},0,50*1024*1024,4,false,"raw"});
    add({"orf",  "image/x-olympus-orf",  {0x49,0x49,0x52,0x4F},                              {},0,20*1024*1024,4,false,"raw"});
    add({"rw2",  "image/x-panasonic-rw2",{0x49,0x49,0x55,0x00},                              {},0,20*1024*1024,4,false,"raw"});
    add({"mrw",  "image/x-minolta-mrw",  {0x00,0x4D,0x52,0x4D},                              {},0,20*1024*1024,4,false,"raw"});

    // ─── VIDEO ───────────────────────────────────────────────────────────────
    add({"mp4",  "video/mp4",   {0x00,0x00,0x00,0x18,0x66,0x74,0x79,0x70},{},0,4ULL*1024*1024*1024,8,false,"video"});
    add({"mp4",  "video/mp4",   {0x00,0x00,0x00,0x20,0x66,0x74,0x79,0x70},{},0,4ULL*1024*1024*1024,8,false,"video"});
    add({"mp4",  "video/mp4",   {0x00,0x00,0x00,0x1C,0x66,0x74,0x79,0x70},{},0,4ULL*1024*1024*1024,8,false,"video"});
    add({"mov",  "video/quicktime",{0x00,0x00,0x00,0x14,0x66,0x74,0x79,0x70,0x71,0x74},{},0,4ULL*1024*1024*1024,10,false,"video"});
    add({"avi",  "video/avi",   {0x52,0x49,0x46,0x46},                   {},  0,2ULL*1024*1024*1024,4,false,"video"});
    add({"mkv",  "video/x-matroska",{0x1A,0x45,0xDF,0xA3},               {},  0,4ULL*1024*1024*1024,4,false,"video"});
    add({"flv",  "video/x-flv", {0x46,0x4C,0x56,0x01},                   {},  0,2ULL*1024*1024*1024,4,false,"video"});
    add({"wmv",  "video/x-ms-wmv",{0x30,0x26,0xB2,0x75,0x8E,0x66,0xCF,0x11},{},0,2ULL*1024*1024*1024,8,false,"video"});
    add({"mpeg", "video/mpeg",  {0x00,0x00,0x01,0xBA},                    {},  0,2ULL*1024*1024*1024,4,false,"video"});
    add({"mpeg", "video/mpeg",  {0x00,0x00,0x01,0xB3},                    {},  0,2ULL*1024*1024*1024,4,false,"video"});
    add({"webm", "video/webm",  {0x1A,0x45,0xDF,0xA3},                    {},  0,2ULL*1024*1024*1024,4,false,"video"});
    add({"mxf",  "application/mxf",{0x06,0x0E,0x2B,0x34,0x02,0x05,0x01,0x01},{},0,10ULL*1024*1024*1024,8,false,"video"});
    add({"3gp",  "video/3gpp",  {0x00,0x00,0x00,0x14,0x66,0x74,0x79,0x70,0x33},{},0,500*1024*1024,9,false,"video"});
    add({"m4v",  "video/x-m4v", {0x00,0x00,0x00,0x18,0x66,0x74,0x79,0x70,0x4D},{},0,4ULL*1024*1024*1024,9,false,"video"});

    // ─── AUDIO ───────────────────────────────────────────────────────────────
    add({"mp3",  "audio/mpeg",  {0xFF,0xFB},                  {},           0, 100*1024*1024, 2, false,"audio"});
    add({"mp3",  "audio/mpeg",  {0xFF,0xF3},                  {},           0, 100*1024*1024, 2, false,"audio"});
    add({"mp3",  "audio/mpeg",  {0x49,0x44,0x33},             {},           0, 100*1024*1024, 3, false,"audio"}); // ID3
    add({"wav",  "audio/wav",   {0x52,0x49,0x46,0x46},        {},           0, 500*1024*1024, 4, false,"audio"});
    add({"flac", "audio/flac",  {0x66,0x4C,0x61,0x43},        {},           0, 500*1024*1024, 4, false,"audio"});
    add({"m4a",  "audio/m4a",   {0x00,0x00,0x00,0x20,0x66,0x74,0x79,0x70,0x4D,0x34},{},0,200*1024*1024,10,false,"audio"});
    add({"ogg",  "audio/ogg",   {0x4F,0x67,0x67,0x53},        {},           0, 100*1024*1024, 4, false,"audio"});
    add({"aac",  "audio/aac",   {0xFF,0xF1},                  {},           0, 15*1024*1024, 2, false,"audio"});
    add({"wma",  "audio/x-ms-wma",{0x30,0x26,0xB2,0x75,0x8E,0x66,0xCF,0x11},{},0,100*1024*1024,8,false,"audio"});

    // ─── DOCUMENTS ───────────────────────────────────────────────────────────
    add({"pdf",  "application/pdf",    {0x25,0x50,0x44,0x46},            {0x25,0x25,0x45,0x4F,0x46},0,500*1024*1024,4,true,"document"});
    add({"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document",{0x50,0x4B,0x03,0x04},{},0,100*1024*1024,4,false,"document"});
    add({"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",{0x50,0x4B,0x03,0x04},{},0,100*1024*1024,4,false,"document"});
    add({"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation",{0x50,0x4B,0x03,0x04},{},0,100*1024*1024,4,false,"document"});
    add({"odt",  "application/vnd.oasis.opendocument.text",{0x50,0x4B,0x03,0x04},{},0,50*1024*1024,4,false,"document"});
    add({"rtf",  "application/rtf",   {0x7B,0x5C,0x72,0x74,0x66},       {},            0,50*1024*1024,5,false,"document"});
    add({"epub", "application/epub+zip",{0x50,0x4B,0x03,0x04},           {},            0,100*1024*1024,4,false,"document"});

    // ─── ARCHIVES ────────────────────────────────────────────────────────────
    add({"zip",  "application/zip",   {0x50,0x4B,0x03,0x04},            {0x50,0x4B,0x05,0x06},0,4ULL*1024*1024*1024,4,true,"archive"});
    add({"rar",  "application/x-rar", {0x52,0x61,0x72,0x21,0x1A,0x07}, {},              0,4ULL*1024*1024*1024,6,false,"archive"});
    add({"7z",   "application/x-7z-compressed",{0x37,0x7A,0xBC,0xAF,0x27,0x1C},{},0,4ULL*1024*1024*1024,6,false,"archive"});
    // Extended from 2 to 3 bytes: byte[2]=0x08 is the compression method
    // (deflate), present in ~99.9% of real gzip files. Cuts the random
    // false-positive rate from 1-in-65,536 to 1-in-16.7M per byte offset.
    add({"gz",   "application/gzip",  {0x1F,0x8B,0x08},                  {},              0,4ULL*1024*1024*1024,3,false,"archive"});
    add({"bz2",  "application/x-bzip2",{0x42,0x5A,0x68},                {},              0,4ULL*1024*1024*1024,3,false,"archive"});

    // ─── DATABASES ───────────────────────────────────────────────────────────
    add({"sqlite","application/x-sqlite3",{0x53,0x51,0x4C,0x69,0x74,0x65,0x20,0x66,0x6F,0x72,0x6D,0x61,0x74,0x20,0x33,0x00},{},0,4ULL*1024*1024*1024,16,false,"database"});
}

std::vector<FileSignature> FileSignatureDB::forTypes(const std::vector<std::string>& types) const {
    if (types.empty()) return sigs_;
    std::vector<FileSignature> out;
    for (auto& s : sigs_) {
        for (auto& t : types) {
            if (s.extension == t || s.category == t) { out.push_back(s); break; }
        }
    }
    return out;
}

bool FileSignatureDB::matchHeader(const uint8_t* buf, size_t len, const FileSignature& sig) const {
    const auto& h = sig.header;
    if (len < sig.headerOffset + h.size()) return false;
    return std::memcmp(buf + sig.headerOffset, h.data(), h.size()) == 0;
}

bool FileSignatureDB::matchFooter(const uint8_t* buf, size_t len, const FileSignature& sig) const {
    if (!sig.hasFooter || sig.footer.empty()) return false;
    const auto& f = sig.footer;
    if (len < f.size()) return false;
    return std::memcmp(buf + len - f.size(), f.data(), f.size()) == 0;
}