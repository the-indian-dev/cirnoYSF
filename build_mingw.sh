#!/bin/bash

# YSFlight MinGW/MSYS2 Build Script
# This script builds YSFlight using MinGW-w64 GCC compiler on Windows
# Run this from MSYS2 terminal or Git Bash

set -e  # Exit on any error

echo "YSFlight MinGW/MSYS2 Build Script"
echo "================================="

# Check if we're in the right directory
if [ ! -f "src/CMakeLists.txt" ]; then
    echo "Error: Please run this script from the YSFlight root directory"
    exit 1
fi

# Check for required tools
echo "Checking for required tools..."

if ! command -v cmake &> /dev/null; then
    echo "Error: CMake not found. Please install cmake:"
    echo "  pacman -S mingw-w64-x86_64-cmake"
    exit 1
fi

if ! command -v gcc &> /dev/null; then
    echo "Error: GCC not found. Please install MinGW-w64 toolchain:"
    echo "  pacman -S mingw-w64-x86_64-toolchain"
    exit 1
fi

if ! command -v g++ &> /dev/null; then
    echo "Error: G++ not found. Please install MinGW-w64 toolchain:"
    echo "  pacman -S mingw-w64-x86_64-toolchain"
    exit 1
fi

echo "Tools check passed!"

# Set build type (default to Release)
BUILD_TYPE=${1:-Release}
echo "Build type: $BUILD_TYPE"

# Set architecture
if [ "$MSYSTEM" = "MINGW64" ]; then
    ARCH="64"
    MINGW_PREFIX="mingw-w64-x86_64"
elif [ "$MSYSTEM" = "MINGW32" ]; then
    ARCH="32"
    MINGW_PREFIX="mingw-w64-i686"
else
    echo "Warning: MSYSTEM not set. Assuming 64-bit build."
    ARCH="64"
    MINGW_PREFIX="mingw-w64-x86_64"
fi

echo "Architecture: ${ARCH}-bit"

# Check for required packages
echo "Checking for required MinGW packages..."

REQUIRED_PACKAGES=(
    "${MINGW_PREFIX}-cmake"
    "${MINGW_PREFIX}-gcc"
    "${MINGW_PREFIX}-opengl"
    "${MINGW_PREFIX}-freeglut"
    "${MINGW_PREFIX}-glew"
)

for package in "${REQUIRED_PACKAGES[@]}"; do
    if ! pacman -Qi "$package" &> /dev/null; then
        echo "Warning: Package $package not found. Install with:"
        echo "  pacman -S $package"
    fi
done

# Create build directory
BUILD_DIR="build_mingw_${ARCH}"
echo "Creating build directory: $BUILD_DIR"

if [ -d "$BUILD_DIR" ]; then
    echo "Build directory exists, cleaning..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Set compiler explicitly
export CC=gcc
export CXX=g++

echo "Configuring build with CMake..."
echo "CC=$CC"
echo "CXX=$CXX"

# Configure with CMake
cmake -G "MSYS Makefiles" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_MAKE_PROGRAM=make \
    -DCMAKE_CXX_STANDARD=11 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DMINGW=ON \
    -DCMAKE_INSTALL_PREFIX="../install_mingw_${ARCH}" \
    ../src

if [ $? -ne 0 ]; then
    echo "Error: CMake configuration failed!"
    exit 1
fi

echo "Building YSFlight..."

# Build with make
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "Error: Build failed!"
    exit 1
fi

echo ""
echo "Build completed successfully!"
echo "Executables should be in: $BUILD_DIR"
echo ""

# List built executables
echo "Built executables:"
find . -name "ysflight*.exe" -type f | head -10

echo ""
echo "To run YSFlight:"
echo "1. Copy the runtime files to the build directory"
echo "2. Run: ./ysflight${ARCH}_gl1.exe or ./ysflight${ARCH}_gl2.exe"
echo ""
echo "Note: You may need to copy required DLLs from your MinGW installation"
echo "if the executable doesn't run on systems without MinGW installed."