#include "FileReader.h"
#include <vector>

namespace FastTransfer {

FileReader::FileReader(uint32_t chunkSize)
    : m_chunkSize(chunkSize > 0 ? chunkSize : 8 * 1024 * 1024) {
}

FileReader::~FileReader() {
    close();
}

bool FileReader::open(const QString& filePath, uint64_t startOffset) {
    close();
    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::ReadOnly)) {
        m_errorString = m_file.errorString();
        return false;
    }

    m_fileSize = static_cast<uint64_t>(m_file.size());
    m_hasher.reset();
    m_currentOffset = 0;

    if (startOffset > 0 && startOffset <= m_fileSize) {
        // Fast-forward BLAKE3 hasher up to startOffset to maintain integrity state
        constexpr qint64 kFastBufferSize = 4 * 1024 * 1024;
        std::vector<char> tempBuf(kFastBufferSize);
        uint64_t remaining = startOffset;

        while (remaining > 0) {
            qint64 toRead = static_cast<qint64>(std::min<uint64_t>(remaining, kFastBufferSize));
            qint64 bytesRead = m_file.read(tempBuf.data(), toRead);
            if (bytesRead <= 0) {
                m_errorString = QStringLiteral("Failed to seek file for resume hashing");
                close();
                return false;
            }
            m_hasher.update(tempBuf.data(), static_cast<size_t>(bytesRead));
            remaining -= static_cast<uint64_t>(bytesRead);
        }

        m_currentOffset = startOffset;
    }

    return true;
}

void FileReader::close() {
    if (m_file.isOpen()) {
        m_file.close();
    }
    m_fileSize = 0;
    m_currentOffset = 0;
}

bool FileReader::isOpen() const {
    return m_file.isOpen();
}

bool FileReader::isAtEnd() const {
    return !m_file.isOpen() || m_currentOffset >= m_fileSize;
}

QByteArray FileReader::readNextChunk() {
    if (!isOpen() || isAtEnd()) {
        return QByteArray();
    }

    uint64_t remaining = m_fileSize - m_currentOffset;
    uint32_t toRead = static_cast<uint32_t>(std::min<uint64_t>(remaining, m_chunkSize));

    QByteArray chunk = m_file.read(toRead);
    if (chunk.isEmpty()) {
        m_errorString = m_file.errorString();
        return QByteArray();
    }

    m_hasher.update(chunk);
    m_currentOffset += static_cast<uint64_t>(chunk.size());
    return chunk;
}

QString FileReader::finalizeBlake3() {
    return m_hasher.finalizeHex();
}

} // namespace FastTransfer
