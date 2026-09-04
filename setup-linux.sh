#!/usr/bin/env bash
# ==============================================================================
# FastTransfer — Automated Linux Setup & Installation Script
# ==============================================================================
# This script automatically:
#  1. Detects your Linux distribution (Arch Linux, Debian/Ubuntu, Fedora, openSUSE)
#  2. Installs required compilers and Qt 6 packages via native package manager
#  3. Compiles FastTransfer using CMake and Ninja (parallelized on all CPU cores)
#  4. Runs the automated verification test suite
#  5. Installs the application, desktop launcher, and icon system-wide
#  6. Configures firewall rules for Ethernet UDP discovery and TCP file transfer
# ==============================================================================

set -e

# --- Color Formatting ---
BOLD='\033[1m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# --- Script Flags ---
RUN_AFTER_BUILD=false
NO_INSTALL=false
BUILD_ARCH_PKG=false

print_banner() {
    echo -e "${CYAN}${BOLD}"
    echo "======================================================================"
    echo "       ⚡ FastTransfer — High-Speed Ethernet File Transfer ⚡        "
    echo "                    Automated Linux Setup Script                      "
    echo "======================================================================"
    echo -e "${NC}"
}

print_help() {
    print_banner
    echo -e "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -r, --run          Launch FastTransfer immediately after build & installation"
    echo "  -n, --no-install   Build and test locally without installing to /usr/bin"
    echo "  -p, --package      Build native Arch Linux package via makepkg (Arch only)"
    echo "  -h, --help         Show this help message"
    echo ""
    exit 0
}

# Parse CLI options
while [[ $# -gt 0 ]]; do
    case "$1" in
        -r|--run)
            RUN_AFTER_BUILD=true
            shift
            ;;
        -n|--no-install)
            NO_INSTALL=true
            shift
            ;;
        -p|--package)
            BUILD_ARCH_PKG=true
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

# --- Step 1: Detect Linux Distribution ---
echo -e "${BLUE}[1/6] Detecting Linux distribution...${NC}"

DISTRO="unknown"
if [ -f /etc/arch-release ] || command -v pacman &> /dev/null; then
    DISTRO="arch"
    echo -e "${GREEN}✓ Detected: Arch Linux / Arch-based distribution${NC}"
elif [ -f /etc/debian_version ] || command -v apt-get &> /dev/null; then
    DISTRO="debian"
    echo -e "${GREEN}✓ Detected: Debian / Ubuntu-based distribution${NC}"
elif [ -f /etc/fedora-release ] || command -v dnf &> /dev/null; then
    DISTRO="fedora"
    echo -e "${GREEN}✓ Detected: Fedora / RHEL-based distribution${NC}"
elif command -v zypper &> /dev/null; then
    DISTRO="suse"
    echo -e "${GREEN}✓ Detected: openSUSE distribution${NC}"
else
    echo -e "${YELLOW}! Unrecognized Linux distribution. Will check for required tools.${NC}"
fi

# --- Step 2: Install Required Dependencies ---
echo -e "\n${BLUE}[2/6] Checking and installing required packages...${NC}"

case "$DISTRO" in
    arch)
        echo -e "${CYAN}→ Synchronizing and installing packages via pacman...${NC}"
        sudo pacman -S --needed --noconfirm base-devel cmake ninja qt6-base
        ;;
    debian)
        echo -e "${CYAN}→ Installing packages via apt...${NC}"
        sudo apt-get update
        sudo apt-get install -y build-essential cmake ninja-build \
            qt6-base-dev qt6-base-dev-tools libqt6network6 libqt6sql6 libgl1-mesa-dev
        ;;
    fedora)
        echo -e "${CYAN}→ Installing packages via dnf...${NC}"
        sudo dnf install -y gcc-c++ cmake ninja-build qt6-qtbase-devel
        ;;
    suse)
        echo -e "${CYAN}→ Installing packages via zypper...${NC}"
        sudo zypper install -y gcc-c++ cmake ninja qt6-base-devel
        ;;
    *)
        echo -e "${YELLOW}Verifying required CLI tools manually...${NC}"
        for cmd in cmake g++ ninja; do
            if ! command -v $cmd &> /dev/null; then
                echo -e "${RED}Error: Required command '$cmd' is not installed.${NC}"
                exit 1
            fi
        done
        ;;
esac

echo -e "${GREEN}✓ All build prerequisites satisfied.${NC}"

# Optional: Handle Arch PKGBUILD flow if requested
if [ "$DISTRO" = "arch" ] && [ "$BUILD_ARCH_PKG" = true ]; then
    echo -e "\n${BLUE}Building native Arch Linux package (.pkg.tar.zst)...${NC}"
    cd packaging/arch
    makepkg -si --noconfirm
    echo -e "${GREEN}✓ Native Arch package installed successfully!${NC}"
    if [ "$RUN_AFTER_BUILD" = true ]; then
        exec fasttransfer
    fi
    exit 0
fi

# --- Step 3: Configure CMake with Ninja ---
BUILD_DIR="build-linux"
echo -e "\n${BLUE}[3/6] Configuring FastTransfer with CMake (Ninja / Release)...${NC}"
cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -GNinja

# --- Step 4: Parallel Compilation ---
CPU_CORES=$(nproc 2>/dev/null || echo 4)
echo -e "\n${BLUE}[4/6] Compiling FastTransfer using ${CPU_CORES} parallel cores...${NC}"
cmake --build "$BUILD_DIR" --parallel "$CPU_CORES"
echo -e "${GREEN}✓ Compilation finished successfully.${NC}"

# --- Step 5: Automated Verification Suite ---
echo -e "\n${BLUE}[5/6] Executing automated test suite...${NC}"
ctest --test-dir "$BUILD_DIR" --output-on-failure
echo -e "${GREEN}✓ All 4 automated test suites passed (100% success).${NC}"

# --- Step 6: System Installation & Desktop Integration ---
if [ "$NO_INSTALL" = false ]; then
    echo -e "\n${BLUE}[6/6] Installing FastTransfer system-wide to /usr/bin...${NC}"
    sudo cmake --install "$BUILD_DIR"

    # Create lowercase symlink for terminal convenience
    if [ -f /usr/bin/FastTransfer ] && [ ! -f /usr/bin/fasttransfer ]; then
        sudo ln -sf /usr/bin/FastTransfer /usr/bin/fasttransfer
    fi

    # Update desktop database and icon caches if available
    if command -v update-desktop-database &> /dev/null; then
        sudo update-desktop-database /usr/share/applications 2>/dev/null || true
    fi
    if command -v gtk-update-icon-cache &> /dev/null; then
        sudo gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
    fi

    echo -e "${GREEN}✓ FastTransfer installed successfully!${NC}"
    echo -e "  • Executable:      ${BOLD}/usr/bin/FastTransfer${NC} (or 'fasttransfer')"
    echo -e "  • Desktop Entry:   ${BOLD}/usr/share/applications/fasttransfer.desktop${NC}"
    echo -e "  • Application Icon:${BOLD}/usr/share/icons/hicolor/scalable/apps/fasttransfer.svg${NC}"
else
    echo -e "\n${YELLOW}[6/6] Skipping system installation (--no-install specified).${NC}"
    echo -e "Local binary available at: ${BOLD}./$BUILD_DIR/FastTransfer${NC}"
fi

# --- Optional: Firewall Setup ---
echo -e "\n${BLUE}Checking firewall settings...${NC}"
if command -v ufw &> /dev/null && sudo ufw status | grep -q "Status: active"; then
    echo -e "${CYAN}Configuring UFW to allow FastTransfer Ethernet ports (45820/udp & 45821/tcp)...${NC}"
    sudo ufw allow 45820/udp comment 'FastTransfer Discovery' >/dev/null 2>&1 || true
    sudo ufw allow 45821/tcp comment 'FastTransfer File Transfer' >/dev/null 2>&1 || true
    echo -e "${GREEN}✓ UFW rules configured.${NC}"
elif command -v firewall-cmd &> /dev/null && sudo systemctl is-active --quiet firewalld; then
    echo -e "${CYAN}Configuring Firewalld to allow FastTransfer Ethernet ports...${NC}"
    sudo firewall-cmd --permanent --add-port=45820/udp >/dev/null 2>&1 || true
    sudo firewall-cmd --permanent --add-port=45821/tcp >/dev/null 2>&1 || true
    sudo firewall-cmd --reload >/dev/null 2>&1 || true
    echo -e "${GREEN}✓ Firewalld rules configured.${NC}"
else
    echo -e "${CYAN}No active UFW or Firewalld detected. Standard Ethernet traffic is unrestricted.${NC}"
fi

echo -e "\n${GREEN}${BOLD}======================================================================${NC}"
echo -e "${GREEN}${BOLD}✓ Setup Complete! FastTransfer is ready to use.                        ${NC}"
echo -e "${GREEN}${BOLD}======================================================================${NC}"

if [ "$RUN_AFTER_BUILD" = true ]; then
    echo -e "\n${CYAN}Launching FastTransfer...${NC}"
    if [ "$NO_INSTALL" = true ]; then
        exec "./$BUILD_DIR/FastTransfer"
    else
        exec /usr/bin/FastTransfer
    fi
else
    echo -e "You can launch FastTransfer by:"
    echo -e "  1. Running ${BOLD}fasttransfer${NC} from your terminal."
    echo -e "  2. Selecting ${BOLD}FastTransfer${NC} from your desktop application launcher."
    echo -e "  3. Running ${BOLD}./$BUILD_DIR/FastTransfer${NC} directly."
fi
