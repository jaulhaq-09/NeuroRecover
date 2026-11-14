#include "ext_parser.h"
#include "utils/logger.h"
#include <cstring>
#include <algorithm>

ExtParser::ExtParser() {}

bool ExtParser::parse(Scanner& scanner, uint64_t partitionOffset) {
    partOff_ = partitionOffset;
    // Read superblock at partition + 1024
    uint8_t sb[1024];
    size_t got = 0;
    if (!scanner.readAt(partitionOffset + 1024, sb, 1024, got) || got < 264) return false;
    uint16_t magic = sb[56] | (sb[57]<<8);
    if (magic != 0xEF53) return false;

    uint32_t logBs     = sb[24] | (sb[25]<<8) | (sb[26]<<16) | (sb[27]<<24);
    blockSize_         = 1024u << logBs;
    inodesPerGroup_    = sb[40] | (sb[41]<<8) | (sb[42]<<16) | (sb[43]<<24);
    inodeSize_         = sb[88] | (sb[89]<<8);
    uint32_t numGroups = sb[32] | (sb[33]<<8) | (sb[34]<<16) | (sb[35]<<24);
    if (inodeSize_ < 128) inodeSize_ = 128;

    LOG_INFO("ext4: blockSize=" + std::to_string(blockSize_) +
             " groups=" + std::to_string(numGroups));

    // Limit scan to first 256 groups for speed
    uint32_t maxGroups = std::min(numGroups, 256u);
    for (uint32_t g = 0; g < maxGroups; ++g) {
        uint64_t inodeTable = 0;
        if (parseGroupDescriptor(scanner, g, inodeTable))
            parseInodeTable(scanner, inodeTable, g * inodesPerGroup_);
    }
    LOG_INFO("ext4: found " + std::to_string(all_.size()) +
             " inodes, " + std::to_string(deleted_.size()) + " possibly deleted");
    return true;
}

bool ExtParser::parseGroupDescriptor(Scanner& scanner, uint32_t group, uint64_t& inodeTableBlock) {
    // Group descriptor table starts at block 1 (after superblock)
    uint64_t gdtOffset = partOff_ + blockSize_ + (uint64_t)group * 32;
    uint8_t  gd[32];
    size_t   got = 0;
    if (!scanner.readAt(gdtOffset, gd, 32, got) || got < 32) return false;
    inodeTableBlock = gd[8] | (gd[9]<<8) | (gd[10]<<16) | ((uint64_t)gd[11]<<24);
    return inodeTableBlock > 0;
}

bool ExtParser::parseInodeTable(Scanner& scanner, uint64_t tableBlock, uint32_t groupInodeBase) {
    uint64_t tableOffset = partOff_ + tableBlock * (uint64_t)blockSize_;
    size_t   inodeCount  = std::min(inodesPerGroup_, 256u);

    for (uint32_t i = 0; i < inodeCount; ++i) {
        uint8_t inode[256];
        size_t  got = 0;
        if (!scanner.readAt(tableOffset + (uint64_t)i * inodeSize_, inode, inodeSize_, got))
            break;
        uint16_t mode   = inode[0] | (inode[1]<<8);
        uint32_t size32 = inode[4] | (inode[5]<<8) | (inode[6]<<16) | (inode[7]<<24);
        uint32_t dtime  = inode[20]| (inode[21]<<8)| (inode[22]<<16)| (inode[23]<<24);
        if (size32 == 0 && mode == 0) continue;

        ExtEntry e;
        e.inode       = groupInodeBase + i + 1;
        e.size        = size32;
        e.isDeleted   = (dtime > 0);
        e.isDirectory = (mode & 0xF000) == 0x4000;
        e.name        = "inode_" + std::to_string(e.inode);
        // Rough disk offset: first block pointer at inode+40
        uint32_t blk0 = inode[40]|(inode[41]<<8)|(inode[42]<<16)|(inode[43]<<24);
        e.diskOffset  = partOff_ + (uint64_t)blk0 * blockSize_;
        e.extension   = "";
        all_.push_back(e);
        if (e.isDeleted && size32 > 0) deleted_.push_back(e);
    }
    return true;
}

std::string ExtParser::getExtension(const std::string& name) {
    auto dot = name.rfind('.');
    if (dot == std::string::npos) return "";
    std::string ext = name.substr(dot+1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}
