#include "fs_analyzer.h"
#include "utils/logger.h"
#include <cstring>
#include <cstdio>

FSAnalyzer::FSAnalyzer() {}

std::string FSAnalyzer::fsTypeName(FSType t) {
    switch(t) {
        case FSType::NTFS:     return "NTFS";
        case FSType::FAT32:    return "FAT32";
        case FSType::EXFAT:    return "exFAT";
        case FSType::EXT2:     return "ext2";
        case FSType::EXT3:     return "ext3";
        case FSType::EXT4:     return "ext4";
        case FSType::HFS_PLUS: return "HFS+";
        case FSType::APFS:     return "APFS";
        case FSType::RAW:      return "RAW";
        default:               return "Unknown";
    }
}

FSInfo FSAnalyzer::detect(Scanner& scanner) {
    FSInfo info;
    if (detectNTFS(scanner, info)) return info;
    if (detectFAT(scanner, info))  return info;
    if (detectExt(scanner, info))  return info;
    if (detectHFS(scanner, info))  return info;
    if (detectAPFS(scanner, info)) return info;
    info.type = FSType::RAW;
    info.name = "RAW";
    info.detected = true;
    LOG_INFO("Filesystem: RAW (no known filesystem detected, will use raw carving)");
    return info;
}

bool FSAnalyzer::detectNTFS(Scanner& scanner, FSInfo& info) {
    uint8_t buf[512];
    size_t got = 0;
    if (!scanner.readAt(0, buf, 512, got) || got < 512) return false;
    // OEM ID at offset 3: "NTFS    "
    if (std::memcmp(buf+3, "NTFS    ", 8) != 0) return false;
    info.type       = FSType::NTFS;
    info.name       = "NTFS";
    info.sectorSize = buf[11] | (buf[12] << 8);
    uint8_t spc     = buf[13]; // sectors per cluster
    info.clusterSize = info.sectorSize * spc;
    info.detected   = true;
    LOG_INFO("Filesystem: NTFS detected (cluster=" + std::to_string(info.clusterSize) + " bytes)");
    return true;
}

bool FSAnalyzer::detectFAT(Scanner& scanner, FSInfo& info) {
    uint8_t buf[512];
    size_t got = 0;
    if (!scanner.readAt(0, buf, 512, got) || got < 512) {
        LOG_INFO("[detectFAT] readAt failed or short read: got=" + std::to_string(got));
        return false;
    }
    // DIAGNOSTIC: dump raw boot sector signature bytes so we can see exactly
    // what's on disk instead of guessing why detection fails.
    char sigDump[64];
    snprintf(sigDump, sizeof(sigDump), "buf[510]=0x%02X buf[511]=0x%02X", buf[510], buf[511]);
    LOG_INFO(std::string("[detectFAT] boot sig check: ") + sigDump);

    // Boot sector signature
    if (buf[510] != 0x55 || buf[511] != 0xAA) {
        LOG_INFO("[detectFAT] REJECTED: bad boot sector signature (expected 0x55 0xAA)");
        return false;
    }
    // Check exFAT signature
    if (std::memcmp(buf+3, "EXFAT   ", 8) == 0) {
        info.type     = FSType::EXFAT;
        info.name     = "exFAT";
        info.detected = true;
        LOG_INFO("Filesystem: exFAT detected");
        return true;
    }
    // FAT12/16/32 detection — the boot sector's type LABEL string (at offset
    // 54 or 82) is informational only and unreliable; many real-world FAT32
    // drives (SD/USB formatters, cameras, phones) leave it blank or wrong.
    // The spec-correct method is to compute cluster count from the numeric
    // BPB fields, per Microsoft's FAT spec.
    uint16_t bps = buf[11] | (buf[12] << 8);
    LOG_INFO("[detectFAT] bytesPerSector=" + std::to_string(bps));
    if (bps != 512 && bps != 1024 && bps != 2048 && bps != 4096) {
        LOG_INFO("[detectFAT] REJECTED: bytesPerSector not a standard value");
        return false;
    }

    uint8_t  secPerClus   = buf[13];
    uint16_t rsvdSecCnt   = buf[14] | (buf[15] << 8);
    uint8_t  numFATs      = buf[16];
    uint16_t rootEntCnt   = buf[17] | (buf[18] << 8);
    uint16_t totSec16     = buf[19] | (buf[20] << 8);
    uint16_t fatSz16      = buf[22] | (buf[23] << 8);
    uint32_t totSec32     = buf[32] | (buf[33]<<8) | (buf[34]<<16) | (buf[35]<<24);
    uint32_t fatSz32      = buf[36] | (buf[37]<<8) | (buf[38]<<16) | (buf[39]<<24);

    LOG_INFO("[detectFAT] secPerClus=" + std::to_string(secPerClus) +
              " rsvdSecCnt=" + std::to_string(rsvdSecCnt) +
              " numFATs=" + std::to_string(numFATs) +
              " rootEntCnt=" + std::to_string(rootEntCnt) +
              " totSec16=" + std::to_string(totSec16) +
              " fatSz16=" + std::to_string(fatSz16) +
              " totSec32=" + std::to_string(totSec32) +
              " fatSz32=" + std::to_string(fatSz32));

    if (secPerClus == 0 || numFATs == 0) {
        LOG_INFO("[detectFAT] REJECTED: secPerClus or numFATs is zero");
        return false;
    }

    uint32_t fatSz  = (fatSz16 != 0) ? fatSz16 : fatSz32;
    uint32_t totSec = (totSec16 != 0) ? totSec16 : totSec32;
    if (fatSz == 0 || totSec == 0) {
        LOG_INFO("[detectFAT] REJECTED: fatSz or totSec is zero (fatSz=" +
                  std::to_string(fatSz) + " totSec=" + std::to_string(totSec) + ")");
        return false;
    }

    uint32_t rootDirSectors = ((uint32_t)rootEntCnt * 32 + bps - 1) / bps;
    uint32_t dataSec = totSec - (rsvdSecCnt + (uint32_t)numFATs * fatSz + rootDirSectors);
    uint32_t countOfClusters = dataSec / secPerClus;
    LOG_INFO("[detectFAT] rootDirSectors=" + std::to_string(rootDirSectors) +
              " dataSec=" + std::to_string(dataSec) +
              " countOfClusters=" + std::to_string(countOfClusters));

    // Any FAT variant is routed to FSType::FAT32 here since phase0 recovery
    // (fat_parser.cpp) branches on isFAT32_ internally based on fatSz16==0,
    // which already correctly distinguishes FAT32 from FAT12/16 for chain walking.
    if (countOfClusters >= 4085) {
        info.type        = FSType::FAT32;
        info.name        = (countOfClusters >= 65525) ? "FAT32" : "FAT16";
        info.sectorSize  = bps;
        info.clusterSize = bps * secPerClus;
        info.detected    = true;
        LOG_INFO("Filesystem: " + info.name + " detected (clusters=" +
                 std::to_string(countOfClusters) + ", cluster size=" +
                 std::to_string(info.clusterSize) + " bytes)");
        return true;
    }
    LOG_INFO("[detectFAT] REJECTED: countOfClusters < 4085 (too small for FAT16/32, likely FAT12)");
    return false; // FAT12 — too rare/small (floppy-era) to bother supporting
}

bool FSAnalyzer::detectExt(Scanner& scanner, FSInfo& info) {
    // ext superblock at offset 1024, magic = 0xEF53
    uint8_t buf[1024+264];
    size_t got = 0;
    if (!scanner.readAt(1024, buf, 264, got) || got < 264) return false;
    uint16_t magic = buf[56] | (buf[57] << 8); // s_magic at superblock+56
    if (magic != 0xEF53) return false;
    uint32_t feat = buf[96] | (buf[97]<<8) | (buf[98]<<16) | (buf[99]<<24); // s_feature_incompat
    if (feat & 0x40) { info.type = FSType::EXT4; info.name = "ext4"; }
    else if (feat & 0x4) { info.type = FSType::EXT3; info.name = "ext3"; }
    else { info.type = FSType::EXT2; info.name = "ext2"; }
    uint32_t logBs = buf[24] | (buf[25]<<8) | (buf[26]<<16) | (buf[27]<<24);
    info.clusterSize = 1024u << logBs;
    info.detected    = true;
    LOG_INFO("Filesystem: " + info.name + " detected (block=" + std::to_string(info.clusterSize) + " bytes)");
    return true;
}

bool FSAnalyzer::detectHFS(Scanner& scanner, FSInfo& info) {
    uint8_t buf[2];
    size_t got = 0;
    // HFS+ volume header at offset 1024, magic 0x482B or 0x4858 (HFSX)
    if (!scanner.readAt(1024, buf, 2, got) || got < 2) return false;
    if ((buf[0]==0x48 && buf[1]==0x2B) || (buf[0]==0x48 && buf[1]==0x58)) {
        info.type     = FSType::HFS_PLUS;
        info.name     = "HFS+";
        info.detected = true;
        LOG_INFO("Filesystem: HFS+ detected");
        return true;
    }
    return false;
}

bool FSAnalyzer::detectAPFS(Scanner& scanner, FSInfo& info) {
    uint8_t buf[32];
    size_t got = 0;
    if (!scanner.readAt(32, buf, 32, got) || got < 32) return false;
    // APFS magic "NXSB" at NX Superblock offset 32
    if (buf[0]=='N' && buf[1]=='X' && buf[2]=='S' && buf[3]=='B') {
        info.type     = FSType::APFS;
        info.name     = "APFS";
        info.detected = true;
        LOG_INFO("Filesystem: APFS detected");
        return true;
    }
    return false;
}
