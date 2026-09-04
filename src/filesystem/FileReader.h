#ifndef FASTTRANSFER_FILE_READER_H
#define FASTTRANSFER_FILE_READER_H

#include <QString>
#include <QFile>
#include <QByteArray>
#include <cstdint>
#include "../integrity/Blake3Hasher.h"

namespace FastTransfer {

class FileReader {
public:
    explicit FileReader(uint32_t chunkSize = 8 * 1024 * 1024);
    ~FileReader();

    bool open(const QString& filePath, uint64_t startOffset = 0);
    void close();

    bool isOpen() const;
    uint64_t fileSize() const { return m_fileSize; }
    uint64_t currentOffset() const { return m_currentOffset; }
    bool isAtEnd() const;

    // Reads the next chunk and feeds it into the BLAKE3 incremental hasher
    QByteArray readNextChunk();

    // Returns the calculated BLAKE3 checksum of all bytes read
    QString finalizeBlake3();

    QString errorString() const { return m_errorString; }

private:
    QFile m_file;
    uint32_t m_chunkSize;
    uint64_t m_fileSize = 0;
    uint64_t m_currentOffset = 0;
    Blake3Hasher m_hasher;
    QString m_errorString;
};

} // namespace FastTransfer

#endif // FASTTRANSFER_FILE_READER_H
