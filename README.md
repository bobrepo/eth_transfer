# FastTransfer — High-Speed Ethernet M2M File Transfer Application

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![Qt 6](https://img.shields.io/badge/Qt-6.8-green.svg)](https://www.qt.io/)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-brightgreen.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**FastTransfer** is a high-performance, modern, cross-platform (Windows and Linux) desktop application specifically engineered for transferring multi-gigabyte and multi-terabyte files and deep folder structures directly between computers over local Ethernet.

FastTransfer works completely offline without cloud services, accounts, or external servers. It supports direct **PC-to-PC Ethernet cables** (e.g. `192.168.10.1` ↔ `192.168.10.2`) as well as standard local LAN Ethernet networks (via switch/router).

---

## Key Features

- **Blazing Transfer Speed**: Streaming I/O with configurable 8 MB – 64 MB transfer buffers, `TCP_NODELAY`, and tuned 8 MB socket buffers designed to saturate 1 Gbps (~115 MB/s), 2.5 Gbps (~280 MB/s), and 10 Gbps Ethernet interfaces.
- **Strict Separation of Core & GUI**: The transfer core engine (`fasttransfer_core`) runs independently from the Qt GUI, allowing it to be integrated into CLI tools, daemons, or background services.
- **Folder Preservation**: Preserves relative paths exactly inside `<DownloadDir>/<Sender-Device-Name>/...`. Never flattens nested folder trees.
- **BLAKE3 Cryptographic Integrity**: Streaming chunk hashing using vendored C BLAKE3 with automatic checksum comparison and `.partial` staging.
- **Pause & Resume**: Interrupted transfers and network disconnects preserve `.partial` files and resume from the exact byte offset without restarting from zero.
- **Automatic UDP Discovery**: Broadcasts and listens on UDP port `45820` for instant peer discovery on local networks.
- **Direct PC-to-PC Fallback**: Manual IP connect (`IP:Port`) allows direct link transfers even when UDP broadcast is disabled or unavailable.
- **Physical Ethernet Preference**: Automatically queries physical network adapters, detects link speeds (1 Gbps, 2.5 Gbps, 10 Gbps), and filters out virtual adapters (Docker, WSL, VMware, VPNs).
- **Modern Desktop GUI**: Sleek dark-mode interface with large typography speed meter, dual progress bars, anti-aliased live scrolling speed graph, drag-and-drop landing zone, and SQLite transfer history.

---

## Architecture Overview

```text
┌──────────────────────────────────────────────────────────┐
│                    Qt 6 Desktop GUI                      │
│   (SendPage, ReceivePage, TransferPage, SpeedGraph, etc) │
└────────────────────────────┬─────────────────────────────┘
                             │ Qt Signals / Slots
┌────────────────────────────▼─────────────────────────────┐
│                 Application Controller                   │
│          (TransferManager, DiscoveryService)             │
└────────────────────────────┬─────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────┐
│                     Transfer Session                     │
│    - Streaming Buffered Reader (8MB chunks)              │
│    - Disk Preallocation (fallocate / SetEndOfFile)       │
│    - In-Flight BLAKE3 Hasher                             │
└─────────────┬──────────────────────────────┬─────────────┘
              ▼                              ▼
┌───────────────────────────┐  ┌───────────────────────────┐
│     FT01 TCP Protocol     │  │   UDP Peer Discovery      │
│  - 16-byte binary header  │  │  - Periodic broadcast     │
│  - Chunk framing & Acks   │  │  - Link speed detection   │
└───────────────────────────┘  └───────────────────────────┘
```

---

## Protocol Specification (FT01)

All TCP packets begin with a 16-byte fixed binary header (Network Byte Order / Big-Endian):

| Offset | Size | Type | Description |
|---|---|---|---|
| `0x00` | 4 B | `uint32_t` | Magic bytes: `0x46543031` (`"FT01"`) |
| `0x04` | 2 B | `uint16_t` | Message Type |
| `0x06` | 2 B | `uint16_t` | Flags (compression, options) |
| `0x08` | 8 B | `uint64_t` | Payload length in bytes |

### Core Message Types:
- `0x0001` `HANDSHAKE_REQ`: Device name, version, OS.
- `0x0002` `HANDSHAKE_RESP`: Acceptance status, device name.
- `0x0010` `TRANSFER_OFFER`: Manifest metadata (file list, sizes, timestamps).
- `0x0011` `TRANSFER_ACCEPT`: Acceptance confirmation and resume offsets.
- `0x0012` `TRANSFER_REJECT`: Rejection reason.
- `0x0020` `FILE_START`: File index, safe relative path, total size.
- `0x0021` `FILE_CHUNK`: File index, 64-bit offset, raw chunk payload.
- `0x0022` `FILE_END`: File index, sender BLAKE3 checksum.
- `0x0023` `FILE_ACK`: File index, verification status (0=OK, 1=Mismatch).
- `0x0030` `TRANSFER_PAUSE` / `0x0031` `TRANSFER_RESUME` / `0x0032` `TRANSFER_CANCEL`.
- `0x0033` `TRANSFER_COMPLETE`: Final transfer acknowledgement.

---

## Building and Running

### Prerequisites
- **CMake**: version 3.20 or newer
- **C++ Compiler**: MSVC 2022/2026 (Windows) or GCC 12+ / Clang 14+ (Linux) with C++20 support
- **Qt 6**: `QtCore`, `QtGui`, `QtWidgets`, `QtNetwork`, `QtSql`, `QtTest`

### Windows (MSVC)
```powershell
# Open Visual Studio Developer Command Prompt or initialize vcvars64.bat
cmake -B build -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.8.2/msvc2022_64"
cmake --build build --config Release --parallel

# Run automated test suite
ctest --test-dir build -C Release --output-on-failure

# Deploy Qt runtime dependencies (creates a standalone portable distribution)
& "C:\Qt\6.8.2\msvc2022_64\bin\windeployqt.exe" --release build\Release\FastTransfer.exe

# Launch FastTransfer
.\build\Release\FastTransfer.exe
```

### Linux / Arch Linux (Automated Setup Script)

FastTransfer includes an automated setup script that detects your distribution (Arch Linux, Ubuntu/Debian, Fedora, openSUSE), automatically installs required packages and Qt 6 modules, compiles with Ninja across all CPU cores, runs tests, installs the application and desktop launcher, and configures firewall rules.

#### 1. Quick Automated Setup & Run (Recommended)
```bash
# Make script executable and run:
chmod +x setup.sh
./setup.sh --run
```
*Flag options:*
* `./setup.sh --run`: Automatically builds, installs to `/usr/bin/fasttransfer`, and launches the app.
* `./setup.sh --package`: Builds and installs a native Arch Linux `.pkg.tar.zst` package via `makepkg -si`.
* `./setup.sh --no-install`: Builds and tests locally without requiring `sudo` system installation.

#### 2. Native Arch Linux Package (`makepkg`)
```bash
# Build and install standard native Arch package with desktop integration
cd packaging/arch
makepkg -si
```

#### 3. Standard Manual CMake Build
```bash
sudo pacman -S --needed base-devel cmake ninja qt6-base

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -GNinja
cmake --build build -j$(nproc)

# Run automated test suite
ctest --test-dir build --output-on-failure

# Install system-wide to /usr/local/bin
sudo cmake --install build
```

### Ubuntu / Debian
```bash
sudo apt update && sudo apt install -y qt6-base-dev libqt6network6 libqt6sql6 cmake build-essential ninja-build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/FastTransfer
```

---

## Arch Linux & Linux Firewall Setup

If `ufw`, `firewalld`, or `iptables` is active, open the FastTransfer ports:

```bash
# UFW (Uncomplicated Firewall)
sudo ufw allow 45820/udp comment 'FastTransfer UDP Discovery'
sudo ufw allow 45821/tcp comment 'FastTransfer TCP File Transfer'

# Firewalld
sudo firewall-cmd --permanent --add-port=45820/udp
sudo firewall-cmd --permanent --add-port=45821/tcp
sudo firewall-cmd --reload
```

---

## Direct PC-to-PC Ethernet Setup

To transfer directly between two computers connected by an Ethernet cable:

1. Connect an Ethernet cable between **PC A** and **PC B**.
2. If no DHCP server is present, assign static IPs:
   - **PC A**: IP `192.168.10.1`, Subnet Mask `255.255.255.0`
   - **PC B**: IP `192.168.10.2`, Subnet Mask `255.255.255.0`
3. Launch FastTransfer on both computers.
4. Auto-discovery will locate the peer. Alternatively, enter `192.168.10.2` in the **Manual IP Fallback** field on PC A and click **START TRANSFER**.

---

## Security & Path Traversal Protections

FastTransfer incorporates defensive security practices:
- **Strict Path Normalization**: Reject all absolute paths, drive letters (`C:\`), UNC paths (`\\server\share`), and directory escaping sequences (`../`, `..\`).
- **Bounded Buffer Processing**: Maximum metadata size caps (64 MB) and chunk bounds validation to prevent memory exhaustion attacks.
- **Local Sandbox Execution**: Files are strictly written into `<DownloadDir>/<SanitizedSenderName>/...`.

---

## License

MIT License. See [LICENSE](LICENSE) for details.
