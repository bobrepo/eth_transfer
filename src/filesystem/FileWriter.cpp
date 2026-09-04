#include "FileWriter.h"
#include <QDir>
#include <QFileInfo>
#include <vector>
#include "../platform/PlatformFilesystem.h"

namespace FastTransfer {

FileWriter::FileWriter() {
}

FileWriter::~FileWriter() {
    close();
}

QString FileWriter::resolveUniqueName(const QString& targetFilePath) {
    if (!QFile::exists(targetFilePath)) {
        return targetFilePath;
    }

    QFileInfo info(targetFilePath);
    QString dir = info.absolutePath();
    QString baseName = info.completeBaseName();
    QString ext = info.suffix();
    if (!ext.isEmpty()) {
        ext.prepend('.');
    }

    int counter = 1;
    QString candidate;
    do {
        candidate = QString("%1/%2 (%3)%4").arg(dir, baseName, QString::number(counter++), ext);
    } while (QFile::exists(candidate) || QFile::exists(candidate + ".partial"));

    return candidate;
}

bool FileWriter::open(const QString& baseOutputDir,
                      const QString& senderDevice,
                      const QString& relativePath,
                      uint64_t expectedFileSize,
                      uint64_t resumeOffset,
                      DuplicatePolicy policy) {
    close();

    // Verify disk space upfront if non-zero expected size
    if (expectedFileSize > 0) {
        uint64_t availableDisk = PlatformFilesystem::getAvailableDiskSpace(baseOutputDir);
        if (availableDisk > 0 && expectedFileSize > availableDisk) {
            m_errorString = QString("Insufficient disk space on destination drive. Required: %1 bytes, Available: %2 bytes")
                                .arg(expectedFileSize)
                                .arg(availableDisk);
            return false;
        }
    }

    QString cleanFolder = PlatformFilesystem::sanitizeDeviceName(senderDevice);
    QString cleanRel = PlatformFilesystem::sanitizePath(relativePath);

    QDir base(baseOutputDir);
    QString targetDir = base.filePath(cleanFolder);
    m_targetFilePath = QDir(targetDir).filePath(cleanRel);
    m_expectedFileSize = expectedFileSize;
    m_hasher.reset();
    m_errorString.clear();

    // Handle existing file duplicate policies
    if (QFile::exists(m_targetFilePath)) {
        if (policy == DuplicatePolicy::Rename || policy == DuplicatePolicy::Ask) {
            m_targetFilePath = resolveUniqueName(m_targetFilePath);
        } else if (policy == DuplicatePolicy::Replace) {
            QFile::remove(m_targetFilePath);
        } else if (policy == DuplicatePolicy::Skip) {
            m_errorString = QStringLiteral("File already exists and policy is Skip");
            return false;
        }
    }

    m_partialFilePath = m_targetFilePath + QStringLiteral(".partial");

    // Ensure directory hierarchy is created
    QFileInfo partialInfo(m_partialFilePath);
    QDir().mkpath(partialInfo.absolutePath());

    if (resumeOffset > 0 && QFile::exists(m_partialFilePath)) {
        m_file.setFileName(m_partialFilePath);
        if (!m_file.open(QIODevice::ReadWrite)) {
            m_errorString = m_file.errorString();
            return false;
        }

        // Fast-forward BLAKE3 hasher on already written bytes
        constexpr qint64 kFastBuf = 4 * 1024 * 1024;
        std::vector<char> tempBuf(kFastBuf);
        uint64_t remaining = std::min<uint64_t>(resumeOffset, static_cast<uint64_t>(m_file.size()));

        m_file.seek(0);
        while (remaining > 0) {
            qint64 toRead = static_cast<qint64>(std::min<uint64_t>(remaining, kFastBuf));
            qint64 bytesRead = m_file.read(tempBuf.data(), toRead);
            if (bytesRead <= 0) break;
            m_hasher.update(tempBuf.data(), static_cast<size_t>(bytesRead));
            remaining -= static_cast<uint64_t>(bytesRead);
        }

        m_file.seek(resumeOffset);
        m_bytesWritten = resumeOffset;
    } else {
        if (QFile::exists(m_partialFilePath)) {
            QFile::remove(m_partialFilePath);
        }

        // Preallocate disk space
        PlatformFilesystem::preallocateFile(m_partialFilePath, expectedFileSize);

        m_file.setFileName(m_partialFilePath);
        // Note: Using ReadWrite so that Qt does not truncate the preallocated file!
        if (!m_file.open(QIODevice::ReadWrite)) {
            m_errorString = m_file.errorString();
            return false;
        }
        m_bytesWritten = 0;
    }

    return true;
}

bool FileWriter::writeChunk(uint64_t offset, const QByteArray& data) {
    if (!m_file.isOpen()) {
        m_errorString = QStringLiteral("File is not open for writing");
        return false;
    }

    if (static_cast<uint64_t>(m_file.pos()) != offset) {
        if (!m_file.seek(static_cast<qint64>(offset))) {
            m_errorString = m_file.errorString();
            return false;
        }
    }

    qint64 written = m_file.write(data);
    if (written != data.size()) {
        m_errorString = m_file.errorString();
        return false;
    }

    m_hasher.update(data);
    m_bytesWritten += static_cast<uint64_t>(written);
    return true;
}

bool FileWriter::finalizeAndCommit(const QString& expectedBlake3) {
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }

    m_calculatedChecksum = m_hasher.finalizeHex();

    if (!expectedBlake3.isEmpty() &&
        QString::compare(expectedBlake3, m_calculatedChecksum, Qt::CaseInsensitive) != 0) {
        m_errorString = QString("BLAKE3 Checksum Mismatch: expected %1, computed %2")
                            .arg(expectedBlake3, m_calculatedChecksum);
        return false; // Retain .partial file for retry/resume
    }

    // Atomic commit: remove final destination file if exists, then rename .partial -> final
    if (QFile::exists(m_targetFilePath)) {
        QFile::remove(m_targetFilePath);
    }

    bool renamed = QFile::rename(m_partialFilePath, m_targetFilePath);
    if (!renamed) {
        m_errorString = QStringLiteral("Failed to commit .partial file to destination path");
        return false;
    }

    return true;
}

void FileWriter::close() {
    if (m_file.isOpen()) {
        m_file.close();
    }
}

void FileWriter::abort() {
    close();
    // Intentionally keep .partial file on disk for future resume!
}

} // namespace FastTransfer
