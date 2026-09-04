#ifndef FASTTRANSFER_PLATFORM_FILESYSTEM_H
#define FASTTRANSFER_PLATFORM_FILESYSTEM_H

#include <QString>
#include <cstdint>

namespace FastTransfer {

class PlatformFilesystem {
public:
    // Preallocates contiguous disk blocks for a file to minimize filesystem fragmentation
    static bool preallocateFile(const QString& filePath, uint64_t size);

    // Normalizes and sanitizes a relative path from the network, preventing directory traversal
    static QString sanitizePath(const QString& relativePath);

    // Checks whether a path is strictly a safe relative path without escaping parent bounds
    static bool isSafeRelativePath(const QString& relativePath);

    // Sanitizes a device name for safe usage as a directory name on all operating systems
    static QString sanitizeDeviceName(const QString& rawName);

    // Queries available free disk space on the drive hosting the given path
    static uint64_t getAvailableDiskSpace(const QString& path);
};

} // namespace FastTransfer

#endif // FASTTRANSFER_PLATFORM_FILESYSTEM_H
