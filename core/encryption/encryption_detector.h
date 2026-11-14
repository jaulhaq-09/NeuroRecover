#pragma once
#include <string>
#include <vector>
#include <cstdint>

enum class EncryptionType {
    NONE,
    BITLOCKER,
    VERACRYPT,
    LUKS,
    APFS_ENCRYPTED,
    FILEVAULT,
    UNKNOWN_ENCRYPTED
};

struct EncryptionInfo {
    EncryptionType type       = EncryptionType::NONE;
    std::string    name;
    std::string    description;
    bool           isEncrypted = false;
    uint64_t       offset      = 0;   // where signature was found
    double         entropy     = 0.0;
};

class EncryptionDetector {
public:
    EncryptionDetector();

    // Detect encryption from first N bytes of device
    EncryptionInfo detect(const uint8_t* buf, size_t len, uint64_t offset = 0);

    // Check if a buffer region looks encrypted (high entropy)
    bool isEncryptedRegion(const uint8_t* buf, size_t len) const;

    // Returns human-readable name
    static std::string typeName(EncryptionType t);

    // Returns recovery advice for detected type
    static std::string recoveryAdvice(EncryptionType t);

private:
    bool detectBitLocker(const uint8_t* buf, size_t len, EncryptionInfo& info);
    bool detectVeraCrypt(const uint8_t* buf, size_t len, EncryptionInfo& info);
    bool detectLUKS(const uint8_t* buf, size_t len, EncryptionInfo& info);
    bool detectAPFSEncrypted(const uint8_t* buf, size_t len, EncryptionInfo& info);
    double calcEntropy(const uint8_t* buf, size_t len) const;
};
