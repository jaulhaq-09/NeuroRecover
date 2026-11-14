#include "encryption_detector.h"
#include "utils/logger.h"
#include <cstring>
#include <cmath>
#include <array>

EncryptionDetector::EncryptionDetector() {}

double EncryptionDetector::calcEntropy(const uint8_t* buf, size_t len) const {
    if (!len) return 0.0;
    std::array<uint64_t,256> freq{};
    for (size_t i=0;i<len;++i) freq[buf[i]]++;
    double h=0.0;
    for (auto c:freq){if(!c)continue;double p=(double)c/len;h-=p*std::log2(p);}
    return h;
}

bool EncryptionDetector::detectBitLocker(const uint8_t* buf, size_t len,
                                          EncryptionInfo& info) {
    // BitLocker signature: "-FVE-FS-" at offset 3
    if (len < 11) return false;
    if (std::memcmp(buf+3, "-FVE-FS-", 8) == 0) {
        info.type        = EncryptionType::BITLOCKER;
        info.name        = "BitLocker";
        info.isEncrypted = true;
        info.description = "Windows BitLocker Drive Encryption detected";
        return true;
    }
    // Also check NTFS with BitLocker metadata
    if (len >= 512) {
        // BitLocker NTFS: OEM ID "NTFS    " but with FVE metadata
        if (std::memcmp(buf+3, "NTFS    ", 8)==0) {
            // Check for FVE metadata cluster reference
            if (buf[160]!=0 || buf[168]!=0) {
                // Heuristic: check entropy of data area
                double e = calcEntropy(buf+512, std::min(len-512,(size_t)4096));
                if (e > 7.8) {
                    info.type        = EncryptionType::BITLOCKER;
                    info.name        = "BitLocker (NTFS)";
                    info.isEncrypted = true;
                    info.entropy     = e;
                    info.description = "NTFS volume with BitLocker encryption (high entropy data)";
                    return true;
                }
            }
        }
    }
    return false;
}

bool EncryptionDetector::detectVeraCrypt(const uint8_t* buf, size_t len,
                                          EncryptionInfo& info) {
    // VeraCrypt volumes start with 512 bytes of random-looking data (no magic)
    // Detection: first 512 bytes have entropy > 7.9 AND no filesystem signature
    if (len < 512) return false;
    double e = calcEntropy(buf, 512);
    if (e > 7.95) {
        // No known filesystem or encryption signature — likely VeraCrypt/TrueCrypt
        bool hasKnownSig = (std::memcmp(buf+3,"NTFS    ",8)==0) ||
                           (buf[510]==0x55&&buf[511]==0xAA) ||
                           (std::memcmp(buf+1024+56,"\x53\xEF",2)==0); // ext
        if (!hasKnownSig) {
            info.type        = EncryptionType::VERACRYPT;
            info.name        = "VeraCrypt / TrueCrypt";
            info.isEncrypted = true;
            info.entropy     = e;
            info.description = "High-entropy volume with no filesystem signature — likely VeraCrypt";
            return true;
        }
    }
    return false;
}

bool EncryptionDetector::detectLUKS(const uint8_t* buf, size_t len,
                                     EncryptionInfo& info) {
    // LUKS magic: "LUKS\xBA\xBE" at offset 0
    if (len < 6) return false;
    if (buf[0]=='L'&&buf[1]=='U'&&buf[2]=='K'&&buf[3]=='S'&&
        buf[4]==0xBA&&buf[5]==0xBE) {
        info.type        = EncryptionType::LUKS;
        info.name        = "LUKS";
        info.isEncrypted = true;
        info.description = "Linux Unified Key Setup (LUKS) encrypted volume";
        uint16_t ver = (buf[6]<<8)|buf[7];
        info.description += " v" + std::to_string(ver);
        return true;
    }
    return false;
}

bool EncryptionDetector::detectAPFSEncrypted(const uint8_t* buf, size_t len,
                                              EncryptionInfo& info) {
    // APFS NX Superblock at offset 32, magic "NXSB"
    if (len < 40) return false;
    if (buf[32]=='N'&&buf[33]=='X'&&buf[34]=='S'&&buf[35]=='B') {
        // Check APFS encryption flag at offset 96 (flags field)
        // bit 0x4 = encrypted
        if (len > 100) {
            uint64_t flags = 0;
            for (int i=0;i<8;i++) flags|=((uint64_t)buf[96+i]<<(8*i));
            if (flags & 0x4) {
                info.type        = EncryptionType::APFS_ENCRYPTED;
                info.name        = "FileVault 2 / APFS Encrypted";
                info.isEncrypted = true;
                info.description = "Apple APFS encrypted volume (FileVault 2)";
                return true;
            }
        }
    }
    return false;
}

bool EncryptionDetector::isEncryptedRegion(const uint8_t* buf, size_t len) const {
    return calcEntropy(buf, len) > 7.8;
}

EncryptionInfo EncryptionDetector::detect(const uint8_t* buf, size_t len,
                                           uint64_t offset) {
    EncryptionInfo info;
    info.offset = offset;

    if (detectLUKS(buf, len, info))          { LOG_INFO("Encryption: " + info.name); return info; }
    if (detectBitLocker(buf, len, info))      { LOG_INFO("Encryption: " + info.name); return info; }
    if (detectAPFSEncrypted(buf, len, info))  { LOG_INFO("Encryption: " + info.name); return info; }
    if (detectVeraCrypt(buf, len, info))      { LOG_INFO("Encryption: " + info.name); return info; }

    info.entropy = calcEntropy(buf, std::min(len,(size_t)4096));
    return info;
}

std::string EncryptionDetector::typeName(EncryptionType t) {
    switch(t) {
        case EncryptionType::BITLOCKER:       return "BitLocker";
        case EncryptionType::VERACRYPT:       return "VeraCrypt/TrueCrypt";
        case EncryptionType::LUKS:            return "LUKS";
        case EncryptionType::APFS_ENCRYPTED:  return "FileVault 2";
        case EncryptionType::FILEVAULT:       return "FileVault";
        case EncryptionType::UNKNOWN_ENCRYPTED:return "Unknown Encryption";
        default:                              return "None";
    }
}

std::string EncryptionDetector::recoveryAdvice(EncryptionType t) {
    switch(t) {
        case EncryptionType::BITLOCKER:
            return "BitLocker detected. You need the recovery key (48-digit key or .bek file). "
                   "Unlock the drive in Windows first, then run NeuroRecover on the unlocked volume.";
        case EncryptionType::VERACRYPT:
            return "VeraCrypt volume detected. Mount it with VeraCrypt using your password, "
                   "then run NeuroRecover on the mounted drive letter.";
        case EncryptionType::LUKS:
            return "LUKS volume detected. Decrypt with: cryptsetup luksOpen /dev/sdX myvolume, "
                   "then run NeuroRecover on /dev/mapper/myvolume.";
        case EncryptionType::APFS_ENCRYPTED:
            return "FileVault 2 detected. Boot into macOS and unlock with your Apple ID or "
                   "recovery key. Then run NeuroRecover on the unlocked volume.";
        default:
            return "Encrypted volume: Obtain the decryption key/password and unlock before recovery.";
    }
}
