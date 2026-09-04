#ifndef FASTTRANSFER_FILE_WRITER_H
#define FASTTRANSFER_FILE_WRITER_H

#include <QString>
#include <QFile>
#include <QByteArray>
#include <cstdint>
#include "../integrity/Blake3Hasher.h"

namespace FastTransfer {

enum class DuplicatePolicy {
    Ask = 0,
    Replace = 1,
    Skip = 2,
    Rename = 3
};

class FileWriter {
public:
    FileWriter();
    ~FileWriter();

    // Prepares the target file, applies duplicate policy, and opens .partial file
    bool open(const QString& baseOutputDir,
              const QString& senderDevice,
              const QString& relativePath,
              uint64_t expectedFileSize,
              uint64_t resumeOffset = 0,
              DuplicatePolicy policy = DuplicatePolicy::Rename);

    // Write chunk at specified offset
    bool writeChunk(uint64_t offset, const QByteArray& data);

    // Finalizes BLAKE3 checksum, verifies against expected checksum, and atomically commits .partial to final path
    bool finalizeAndCommit(const QString& expectedBlake3);

    void close();
    void abort(); // Closes file handle but leaves .partial for resume

    uint64_t bytesWritten() const { return m_bytesWritten; }
    QString targetFilePath() const { return m_targetFilePath; }
    QString partialFilePath() const { return m_partialFilePath; }
    QString calculatedChecksum() const { return m_calculatedChecksum; }
    QString errorString() const { return m_errorString; }

    // Utility: generates safe non-colliding name e.g. "Game (1).zip"
    static QString resolveUniqueName(const QString& targetFilePath);

private:
    QFile m_file;
    QString m_targetFilePath;
    QString m_partialFilePath;
    uint64_t m_expectedFileSize = 0;
    uint64_t m_bytesWritten = 0;
    Blake3Hasher m_hasher;
    QString m_calculatedChecksum;
    QString m_errorString;
};

} // namespace FastTransfer

#endif // FASTTRANSFER_FILE_WRITER_H
