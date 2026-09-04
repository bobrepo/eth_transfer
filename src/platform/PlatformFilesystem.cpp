#include "PlatformFilesystem.h"
#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <QRegularExpression>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/statvfs.h>
#endif

namespace FastTransfer {

bool PlatformFilesystem::preallocateFile(const QString& filePath, uint64_t size) {
    if (size == 0) return true;

#ifdef Q_OS_WIN
    HANDLE hFile = CreateFileW(reinterpret_cast<LPCWSTR>(filePath.utf16()),
                               GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr,
                               OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,
                               nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER liSize;
    liSize.QuadPart = static_cast<LONGLONG>(size);

    FILE_ALLOCATION_INFO allocInfo;
    allocInfo.AllocationSize = liSize;

    BOOL ok = SetFileInformationByHandle(hFile, FileAllocationInfo, &allocInfo, sizeof(allocInfo));
    if (!ok) {
        // Fallback to setting end of file
        if (SetFilePointerEx(hFile, liSize, nullptr, FILE_BEGIN)) {
            SetEndOfFile(hFile);
            LARGE_INTEGER zero;
            zero.QuadPart = 0;
            SetFilePointerEx(hFile, zero, nullptr, FILE_BEGIN);
            ok = TRUE;
        }
    }

    CloseHandle(hFile);
    return ok != FALSE;

#elif defined(Q_OS_LINUX)
    int fd = open(filePath.toUtf8().constData(), O_WRONLY | O_CREAT, 0644);
    if (fd == -1) {
        return false;
    }

    int ret = posix_fallocate(fd, 0, static_cast<off_t>(size));
    close(fd);
    return (ret == 0);
#else
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return file.resize(static_cast<qint64>(size));
    }
    return false;
#endif
}

QString PlatformFilesystem::sanitizePath(const QString& relativePath) {
    QString clean = relativePath;
    clean.replace('\\', '/');

    // Strip leading slashes
    while (clean.startsWith('/')) {
        clean.remove(0, 1);
    }

    // Strip drive letters like "C:/" or "D:/"
    if (clean.length() >= 2 && clean.at(1) == ':') {
        clean.remove(0, 2);
        while (clean.startsWith('/')) {
            clean.remove(0, 1);
        }
    }

    QStringList segments = clean.split('/', Qt::SkipEmptyParts);
    QStringList safeSegments;

    for (const QString& seg : segments) {
        QString trimmed = seg.trimmed();
        if (trimmed.isEmpty() || trimmed == "." || trimmed == "..") {
            continue; // Drop parent or current directory tokens
        }

        // Strip illegal filename characters: < > : " / \ | ? *
        static const QRegularExpression illegalChars(QStringLiteral(R"([<>:"/\\|?*\x00-\x1F])"));
        trimmed.remove(illegalChars);

        if (!trimmed.isEmpty()) {
            safeSegments.append(trimmed);
        }
    }

    return safeSegments.join('/');
}

bool PlatformFilesystem::isSafeRelativePath(const QString& relativePath) {
    if (relativePath.isEmpty()) {
        return false;
    }

    QString norm = relativePath;
    norm.replace('\\', '/');

    // Disallow absolute paths or UNC
    if (norm.startsWith('/') || norm.startsWith("//")) {
        return false;
    }

    // Disallow drive letters
    if (norm.contains(':')) {
        return false;
    }

    // Disallow null bytes
    if (norm.contains('\0')) {
        return false;
    }

    QStringList parts = norm.split('/', Qt::SkipEmptyParts);
    int depth = 0;
    for (const QString& part : parts) {
        QString p = part.trimmed();
        if (p == "..") {
            depth--;
            if (depth < 0) {
                return false; // Traverses outside root
            }
        } else if (p != ".") {
            depth++;
        }
    }

    return depth > 0;
}

QString PlatformFilesystem::sanitizeDeviceName(const QString& rawName) {
    QString clean = rawName.trimmed();
    static const QRegularExpression illegalChars(QStringLiteral(R"([<>:"/\\|?*\x00-\x1F\s]+)"));
    clean.replace(illegalChars, QStringLiteral("-"));

    while (clean.startsWith('-')) clean.remove(0, 1);
    while (clean.endsWith('-')) clean.chop(1);

    if (clean.isEmpty()) {
        clean = QStringLiteral("Unknown-Device");
    }

    return clean;
}

uint64_t PlatformFilesystem::getAvailableDiskSpace(const QString& path) {
    QStorageInfo storage(path);
    storage.refresh();
    if (storage.isValid()) {
        qint64 freeBytes = storage.bytesAvailable();
        if (freeBytes > 0) {
            return static_cast<uint64_t>(freeBytes);
        }
    }
    return 0;
}

} // namespace FastTransfer
