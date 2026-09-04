#include "Blake3Hasher.h"
#include <QFile>
#include <QFileInfo>
#include <vector>

namespace FastTransfer {

Blake3Hasher::Blake3Hasher() {
    reset();
}

void Blake3Hasher::reset() {
    blake3_hasher_init(&m_hasher);
    m_finalized = false;
    memset(m_cachedDigest, 0, sizeof(m_cachedDigest));
}

void Blake3Hasher::update(const void* data, size_t length) {
    if (m_finalized) {
        reset();
    }
    if (data && length > 0) {
        blake3_hasher_update(&m_hasher, data, length);
    }
}

void Blake3Hasher::update(const QByteArray& data) {
    update(data.constData(), static_cast<size_t>(data.size()));
}

QByteArray Blake3Hasher::finalizeRaw() {
    if (!m_finalized) {
        blake3_hasher_finalize(&m_hasher, m_cachedDigest, BLAKE3_OUT_LEN);
        m_finalized = true;
    }
    return QByteArray(reinterpret_cast<const char*>(m_cachedDigest), BLAKE3_OUT_LEN);
}

QString Blake3Hasher::finalizeHex() {
    QByteArray raw = finalizeRaw();
    return QString::fromLatin1(raw.toHex());
}

QString Blake3Hasher::hashBytes(const QByteArray& data) {
    Blake3Hasher hasher;
    hasher.update(data);
    return hasher.finalizeHex();
}

QString Blake3Hasher::hashFile(const QString& filePath,
                              const std::function<void(uint64_t bytesProcessed, uint64_t totalBytes)>& progressCallback,
                              const std::function<bool()>& shouldCancelCallback) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    const uint64_t totalBytes = static_cast<uint64_t>(file.size());
    uint64_t bytesProcessed = 0;

    Blake3Hasher hasher;

    // Use 4MB buffer for streaming file hashing
    constexpr qint64 kBufferSize = 4 * 1024 * 1024;
    std::vector<char> buffer(kBufferSize);

    while (!file.atEnd()) {
        if (shouldCancelCallback && shouldCancelCallback()) {
            return QString();
        }

        qint64 bytesRead = file.read(buffer.data(), kBufferSize);
        if (bytesRead <= 0) {
            break;
        }

        hasher.update(buffer.data(), static_cast<size_t>(bytesRead));
        bytesProcessed += static_cast<uint64_t>(bytesRead);

        if (progressCallback) {
            progressCallback(bytesProcessed, totalBytes);
        }
    }

    return hasher.finalizeHex();
}

} // namespace FastTransfer
