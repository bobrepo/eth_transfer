#include <QTest>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QSignalSpy>
#include "../src/core/TransferSession.h"
#include "../src/filesystem/FileEnumerator.h"
#include "../src/integrity/Blake3Hasher.h"

using namespace FastTransfer;

class TestLoopbackTransfer : public QObject {
    Q_OBJECT
private slots:
    void testEndToEndLoopbackStreaming() {
        QTemporaryDir srcDir;
        QTemporaryDir destDir;
        QVERIFY(srcDir.isValid());
        QVERIFY(destDir.isValid());

        // Create 2 MB source test file
        QString srcFilePath = srcDir.filePath("test_large_file.bin");
        QFile srcFile(srcFilePath);
        QVERIFY(srcFile.open(QIODevice::WriteOnly));

        QByteArray pattern(64 * 1024, 'A');
        for (int i = 0; i < pattern.size(); ++i) pattern[i] = static_cast<char>(i % 256);
        for (int block = 0; block < 32; ++block) {
            srcFile.write(pattern);
        }
        srcFile.close();

        QString expectedHash = Blake3Hasher::hashFile(srcFilePath);
        QVERIFY(!expectedHash.isEmpty());

        // Setup loopback TCP Server
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        uint16_t port = server.serverPort();

        TransferSession* receiverSession = nullptr;
        connect(&server, &QTcpServer::newConnection, [&]() {
            QTcpSocket* s = server.nextPendingConnection();
            TcpConnection* conn = new TcpConnection(s);
            receiverSession = new TransferSession(conn, this);
        });

        // Setup Sender
        TransferSession senderSession(TransferDirection::Send);

        FileManifest manifest = FileEnumerator::enumeratePaths(QStringList() << srcFilePath);
        QCOMPARE(manifest.totalFiles, 1ULL);
        QCOMPARE(manifest.totalBytes, 2ULL * 1024 * 1024);

        senderSession.startSend(QHostAddress::LocalHost, port, manifest, "Sender-PC", 512 * 1024);

        // Wait for receiver session to be created
        QTRY_VERIFY_WITH_TIMEOUT(receiverSession != nullptr, 3000);

        // Wait for incoming offer
        QSignalSpy offerSpy(receiverSession, &TransferSession::incomingOfferReceived);
        QTRY_VERIFY_WITH_TIMEOUT(offerSpy.count() >= 1, 3000);

        // Accept offer
        receiverSession->acceptTransfer(destDir.path(), DuplicatePolicy::Replace);

        // Wait for both sender and receiver to complete
        QSignalSpy senderDoneSpy(&senderSession, &TransferSession::transferCompleted);
        QSignalSpy receiverDoneSpy(receiverSession, &TransferSession::transferCompleted);

        QTRY_VERIFY_WITH_TIMEOUT(senderDoneSpy.count() >= 1, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(receiverDoneSpy.count() >= 1, 10000);

        // Verify receiver output path with unique timestamped folder
        QVERIFY(!receiverSession->sessionSubfolder().isEmpty());
        QVERIFY(receiverSession->sessionSubfolder().startsWith(QStringLiteral("Sender-PC_")));

        QString receivedFilePath = destDir.filePath(receiverSession->sessionSubfolder() + QStringLiteral("/test_large_file.bin"));
        QVERIFY(QFile::exists(receivedFilePath));

        // Verify file size and BLAKE3 checksum
        QFile receivedFile(receivedFilePath);
        QCOMPARE(static_cast<uint64_t>(receivedFile.size()), 2ULL * 1024 * 1024);

        QString actualHash = Blake3Hasher::hashFile(receivedFilePath);
        QCOMPARE(actualHash, expectedHash);

        delete receiverSession;
    }
};

QTEST_MAIN(TestLoopbackTransfer)
#include "test_loopback_transfer.moc"
