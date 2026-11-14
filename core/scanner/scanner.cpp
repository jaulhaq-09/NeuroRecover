#include "scanner.h"
#include "utils/logger.h"
#include <cstring>
#include <stdexcept>
#include <sys/stat.h>
#ifdef _WIN32
  #include <malloc.h>   // _aligned_malloc / _aligned_free
#endif

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <winioctl.h>
#else
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/ioctl.h>
  #ifdef __linux__
    #include <linux/fs.h>
  #endif
  #ifdef __APPLE__
    #include <sys/disk.h>
  #endif
#endif

std::atomic<uint64_t> Scanner::g_readCount{0};

Scanner::Scanner(const RecoveryConfig& config) : config_(config) {}

Scanner::~Scanner() { close(); }

bool Scanner::open() {
#ifdef _WIN32
    HANDLE h = CreateFileA(config_.devicePath.c_str(),
        GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        // Fallback: try opening without FILE_FLAG_NO_BUFFERING (e.g. for image files or filesystems not supporting unbuffered I/O)
        h = CreateFileA(config_.devicePath.c_str(),
            GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        LOG_ERROR("Cannot open device: " + config_.devicePath +
                   " (Win32 error code=" + std::to_string(err) + ")");
        return false;
    }
    fileHandle_ = h;

    // Query the real physical sector size — required to correctly align
    // FILE_FLAG_NO_BUFFERING reads. Hardcoding 512 breaks on 4Kn drives,
    // and more importantly the alignment logic below needs this value.
    DISK_GEOMETRY_EX geo;
    DWORD geoBytes = 0;
    if (DeviceIoControl((HANDLE)fileHandle_, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
            nullptr, 0, &geo, sizeof(geo), &geoBytes, nullptr) &&
        geo.Geometry.BytesPerSector > 0) {
        sectorSize_ = geo.Geometry.BytesPerSector;
    }
    LOG_INFO("Sector size: " + std::to_string(sectorSize_) + " bytes");

    deviceSize_ = detectSize();
#else
    fd_ = ::open(config_.devicePath.c_str(), O_RDONLY | O_LARGEFILE);
    if (fd_ < 0) {
        LOG_ERROR("Cannot open device: " + config_.devicePath + " (run as root?)");
        return false;
    }
    deviceSize_ = detectSize();
#endif
    LOG_INFO("Opened: " + config_.devicePath + " size=" + std::to_string(deviceSize_) + " bytes");
    return true;
}

void Scanner::close() {
#ifdef _WIN32
    if (fileHandle_) { CloseHandle((HANDLE)fileHandle_); fileHandle_ = nullptr; }
#else
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
#endif
}

uint64_t Scanner::detectSize() {
#ifdef _WIN32
    LARGE_INTEGER sz;
    if (GetFileSizeEx((HANDLE)fileHandle_, &sz)) return (uint64_t)sz.QuadPart;
    DISK_GEOMETRY_EX geo;
    DWORD bytes;
    if (DeviceIoControl((HANDLE)fileHandle_, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
            nullptr, 0, &geo, sizeof(geo), &bytes, nullptr))
        return (uint64_t)geo.DiskSize.QuadPart;
    return 0;
#else
    struct stat st;
    if (fstat(fd_, &st) == 0 && S_ISREG(st.st_mode))
        return (uint64_t)st.st_size;
#ifdef __linux__
    uint64_t sz = 0;
    if (ioctl(fd_, BLKGETSIZE64, &sz) == 0) return sz;
#endif
#ifdef __APPLE__
    uint64_t sz = 0;
    if (ioctl(fd_, DKIOCGETBLOCKCOUNT, &sz) == 0) {
        uint32_t bs = 512;
        ioctl(fd_, DKIOCGETBLOCKSIZE, &bs);
        return sz * bs;
    }
#endif
    return 0;
#endif
}

bool Scanner::readAt(uint64_t offset, uint8_t* buf, size_t len, size_t& bytesRead) {
    bytesRead = 0;
    if (!buf || len == 0) return true;
    uint64_t n = ++g_readCount;
    if (n % 500 == 0) LOG_INFO("[readAt] total calls so far=" + std::to_string(n));
#ifdef _WIN32
    // Windows raw disk reads with FILE_FLAG_NO_BUFFERING require THREE things:
    // 1) offset sector-aligned, 2) length sector-aligned, 3) the destination
    // BUFFER ADDRESS itself sector-aligned. std::vector gives no such guarantee
    // (~16-byte alignment only) — that silently broke every small read (boot
    // sectors, MFT entries, FAT directory entries) with ERROR_INVALID_PARAMETER.
    //
    // IMPORTANT: readAt() is called concurrently from multiple ThreadPool
    // worker threads AND the main scan loop on the SAME Scanner instance
    // (see phase2_carve). A shared buffer here would be a data race. Each
    // thread gets its own scratch buffer via thread_local — safe, and still
    // avoids repeated allocation within a given thread's lifetime.
    struct AlignedScratch {
        uint8_t* ptr = nullptr;
        size_t   cap = 0;
        ~AlignedScratch() { if (ptr) _aligned_free(ptr); }
    };
    thread_local AlignedScratch scratch;

    const size_t SECTOR = sectorSize_ ? sectorSize_ : 512;
    uint64_t alignedOffset = (offset / SECTOR) * SECTOR;
    size_t   prefix        = (size_t)(offset - alignedOffset);
    size_t   alignedLen    = ((prefix + len + SECTOR - 1) / SECTOR) * SECTOR;

    if (alignedLen > scratch.cap) {
        if (scratch.ptr) _aligned_free(scratch.ptr);
        scratch.ptr = (uint8_t*)_aligned_malloc(alignedLen, SECTOR);
        scratch.cap = scratch.ptr ? alignedLen : 0;
        if (!scratch.ptr) {
            LOG_ERROR("readAt: aligned allocation failed for " + std::to_string(alignedLen) + " bytes");
            return false;
        }
    }

    // CRITICAL: readAt() is called concurrently from 16 ThreadPool worker
    // threads (via carveBuffer -> estimateSize -> findFooter) all sharing
    // ONE HANDLE. SetFilePointerEx-then-ReadFile is TWO separate steps —
    // under concurrency, Thread A's seek can be overwritten by Thread B's
    // seek before Thread A's ReadFile executes, causing every thread to
    // read from the wrong offset. This silently corrupted carving results
    // and caused effective livelock (searches never finding what they're
    // looking for because they're reading the wrong bytes, while 16 threads
    // simultaneously thrash the same drive's seek position).
    //
    // Fix: pass the offset directly via OVERLAPPED — this makes each read
    // atomic and position-independent, safe for concurrent multi-thread use
    // on a single handle, even without FILE_FLAG_OVERLAPPED.
    OVERLAPPED ov = {};
    ov.Offset     = (DWORD)(alignedOffset & 0xFFFFFFFF);
    ov.OffsetHigh = (DWORD)(alignedOffset >> 32);
    DWORD read = 0;
    if (!ReadFile((HANDLE)fileHandle_, scratch.ptr, (DWORD)alignedLen, &read, &ov)) {
        LOG_ERROR("readAt: ReadFile failed offset=" + std::to_string(alignedOffset) +
                   " len=" + std::to_string(alignedLen) +
                   " (err=" + std::to_string(GetLastError()) + ")");
        return false;
    }
    size_t available = (read > prefix) ? (read - prefix) : 0;
    bytesRead = std::min(available, len);
    if (bytesRead > 0)
        std::memcpy(buf, scratch.ptr + prefix, bytesRead);
#else
    ssize_t r = pread(fd_, buf, len, (off_t)offset);
    if (r < 0) return false;
    bytesRead = (size_t)r;
#endif
    return bytesRead > 0;
}

bool Scanner::readBlock(uint64_t blockIndex, uint8_t* buf, size_t& bytesRead) {
    return readAt(blockIndex * config_.blockSize, buf, config_.blockSize, bytesRead);
}

bool Scanner::scan(ReadCallback cb) {
    if (!isOpen()) return false;
    uint64_t start = config_.startOffset;
    uint64_t end   = (config_.endOffset > 0) ? config_.endOffset : deviceSize_;
    size_t   bufSz = config_.readBuffer;
    std::vector<uint8_t> buf(bufSz);

    uint64_t pos = start;
    while (pos < end) {
        size_t toRead = std::min((uint64_t)bufSz, end - pos);
        size_t got    = 0;
        if (!readAt(pos, buf.data(), toRead, got) || got == 0) break;
        cb(pos, buf.data(), got);
        pos += got;
    }
    return true;
}
