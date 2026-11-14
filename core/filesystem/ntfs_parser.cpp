#include "ntfs_parser.h"
#include "utils/logger.h"
#include <cstring>
#include <algorithm>
#include <filesystem>

NTFSParser::NTFSParser() {}

bool NTFSParser::parse(Scanner& scanner, uint64_t partitionOffset) {
    // Read boot sector
    uint8_t boot[512];
    size_t got = 0;
    if (!scanner.readAt(partitionOffset, boot, 512, got) || got < 512) return false;
    if (std::memcmp(boot+3, "NTFS    ", 8) != 0) return false;

    uint16_t bytesPerSector    = boot[11] | (boot[12] << 8);
    uint8_t  sectorsPerCluster = boot[13];
    uint32_t clusterSize       = bytesPerSector * sectorsPerCluster;

    // MFT LCN at offset 48 (8 bytes)
    uint64_t mftLCN = 0;
    for (int i = 0; i < 8; ++i)
        mftLCN |= ((uint64_t)boot[48+i] << (8*i));
    uint64_t mftOffset = partitionOffset + mftLCN * clusterSize;

    LOG_INFO("NTFS: MFT at LCN=" + std::to_string(mftLCN) +
             " offset=" + std::to_string(mftOffset));
    return parseMFT(scanner, mftOffset, clusterSize, partitionOffset);
}

bool NTFSParser::parseMFT(Scanner& scanner, uint64_t mftOffset, uint32_t clusterSize,
                           uint64_t partitionOffset) {
    const size_t MFT_ENTRY_SIZE = 1024;
    std::vector<uint8_t> buf(MFT_ENTRY_SIZE);
    uint64_t pos   = mftOffset;
    uint64_t limit = scanner.deviceSize();
    int      count = 0;

    while (pos + MFT_ENTRY_SIZE <= limit && count < 500000) {
        size_t got = 0;
        if (!scanner.readAt(pos, buf.data(), MFT_ENTRY_SIZE, got) || got < MFT_ENTRY_SIZE) break;
        // MFT signature "FILE"
        if (buf[0]=='F' && buf[1]=='I' && buf[2]=='L' && buf[3]=='E') {
            MFTEntry entry;
            if (parseMFTEntry(buf.data(), MFT_ENTRY_SIZE, entry, partitionOffset, clusterSize)) {
                entry.mftRef = (pos - mftOffset) / MFT_ENTRY_SIZE;
                all_.push_back(entry);
                if (entry.isDeleted) deleted_.push_back(entry);
            }
        }
        pos += MFT_ENTRY_SIZE;
        ++count;
    }
    LOG_INFO("NTFS: parsed " + std::to_string(all_.size()) +
             " entries, " + std::to_string(deleted_.size()) + " deleted");
    return true;
}

bool NTFSParser::parseMFTEntry(const uint8_t* buf, size_t len, MFTEntry& entry,
                                uint64_t partitionOffset, uint32_t clusterSize) {
    if (len < 48) return false;
    // Flags at offset 22: 0=deleted file, 1=file, 2=deleted dir, 3=dir
    uint16_t flags = buf[22] | (buf[23] << 8);
    entry.isDeleted   = !(flags & 1);
    entry.isDirectory = (flags & 2) != 0;

    // First attribute offset
    uint16_t attrOff = buf[20] | (buf[21] << 8);
    size_t   pos     = attrOff;

    while (pos + 8 < len) {
        uint32_t attrType = buf[pos] | (buf[pos+1]<<8) | (buf[pos+2]<<16) | (buf[pos+3]<<24);
        uint32_t attrLen  = buf[pos+4] | (buf[pos+5]<<8) | (buf[pos+6]<<16) | (buf[pos+7]<<24);
        if (attrType == 0xFFFFFFFF || attrLen == 0 || pos + attrLen > len) break;

        // 0x30 = $FILE_NAME attribute
        if (attrType == 0x30 && pos + 24 < len) {
            uint16_t nameLen    = buf[pos+88]; // in characters
            uint16_t nameOffset = buf[pos+90] | (buf[pos+91]<<8);
            if (pos + nameOffset + nameLen*2 <= len && nameLen > 0) {
                // UTF-16LE name
                std::string name;
                for (int i = 0; i < nameLen; ++i)
                    name += (char)buf[pos + nameOffset + i*2];
                entry.name = name;
                entry.extension = getExtension(name);
            }
        }
        // 0x80 = $DATA attribute — get size AND on-disk location
        if (attrType == 0x80 && pos + 16 < len) {
            uint8_t nonResident = buf[pos+8];
            if (!nonResident) {
                // Resident: value length at pos+16
                entry.size = buf[pos+16] | (buf[pos+17]<<8) |
                             (buf[pos+18]<<16) | ((uint64_t)buf[pos+19]<<24);
                entry.dataOffset = 0; // resident data lives inside the MFT entry itself
            } else {
                // Non-resident: allocated size at pos+40, real size at pos+48
                entry.size = 0;
                if (pos + 56 <= len)
                    for (int i = 0; i < 8; ++i)
                        entry.size |= ((uint64_t)buf[pos+48+i] << (8*i));
                entry.dataRuns = extractDataRuns(buf + pos, attrLen, partitionOffset, clusterSize);
                entry.dataOffset = (!entry.dataRuns.empty() && entry.dataRuns[0].offset != UINT64_MAX)
                                    ? entry.dataRuns[0].offset : 0;
            }
        }
        pos += attrLen;
    }
    return !entry.name.empty();
}

std::string NTFSParser::getExtension(const std::string& name) {
    auto dot = name.rfind('.');
    if (dot == std::string::npos) return "";
    std::string ext = name.substr(dot+1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

std::vector<NTFSDataRun> NTFSParser::extractDataRuns(const uint8_t* attr, size_t attrLen,
                                        uint64_t partOff, uint32_t clusterSize) {
    std::vector<NTFSDataRun> runs;
    if (attrLen < 34) return runs;
    if (!attr[8]) return runs;                     // resident — no run list
    uint16_t runOff = attr[32] | (attr[33] << 8);
    if (runOff == 0 || runOff >= attrLen) return runs;

    size_t  pos = runOff;
    int64_t lcn = 0;

    while (pos < attrLen && attr[pos] != 0x00) {
        uint8_t header  = attr[pos];
        int lenSize = header & 0x0F, offSize = (header >> 4) & 0x0F;
        pos++;
        if (lenSize == 0 || pos + lenSize + offSize > attrLen) break;

        uint64_t runLength = 0;                     // run length, in clusters
        for (int i = 0; i < lenSize; ++i)
            runLength |= ((uint64_t)attr[pos+i]) << (8*i);
        pos += lenSize;

        if (offSize == 0) {
            // Sparse run — zero-filled hole, no disk offset
            runs.push_back({UINT64_MAX, runLength * clusterSize});
            continue;
        }

        int64_t runOffset = 0;
        for (int i = 0; i < offSize; ++i)
            runOffset |= ((int64_t)attr[pos+i]) << (8*i);
        if (attr[pos+offSize-1] & 0x80)             // sign-extend negative offsets
            for (int i = offSize; i < 8; ++i) runOffset |= ((int64_t)0xFF) << (8*i);
        pos += offSize;

        lcn += runOffset;
        runs.push_back({partOff + (uint64_t)lcn * clusterSize, runLength * clusterSize});
    }
    return runs;
}
