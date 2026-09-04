#include <QTest>
#include "../src/integrity/Blake3Hasher.h"
#include <QTemporaryFile>

using namespace FastTransfer;

class TestBlake3 : public QObject {
    Q_OBJECT
private slots:
    void testEmptyInputHash() {
        Blake3Hasher hasher;
        QString hex = hasher.finalizeHex();
        // Official BLAKE3 test vector for 0 bytes input
        QCOMPARE(hex.toLower(), QString("af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"));
    }

    void testIncrementalChunkMatchesDirect() {
        QByteArray data;
        data.resize(50000);
        for (int i = 0; i < data.size(); ++i) {
            data[i] = static_cast<char>(i % 256);
        }

        // Direct hash
        QString directHex = Blake3Hasher::hashBytes(data);

        // Incremental chunked hash
        Blake3Hasher chunkedHasher;
        int chunkSize = 1024;
        for (int offset = 0; offset < data.size(); offset += chunkSize) {
            int len = std::min<int>(chunkSize, static_cast<int>(data.size() - offset));
            chunkedHasher.update(data.constData() + offset, len);
        }
        QString chunkedHex = chunkedHasher.finalizeHex();

        QCOMPARE(chunkedHex, directHex);
    }

    void testFileHashing() {
        QTemporaryFile tempFile;
        QVERIFY(tempFile.open());

        QByteArray testPayload = "The quick brown fox jumps over the lazy dog";
        tempFile.write(testPayload);
        tempFile.flush();

        QString expected = Blake3Hasher::hashBytes(testPayload);
        QString fileHash = Blake3Hasher::hashFile(tempFile.fileName());

        QCOMPARE(fileHash, expected);
    }
};

QTEST_MAIN(TestBlake3)
#include "test_blake3.moc"
