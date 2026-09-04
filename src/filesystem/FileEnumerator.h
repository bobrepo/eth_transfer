#ifndef FASTTRANSFER_FILE_ENUMERATOR_H
#define FASTTRANSFER_FILE_ENUMERATOR_H

#include <QString>
#include <QStringList>
#include <QList>
#include <cstdint>
#include "../network/Protocol.h"

namespace FastTransfer {

struct LocalFileItem {
    uint64_t fileIndex = 0;
    QString sourceAbsolutePath;
    QString relativePath;
    uint64_t fileSize = 0;
    uint64_t modifiedTime = 0;
    QString blake3Checksum;
};

struct FileManifest {
    uint64_t totalFiles = 0;
    uint64_t totalBytes = 0;
    QList<LocalFileItem> items;

    QList<ManifestItem> toProtocolItems() const {
        QList<ManifestItem> pItems;
        pItems.reserve(items.size());
        for (const auto& it : items) {
            ManifestItem pi;
            pi.fileIndex = it.fileIndex;
            pi.relativePath = it.relativePath;
            pi.fileSize = it.fileSize;
            pi.modifiedTime = it.modifiedTime;
            pi.blake3Checksum = it.blake3Checksum;
            pItems.append(pi);
        }
        return pItems;
    }
};

class FileEnumerator {
public:
    // Recursively scans a list of files and folders to construct a transfer manifest
    static FileManifest enumeratePaths(const QStringList& inputPaths);
};

} // namespace FastTransfer

#endif // FASTTRANSFER_FILE_ENUMERATOR_H
