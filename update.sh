#!/usr/bin/env bash
# ==============================================================================
# FastTransfer — Linux & Arch Linux Quick Update Script
# ==============================================================================
# This script automatically:
#  1. Pulls the latest commits from git (optional --no-pull to skip)
#  2. Reconfigures and recompiles FastTransfer using Ninja across all CPU cores
#  3. Runs the automated verification test suite
#  4. Reinstalls the updated binary and desktop files system-wide to /usr/bin
#  5. Optionally terminates old instances and restarts FastTransfer (--run / -r)
# ==============================================================================

set -e

# --- Colors ---
BOLD='\033[1m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

DO_PULL=true
RUN_AFTER=false
BUILD_ARCH_PKG=false

print_banner() {
    echo -e "${CYAN}${BOLD}"
    echo "======================================================================"
    echo "            ⚡ FastTransfer — Quick Update & Rebuild ⚡              "
    echo "======================================================================"
    echo -e "${NC}"
}

print_help() {
    print_banner
    echo -e "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -r, --run          Restart/launch FastTransfer immediately after update"
    echo "  -p, --package      Rebuild and reinstall native Arch package (makepkg)"
    echo "  --no-pull          Skip 'git pull' and only rebuild from local code"
    echo "  -h, --help         Show this help message"
    echo ""
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -r|--run)
            RUN_AFTER=true
            shift
            ;;
        -p|--package)
            BUILD_ARCH_PKG=true
            shift
            ;;
        --no-pull)
            DO_PULL=false
            shift
            ;;
        -h|--help)
            print_help
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            print_help
            ;;
    esac
done

print_banner

# --- Step 1: Pull Latest Git Changes ---
if [ "$DO_PULL" = true ]; then
    echo -e "${BLUE}[1/5] Checking for latest git updates...${NC}"
    if [ -d .git ] && command -v git &> /dev/null; then
        echo -e "${CYAN}→ Pulling from git remote...${NC}"
        git pull || {
            echo -e "${YELLOW}! Git pull encountered a conflict or offline mode. Continuing with local code.${NC}"
        }
    else
        echo -e "${YELLOW}! Not a git repository or git not found. Skipping pull.${NC}"
    fi
else
    echo -e "${YELLOW}[1/5] Skipping git pull (--no-pull specified).${NC}"
fi

# --- Step 2: Handle Arch Package flow if requested ---
if [ "$BUILD_ARCH_PKG" = true ] && [ -f /etc/arch-release ]; then
    echo -e "\n${BLUE}[2/5] Updating via native Arch Linux PKGBUILD...${NC}"
    cd packaging/arch
    makepkg -sif --noconfirm
    cd ../..
    echo -e "${GREEN}✓ Native Arch package updated and installed!${NC}"
    if [ "$RUN_AFTER" = true ]; then
        pkill -x FastTransfer 2>/dev/null || true
        exec fasttransfer
    fi
    exit 0
fi

# --- Step 3: Configure CMake ---
BUILD_DIR="build-linux"
echo -e "\n${BLUE}[2/5] Configuring CMake with Ninja...${NC}"
if [ ! -d "$BUILD_DIR" ]; then
    cmake -B "$BUILD_DIR" -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -GNinja
else
    cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
fi

# --- Step 4: Recompile ---
CPU_CORES=$(nproc 2>/dev/null || echo 4)
echo -e "\n${BLUE}[3/5] Recompiling FastTransfer with ${CPU_CORES} CPU cores...${NC}"
cmake --build "$BUILD_DIR" --parallel "$CPU_CORES"
echo -e "${GREEN}✓ Compilation completed.${NC}"

# --- Step 5: Run Automated Tests ---
echo -e "\n${BLUE}[4/5] Running automated tests...${NC}"
ctest --test-dir "$BUILD_DIR" --output-on-failure
echo -e "${GREEN}✓ All test suites verified successfully.${NC}"

# --- Step 6: System Reinstallation ---
echo -e "\n${BLUE}[5/5] Updating system installation (/usr/bin)...${NC}"
if [ -w /usr/bin ]; then
    cmake --install "$BUILD_DIR"
else
    sudo cmake --install "$BUILD_DIR"
fi

# Ensure symlink exists
if [ -f /usr/bin/FastTransfer ] && [ ! -f /usr/bin/fasttransfer ]; then
    sudo ln -sf /usr/bin/FastTransfer /usr/bin/fasttransfer
fi

# Update desktop and icon databases
if command -v update-desktop-database &> /dev/null; then
    sudo update-desktop-database /usr/share/applications 2>/dev/null || true
fi
if command -v gtk-update-icon-cache &> /dev/null; then
    sudo gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
fi

echo -e "\n${GREEN}${BOLD}======================================================================${NC}"
echo -e "${GREEN}${BOLD}✓ FastTransfer has been updated successfully!                          ${NC}"
echo -e "${GREEN}${BOLD}======================================================================${NC}"

if [ "$RUN_AFTER" = true ]; then
    echo -e "\n${CYAN}Restarting FastTransfer...${NC}"
    # Stop any currently running instance
    pkill -x FastTransfer 2>/dev/null || true
    sleep 0.5
    exec /usr/bin/FastTransfer
else
    echo -e "Launch the updated version with: ${BOLD}fasttransfer${NC} (or from your app launcher)"
fi
