#include <QTest>
#include "../src/network/Protocol.h"
#include "../src/platform/PlatformFilesystem.h"

using namespace FastTransfer;

class TestProtocol : public QObject {
    Q_OBJECT
private slots:
    void testHandshakeRoundtrip() {
        MsgHandshakeReq req;
        req.version = 1;
        req.deviceName = "Alpha-PC";
        req.osType = "Windows";

        QByteArray data = Protocol::serializeHandshakeReq(req);
        auto parsed = Protocol::parseHandshakeReq(data);

        QVERIFY(parsed.has_value());
        QCOMPARE(parsed->version, 1u);
        QCOMPARE(parsed->deviceName, QString("Alpha-PC"));
        QCOMPARE(parsed->osType, QString("Windows"));
    }

    void testPacketFraming() {
        QByteArray payload = "HelloWorldData12345";
        QByteArray packet = Protocol::createPacket(MessageType::HandshakeReq, payload);

        QCOMPARE(packet.size(), static_cast<qsizetype>(sizeof(ProtocolHeader) + payload.size()));

        ProtocolHeader header;
        memcpy(&header, packet.constData(), sizeof(ProtocolHeader));
        QCOMPARE(qFromBigEndian(header.magic), PROTOCOL_MAGIC);
        QCOMPARE(qFromBigEndian(header.msgType), static_cast<uint16_t>(MessageType::HandshakeReq));
        QCOMPARE(qFromBigEndian(header.payloadLength), static_cast<uint64_t>(payload.size()));
    }

    void testChunkSerialization() {
        MsgFileDataChunk chunk;
        chunk.fileIndex = 42;
        chunk.offset = 1048576;
        chunk.chunkSize = 5;
        chunk.chunkData = "CHUNK";

        QByteArray data = Protocol::serializeFileDataChunk(chunk);
        auto parsed = Protocol::parseFileDataChunk(data);

        QVERIFY(parsed.has_value());
        QCOMPARE(parsed->fileIndex, 42ULL);
        QCOMPARE(parsed->offset, 1048576ULL);
        QCOMPARE(parsed->chunkSize, 5u);
        QCOMPARE(parsed->chunkData, QByteArray("CHUNK"));
    }

    void testMaliciousPathRejection() {
        QVERIFY(!PlatformFilesystem::isSafeRelativePath("../../Windows/System32/calc.exe"));
        QVERIFY(!PlatformFilesystem::isSafeRelativePath("/etc/passwd"));
        QVERIFY(!PlatformFilesystem::isSafeRelativePath("C:\\Windows\\System32\\cmd.exe"));
        QVERIFY(!PlatformFilesystem::isSafeRelativePath("\\\\server\\share\\evil.bat"));

        QString sanitized = PlatformFilesystem::sanitizePath("../../Windows/System32/calc.exe");
        QVERIFY(!sanitized.contains(".."));
        QCOMPARE(sanitized, QString("Windows/System32/calc.exe"));
    }
};

QTEST_MAIN(TestProtocol)
#include "test_protocol.moc"
