@echo off
echo Building NeuroRecover v2.0 for Windows...
mkdir build 2>nul
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release --parallel
cd ..
echo Build complete! Binary: build\Release\neurorecover.exe
pause
