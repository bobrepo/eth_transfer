#include "FileEnumerator.h"
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include "../platform/PlatformFilesystem.h"

namespace FastTransfer {

FileManifest FileEnumerator::enumeratePaths(const QStringList& inputPaths) {
    FileManifest manifest;
    uint64_t currentIndex = 0;

    for (const QString& pathStr : inputPaths) {
        QFileInfo info(pathStr);
        if (!info.exists()) continue;

        if (info.isFile()) {
            LocalFileItem item;
            item.fileIndex = currentIndex++;
            item.sourceAbsolutePath = info.absoluteFilePath();
            item.relativePath = PlatformFilesystem::sanitizePath(info.fileName());
            item.fileSize = static_cast<uint64_t>(info.size());
            item.modifiedTime = static_cast<uint64_t>(info.lastModified().toMSecsSinceEpoch());

            manifest.totalFiles++;
            manifest.totalBytes += item.fileSize;
            manifest.items.append(item);
        } else if (info.isDir()) {
            QDir rootDir(info.absoluteFilePath());
            QString rootDirName = info.fileName();
            if (rootDirName.isEmpty()) {
                rootDirName = QStringLiteral("Folder");
            }

            QDirIterator it(rootDir.absolutePath(),
                            QDir::Files | QDir::NoSymLinks | QDir::Hidden,
                            QDirIterator::Subdirectories);

            while (it.hasNext()) {
                QString filePath = it.next();
                QFileInfo fileInfo = it.fileInfo();

                // Compute relative path within the scanned directory
                QString relInside = rootDir.relativeFilePath(filePath);
                QString fullRel = rootDirName + "/" + relInside;
                QString safeRel = PlatformFilesystem::sanitizePath(fullRel);

                LocalFileItem item;
                item.fileIndex = currentIndex++;
                item.sourceAbsolutePath = fileInfo.absoluteFilePath();
                item.relativePath = safeRel;
                item.fileSize = static_cast<uint64_t>(fileInfo.size());
                item.modifiedTime = static_cast<uint64_t>(fileInfo.lastModified().toMSecsSinceEpoch());

                manifest.totalFiles++;
                manifest.totalBytes += item.fileSize;
                manifest.items.append(item);
            }
        }
    }

    return manifest;
}

} // namespace FastTransfer
