#!/usr/bin/env bash
# ==============================================================================
# FastTransfer — Arch Linux Native Build & Installation Script
# ==============================================================================

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}====================================================${NC}"
echo -e "${BLUE}      FastTransfer — Arch Linux Build Script        ${NC}"
echo -e "${BLUE}====================================================${NC}"

# Check for Arch Linux environment
if [ -f /etc/arch-release ]; then
    echo -e "${GREEN}✓ Arch Linux detected.${NC}"
else
    echo -e "${YELLOW}! Non-Arch system detected. Running generic Linux build.${NC}"
fi

# Check required dependencies
MISSING_PKGS=()
for cmd in cmake ninja g++; do
    if ! command -v $cmd &> /dev/null; then
        MISSING_PKGS+=($cmd)
    fi
done

# Check Qt 6
if ! pkg-config --exists Qt6Core Qt6Widgets Qt6Network 2>/dev/null; then
    if [ ! -d /usr/include/qt6 ] && [ ! -d /usr/lib/qt6 ]; then
        MISSING_PKGS+=("qt6-base")
    fi
fi

if [ ${#MISSING_PKGS[@]} -gt 0 ]; then
    echo -e "${YELLOW}Missing build tools/libraries: ${MISSING_PKGS[*]}${NC}"
    if command -v pacman &> /dev/null; then
        echo -e "${BLUE}Installing required packages via pacman...${NC}"
        sudo pacman -S --needed --noconfirm base-devel cmake ninja qt6-base
    else
        echo -e "${RED}Please install missing packages before continuing.${NC}"
        exit 1
    fi
fi

BUILD_DIR="build-linux"
echo -e "\n${BLUE}Configuring FastTransfer with CMake...${NC}"
cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -GNinja

echo -e "\n${BLUE}Compiling FastTransfer...${NC}"
cmake --build "$BUILD_DIR" --parallel

echo -e "\n${BLUE}Running automated test suite...${NC}"
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo -e "\n${GREEN}====================================================${NC}"
echo -e "${GREEN}✓ Build and tests completed successfully!           ${NC}"
echo -e "${GREEN}====================================================${NC}"
echo -e "You can run FastTransfer immediately:"
echo -e "  ${YELLOW}./$BUILD_DIR/FastTransfer${NC}"
echo -e "\nOr install system-wide to /usr/bin:"
echo -e "  ${YELLOW}sudo cmake --install $BUILD_DIR${NC}"
echo -e "\nOr create a native Arch Linux package (.pkg.tar.zst):"
echo -e "  ${YELLOW}cd packaging/arch && makepkg -si${NC}"
