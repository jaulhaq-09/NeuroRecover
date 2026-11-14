# NeuroRecover v2.0

**Professional Data Recovery Tool — Production Ready**

Recover deleted files from drives, USB sticks, memory cards, and disk images.  
Supports 50+ file formats including photos, videos, documents, and archives.

---

## Features

- **4-Phase Recovery Pipeline**: Scan → Carve → Repair → Report
- **50+ File Formats**: JPEG, PNG, RAW camera, MP4, MOV, MKV, PDF, ZIP, SQLite, and more
- **AI-Powered Detection**: Entropy-based boundary detection for accurate carving
- **File System Parsers**: NTFS, FAT32/exFAT, ext2/3/4, HFS+, APFS
- **File Repair**: Automatic repair of corrupted JPEG, PNG, MP4, AVI, MKV files
- **EXIF Metadata Extraction**: Date, camera info, GPS coordinates
- **Deduplication**: FNV-64 fingerprinting skips duplicate files
- **Multi-threaded**: Work-stealing thread pool (1–32 threads)
- **Cross-Platform**: Linux, macOS, Windows, Docker
- **Zero Dependencies**: Pure C++17, no external libraries required

---

## Quick Start

```bash
# 1. Build
cd NeuroRecover
./build.sh Release

# 2. Recover everything from a drive
sudo ./build/neurorecover /dev/sdb ./recovered -t all -d -f -r

# 3. Done — files are in ./recovered/
```

---

## Installation

### Linux / macOS
```bash
./build.sh Release
sudo make install          # installs to /usr/local/bin
```

### Windows
```bat
build.bat Release
```

### Docker
```bash
docker build -t neurorecover .
docker run --privileged -v /dev:/dev -v $(pwd)/out:/output \
  neurorecover /dev/sdb /output -t all -d -f -r
```

---

## Usage

```
neurorecover [options] <device> <output_dir>
```

### File Type Selection
| Option | Description |
|--------|-------------|
| `-t all` | Recover all supported formats |
| `-t jpg,png,mp4` | Recover specific formats |

### Recovery Options
| Option | Description |
|--------|-------------|
| `-d, --deep` | Deep scan (slower, finds more) |
| `-f, --repair` | Repair corrupted files |
| `-r, --report` | Generate recovery_report.txt |
| `-a, --metadata` | Extract EXIF metadata |

### Performance Options
| Option | Description |
|--------|-------------|
| `-j N, --threads N` | Number of threads (default: 4) |
| `--fast` | Fast mode (3–4x faster, less thorough) |
| `--no-dedup` | Disable deduplication |

---

## Examples

```bash
# Recover all files from USB drive
sudo neurorecover /dev/sdc ./recovered -t all -d -f -r

# Recover only photos from SD card
sudo neurorecover /dev/mmcblk0 ./photos -t jpg,png,tiff,raw -f -a

# Recover videos with 8 threads
sudo neurorecover /dev/sdb ./videos -t mp4,mov,avi,mkv -j 8 -d -f

# Fast preview scan
neurorecover /dev/sdb ./preview --fast

# Recover from a disk image
neurorecover disk.img ./recovered -t all -d -f -r

# Docker recovery
docker run --privileged -v /dev:/dev -v $(pwd)/out:/output \
  neurorecover /dev/sdb /output -t jpg,png,mp4 -d -f -r
```

---

## Supported File Formats

### Images
`jpg` `png` `gif` `bmp` `tiff` `webp` `heic` `avif` `psd` `ico`

### RAW Camera
`cr2` `cr3` (Canon) `nef` (Nikon) `arw` (Sony) `raf` (Fujifilm)  
`dng` `orf` (Olympus) `rw2` (Panasonic) `mrw` (Minolta)

### Video
`mp4` `mov` `avi` `mkv` `flv` `wmv` `mpeg` `webm` `mxf` `3gp` `m4v`

### Audio
`mp3` `wav` `flac` `m4a` `ogg` `aac` `wma`

### Documents
`pdf` `docx` `xlsx` `pptx` `odt` `rtf` `epub`

### Archives
`zip` `rar` `7z` `gz` `bz2`

### Databases
`sqlite`

---

## How It Works

### Phase 1 — Filesystem Detection
Reads the boot sector to identify NTFS, FAT32/exFAT, ext4, HFS+, or APFS.  
Falls back to raw carving if no filesystem is detected.

### Phase 2 — File Carving
Scans every byte of the device looking for file signature magic bytes.  
Uses a sliding window to find file headers; locates footers for accurate sizing.  
AI entropy analysis improves boundary detection for compressed/encrypted data.

### Phase 3 — File Repair
For JPEG: validates markers, inserts missing SOI/EOI, removes corrupt segments.  
For PNG: validates 8-byte signature, ensures IEND chunk, recalculates CRCs.  
For MP4/AVI/MKV: validates box structure, rebuilds moov/index atoms.

### Phase 4 — Report
Generates `recovery_report.txt` with a full list of recovered files, offsets,  
sizes, types, confidence scores, and per-extension statistics.

---

## Performance

| Mode | Speed |
|------|-------|
| Fast mode | 200–500 MB/s |
| Deep scan | 50–150 MB/s |
| With repair | 20–100 MB/s |

| Threads | Speedup |
|---------|---------|
| 1 | baseline |
| 4 | 2.5–3× |
| 8 | 5–6× |
| 16 | 8–10× |

Memory usage: ~200 MB base + ~50 MB per thread.

---

## Building from Source

Requirements: GCC 7+ / Clang 5+ / MSVC 2017+, CMake 3.16+, C++17

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Run unit tests:
```bash
./build/test_runner
```

---

## License

MIT License — free for personal and commercial use.
