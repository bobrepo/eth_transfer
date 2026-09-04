#ifndef FASTTRANSFER_BLAKE3_HASHER_H
#define FASTTRANSFER_BLAKE3_HASHER_H

#include <QString>
#include <QByteArray>
#include <cstdint>
#include <functional>
#include "third_party/blake3/blake3.h"

namespace FastTransfer {

class Blake3Hasher {
public:
    Blake3Hasher();
    ~Blake3Hasher() = default;

    void reset();
    void update(const void* data, size_t length);
    void update(const QByteArray& data);

    // Finalize and return 32-byte raw binary hash
    QByteArray finalizeRaw();

    // Finalize and return 64-char lowercase hex string
    QString finalizeHex();

    // Utility to hash in-memory data directly
    static QString hashBytes(const QByteArray& data);

    // Utility to streamingly hash an entire file on disk without reading whole file into RAM
    static QString hashFile(const QString& filePath,
                            const std::function<void(uint64_t bytesProcessed, uint64_t totalBytes)>& progressCallback = nullptr,
                            const std::function<bool()>& shouldCancelCallback = nullptr);

private:
    blake3_hasher m_hasher;
    bool m_finalized = false;
    uint8_t m_cachedDigest[BLAKE3_OUT_LEN] = {0};
};

} // namespace FastTransfer

#endif // FASTTRANSFER_BLAKE3_HASHER_H
