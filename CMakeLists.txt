cmake_minimum_required(VERSION 3.20)
project(FastTransfer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable automatic Qt moc/rcc/uic
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql Test)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# BLAKE3 C source files
set(BLAKE3_SOURCES
    third_party/blake3/blake3.h
    third_party/blake3/blake3_impl.h
    third_party/blake3/blake3.c
    third_party/blake3/blake3_portable.c
)

# Platform-specific sources
set(PLATFORM_SOURCES
    src/platform/PlatformNetwork.h
    src/platform/PlatformFilesystem.h
    src/platform/PlatformFilesystem.cpp
)

if(WIN32)
    list(APPEND PLATFORM_SOURCES src/platform/windows/WindowsNetwork.cpp)
    set(PLATFORM_LIBS iphlpapi ws2_32)
elseif(UNIX AND NOT APPLE)
    list(APPEND PLATFORM_SOURCES src/platform/linux/LinuxNetwork.cpp)
    set(PLATFORM_LIBS "")
endif()

# Core engine sources (independent from GUI)
set(CORE_SOURCES
    ${BLAKE3_SOURCES}
    ${PLATFORM_SOURCES}
    src/integrity/Blake3Hasher.h
    src/integrity/Blake3Hasher.cpp
    src/network/Protocol.h
    src/network/Protocol.cpp
    src/network/DiscoveryService.h
    src/network/DiscoveryService.cpp
    src/network/TcpConnection.h
    src/network/TcpConnection.cpp
    src/filesystem/FileEnumerator.h
    src/filesystem/FileEnumerator.cpp
    src/filesystem/FileReader.h
    src/filesystem/FileReader.cpp
    src/filesystem/FileWriter.h
    src/filesystem/FileWriter.cpp
    src/database/Database.h
    src/database/Database.cpp
    src/core/TransferTypes.h
    src/core/TransferSession.h
    src/core/TransferSession.cpp
    src/core/TransferManager.h
    src/core/TransferManager.cpp
)

add_library(fasttransfer_core STATIC ${CORE_SOURCES})
target_link_libraries(fasttransfer_core PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    ${PLATFORM_LIBS}
)

# GUI sources
set(GUI_SOURCES
    src/gui/ThemeManager.h
    src/gui/ThemeManager.cpp
    src/gui/SpeedGraph.h
    src/gui/SpeedGraph.cpp
    src/gui/SendPage.h
    src/gui/SendPage.cpp
    src/gui/ReceivePage.h
    src/gui/ReceivePage.cpp
    src/gui/TransferPage.h
    src/gui/TransferPage.cpp
    src/gui/HistoryPage.h
    src/gui/HistoryPage.cpp
    src/gui/DevicesPage.h
    src/gui/DevicesPage.cpp
    src/gui/SettingsPage.h
    src/gui/SettingsPage.cpp
    src/gui/MainWindow.h
    src/gui/MainWindow.cpp
)

add_executable(FastTransfer
    src/main.cpp
    ${GUI_SOURCES}
)

target_link_libraries(FastTransfer PRIVATE
    fasttransfer_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
)

# Automated Tests
enable_testing()

add_executable(test_protocol tests/test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE fasttransfer_core Qt6::Test)
add_test(NAME ProtocolTest COMMAND test_protocol)

add_executable(test_blake3 tests/test_blake3.cpp)
target_link_libraries(test_blake3 PRIVATE fasttransfer_core Qt6::Test)
add_test(NAME Blake3Test COMMAND test_blake3)

add_executable(test_filesystem tests/test_filesystem.cpp)
target_link_libraries(test_filesystem PRIVATE fasttransfer_core Qt6::Test)
add_test(NAME FilesystemTest COMMAND test_filesystem)

add_executable(test_loopback tests/test_loopback_transfer.cpp)
target_link_libraries(test_loopback PRIVATE fasttransfer_core Qt6::Test)
add_test(NAME LoopbackTransferTest COMMAND test_loopback)

# Installation Targets for Linux and Windows
include(GNUInstallDirs)

install(TARGETS FastTransfer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/fasttransfer.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    install(FILES resources/icons/fasttransfer.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    )
endif()

