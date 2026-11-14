# NeuroRecover v2.0 — Quick Start Guide

Get recovering files in under 5 minutes.

---

## Step 1: Build

### Linux / macOS
```bash
cd NeuroRecover
chmod +x build.sh
./build.sh Release
```

### Windows
```bat
build.bat Release
```

### Manual (if CMake not installed)
```bash
mkdir build
g++ -std=c++17 -O2 -pthread -I. \
  cli/main.cpp \
  utils/logger.cpp utils/progress.cpp utils/hash.cpp \
  utils/dedup.cpp utils/config.cpp utils/thread_pool.cpp \
  core/scanner/scanner.cpp core/scanner/deep_scanner.cpp \
  core/carving/file_signatures.cpp core/carving/carver.cpp \
  core/carving/fragment_stitcher.cpp \
  core/ai/ai_engine.cpp \
  core/filesystem/fs_analyzer.cpp core/filesystem/ntfs_parser.cpp \
  core/filesystem/fat_parser.cpp core/filesystem/ext_parser.cpp \
  core/reconstruct/jpeg_repair.cpp core/reconstruct/png_repair.cpp \
  core/reconstruct/video_repair.cpp core/reconstruct/reconstructor.cpp \
  core/metadata/metadata_extractor.cpp \
  core/recovery_engine.cpp \
  -o build/neurorecover -lpthread -lstdc++fs
```

---

## Step 2: Find Your Device

### Linux
```bash
lsblk
# or
fdisk -l
# Look for your USB/SD card e.g. /dev/sdb or /dev/mmcblk0
```

### macOS
```bash
diskutil list
# Look for your device e.g. /dev/disk2
```

### Windows
Open **Disk Management** or run:
```bat
wmic diskdrive list brief
# Device will be \\.\PhysicalDrive1 etc.
```

> ⚠️  **Important**: Use the whole-disk path (e.g. `/dev/sdb`) not a partition (e.g. `/dev/sdb1`).  
> Recovery works best on the raw device.

---

## Step 3: Run Recovery

### Recover Everything (recommended first try)
```bash
sudo ./build/neurorecover /dev/sdb ./recovered -t all -d -f -r
```

### Recover Only Photos
```bash
sudo ./build/neurorecover /dev/sdb ./photos -t jpg,png,gif,bmp,tiff -f -a
```

### Recover Only Videos
```bash
sudo ./build/neurorecover /dev/sdb ./videos -t mp4,mov,avi,mkv -d -f
```

### Recover RAW Camera Files
```bash
sudo ./build/neurorecover /dev/sdb ./raw -t cr2,cr3,nef,arw,raf,dng -d -f -a
```

### Fast Preview (quick check what's there)
```bash
sudo ./build/neurorecover /dev/sdb ./preview --fast
```

### Maximum Recovery (slow but thorough)
```bash
sudo ./build/neurorecover /dev/sdb ./recovered -t all -d -f -r -a -j 8
```

---

## Step 4: Check Results

```
recovered/
├── recovered_000001.jpg
├── recovered_000002.jpg
├── recovered_000003.mp4
├── recovered_000004.png
├── ...
├── recovery_report.txt     ← summary of everything found
└── recovery.log            ← detailed log
```

Open `recovery_report.txt` to see:
- Total files recovered by type
- File sizes and disk offsets
- Which files were repaired
- Confidence scores

---

## Common Scenarios

### "I accidentally deleted photos from my SD card"
```bash
# Find the card
lsblk
# Recover photos
sudo ./build/neurorecover /dev/mmcblk0 ./sdcard_recovery -t jpg,png,cr2,nef -d -f -a -r
```

### "My USB drive was formatted"
```bash
sudo ./build/neurorecover /dev/sdb ./usb_recovery -t all -d -f -r
```

### "I need to recover files from a disk image"
```bash
# No sudo needed for image files
./build/neurorecover /path/to/disk.img ./recovered -t all -d -f -r
```

### "I want to recover files from years ago"
```bash
# Deep scan with repair gives best results for old deleted files
sudo ./build/neurorecover /dev/sdb ./old_recovery -t all -d -f -r -j 8
```

### "Recovery is too slow"
```bash
# Use fast mode and more threads
sudo ./build/neurorecover /dev/sdb ./preview --fast -j 8
```

---

## Tips

- **Always run as root** on Linux/macOS for device access
- **Don't write to the source drive** while recovering — it overwrites deleted data
- **Use deep scan** (`-d`) for files deleted a long time ago
- **Use repair** (`-f`) for files that were partially overwritten
- **Use metadata** (`-a`) if you need to sort recovered photos by date
- **More threads** (`-j 8`) speeds up multi-core machines significantly
- **Disk images**: Create one first with `dd if=/dev/sdb of=disk.img bs=4M` for safe recovery

---

## Run Unit Tests

```bash
./build/test_runner
```

All 37 tests should pass with output like:
```
  [PASS] sha256_known_abc
  [PASS] sig_jpeg_match
  [PASS] ai_classify_jpeg
  ...
  Passed: 37 / 37
```
