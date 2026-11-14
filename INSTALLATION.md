# NeuroRecover v2.0 — Installation Guide

---

## Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| Compiler  | GCC 7 / Clang 5 / MSVC 2017 | GCC 12+ / Clang 14+ |
| CMake     | 3.16 | 3.25+ |
| C++ Std   | C++17 | C++17 |
| RAM       | 512 MB | 4 GB+ |
| OS        | Linux / macOS / Windows 7+ | Ubuntu 22.04 / macOS 13 |

No external libraries required. Everything is built-in.

---

## Linux

### Ubuntu / Debian
```bash
sudo apt update
sudo apt install -y build-essential cmake g++
cd NeuroRecover
./build.sh Release
sudo make install          # optional: installs to /usr/local/bin
```

### Fedora / RHEL / CentOS
```bash
sudo dnf install -y gcc-c++ cmake make
cd NeuroRecover
./build.sh Release
```

### Arch Linux
```bash
sudo pacman -S base-devel cmake
cd NeuroRecover
./build.sh Release
```

### Alpine Linux
```bash
apk add build-base cmake
cd NeuroRecover
./build.sh Release
```

### Verify Installation
```bash
./build/neurorecover --help
./build/test_runner
```

---

## macOS

### Using Homebrew
```bash
brew install cmake
xcode-select --install      # install Clang via Xcode CLI tools
cd NeuroRecover
./build.sh Release
```

### Verify
```bash
./build/neurorecover --help
```

### macOS Notes
- macOS 12+ ships with Apple Silicon (arm64) support automatically
- For Intel Macs: same build process applies
- Device paths: `/dev/disk2`, `/dev/disk3`, etc.
- Use `diskutil list` to find your device

---

## Windows

### Using Visual Studio 2019/2022
1. Install Visual Studio with "Desktop development with C++" workload
2. Install CMake from https://cmake.org/download/
3. Open Developer Command Prompt
4. Run:
```bat
cd NeuroRecover
build.bat Release
```
Binary: `build\Release\neurorecover.exe`

### Using MinGW (MSYS2)
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
cd NeuroRecover
./build.sh Release
```

### Windows Device Paths
```
\\.\PhysicalDrive0    # First physical disk
\\.\PhysicalDrive1    # Second physical disk (e.g. USB)
\\.\E:                # Drive letter
```

```bat
neurorecover.exe \\.\PhysicalDrive1 C:\recovered -t all -d -f -r
```

> ⚠️  Run Command Prompt as Administrator for device access.

---

## Docker

### Build Container
```bash
cd NeuroRecover
docker build -t neurorecover:latest .
```

### Run Recovery
```bash
# Linux host
docker run -it --privileged \
  -v /dev:/dev \
  -v $(pwd)/recovered:/output \
  neurorecover:latest /dev/sdb /output -t all -d -f -r

# With specific types
docker run -it --privileged \
  -v /dev:/dev \
  -v $(pwd)/photos:/output \
  neurorecover:latest /dev/sdc /output -t jpg,png,mp4 -f -a -r

# From disk image
docker run -it \
  -v $(pwd)/disk.img:/disk.img:ro \
  -v $(pwd)/recovered:/output \
  neurorecover:latest /disk.img /output -t all -d -f -r
```

---

## Building with CMake (Manual)

```bash
cd NeuroRecover
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..
```

### CMake Options
```bash
# Debug build with symbols
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Specify compiler
cmake .. -DCMAKE_CXX_COMPILER=clang++

# Custom install prefix
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/neurorecover
```

---

## Direct g++ Build (No CMake)

If CMake is not available, build directly:

```bash
mkdir -p build
g++ -std=c++17 -O2 -Wall -pthread -I. \
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

For the test runner:
```bash
g++ -std=c++17 -O2 -Wall -pthread -I. \
  tests/test_runner.cpp \
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
  -o build/test_runner -lpthread -lstdc++fs
```

---

## Uninstall

```bash
# If installed via make install
sudo rm /usr/local/bin/neurorecover

# Remove build artifacts
make clean
# or
rm -rf build/
```

---

## Troubleshooting

### "Permission denied" on device
```bash
sudo ./build/neurorecover /dev/sdb ./output ...
# or add yourself to the disk group:
sudo usermod -aG disk $USER
```

### "filesystem not found" on macOS
macOS requires `-lstdc++fs` only on older versions. Try:
```bash
g++ -std=c++17 ... -lc++
```

### "stdc++fs not found" on older Linux
On GCC < 9, link explicitly:
```bash
g++ ... -lstdc++fs
# or upgrade: sudo apt install g++-9
```

### Windows "Access Denied"
Run Command Prompt or PowerShell as Administrator.

### Large device takes too long
Use `--fast` mode and limit scope with `-t jpg,png` instead of all types.
Use more threads: `-j 8` or `-j 16`.
