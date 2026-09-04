#ifndef FASTTRANSFER_PROTOCOL_H
#define FASTTRANSFER_PROTOCOL_H

#include <QString>
#include <QByteArray>
#include <QList>
#include <cstdint>
#include <optional>

namespace FastTransfer {

constexpr uint32_t PROTOCOL_MAGIC = 0x46543031; // 'FT01'
constexpr uint32_t PROTOCOL_VERSION = 1;
constexpr uint16_t DEFAULT_TCP_PORT = 45821;
constexpr uint16_t DEFAULT_DISCOVERY_PORT = 45820;

// Maximum payload safeguards
constexpr uint64_t MAX_METADATA_PAYLOAD_SIZE = 64 * 1024 * 1024; // 64 MB max for manifests
constexpr uint32_t MAX_CHUNK_SIZE = 64 * 1024 * 1024;            // 64 MB max per data chunk
constexpr uint32_t DEFAULT_CHUNK_SIZE = 8 * 1024 * 1024;         // 8 MB default chunk

enum class MessageType : uint16_t {
    Invalid          = 0x0000,
    HandshakeReq     = 0x0001,
    HandshakeResp    = 0x0002,
    TransferOffer    = 0x0010,
    TransferAccept   = 0x0011,
    TransferReject   = 0x0012,
    FileStart        = 0x0020,
    FileDataChunk    = 0x0021,
    FileEnd          = 0x0022,
    FileAck          = 0x0023,
    TransferPause    = 0x0030,
    TransferResume   = 0x0031,
    TransferCancel   = 0x0032,
    TransferComplete = 0x0033,
    ErrorAlert       = 0x00FF
};

#pragma pack(push, 1)
struct ProtocolHeader {
    uint32_t magic = 0;       // 'FT01'
    uint16_t msgType = 0;     // MessageType
    uint16_t flags = 0;       // Flags (compression, etc.)
    uint64_t payloadLength = 0; // Length in bytes of following payload
};
#pragma pack(pop)

static_assert(sizeof(ProtocolHeader) == 16, "ProtocolHeader must be exactly 16 bytes");

struct ManifestItem {
    uint64_t fileIndex = 0;
    QString relativePath;
    uint64_t fileSize = 0;
    uint64_t modifiedTime = 0;
    QString blake3Checksum;
};

// Handshake
struct MsgHandshakeReq {
    uint32_t version = PROTOCOL_VERSION;
    QString deviceName;
    QString osType;
};

struct MsgHandshakeResp {
    uint32_t version = PROTOCOL_VERSION;
    QString deviceName;
    bool accepted = true;
    QString errorMessage;
};

// Transfer Offer / Accept / Reject
struct MsgTransferOffer {
    QString transferId;
    QString senderDevice;
    uint64_t totalFiles = 0;
    uint64_t totalBytes = 0;
    QList<ManifestItem> items;
};

struct MsgTransferAccept {
    QString transferId;
    QString receiverDevice;
    bool accepted = true;
    uint64_t startFileIndex = 0;
    uint64_t resumeOffset = 0;
};

struct MsgTransferReject {
    QString transferId;
    QString reason;
};

// File Transfer
struct MsgFileStart {
    QString transferId;
    uint64_t fileIndex = 0;
    QString relativePath;
    uint64_t fileSize = 0;
    uint64_t modifiedTime = 0;
    QString expectedBlake3;
};

struct MsgFileDataChunk {
    uint64_t fileIndex = 0;
    uint64_t offset = 0;
    uint32_t chunkSize = 0;
    QByteArray chunkData;
};

struct MsgFileEnd {
    uint64_t fileIndex = 0;
    QString blake3Checksum;
};

struct MsgFileAck {
    uint64_t fileIndex = 0;
    uint32_t statusCode = 0; // 0 = OK, 1 = Hash Mismatch, 2 = Write Error
    uint64_t bytesReceived = 0;
    QString calculatedBlake3;
};

// Session Controls
struct MsgTransferPause {
    QString transferId;
    QString reason;
};

struct MsgTransferResume {
    QString transferId;
    uint64_t fileIndex = 0;
    uint64_t resumeOffset = 0;
};

struct MsgTransferCancel {
    QString transferId;
    QString reason;
};

struct MsgTransferComplete {
    QString transferId;
    uint64_t totalBytesTransferred = 0;
};

struct MsgErrorAlert {
    uint32_t errorCode = 0;
    QString message;
};

class Protocol {
public:
    // Encode packet framing
    static QByteArray createPacket(MessageType type, const QByteArray& payload, uint16_t flags = 0);

    // Serialization functions
    static QByteArray serializeHandshakeReq(const MsgHandshakeReq& msg);
    static QByteArray serializeHandshakeResp(const MsgHandshakeResp& msg);
    static QByteArray serializeTransferOffer(const MsgTransferOffer& msg);
    static QByteArray serializeTransferAccept(const MsgTransferAccept& msg);
    static QByteArray serializeTransferReject(const MsgTransferReject& msg);
    static QByteArray serializeFileStart(const MsgFileStart& msg);
    static QByteArray serializeFileDataChunk(const MsgFileDataChunk& msg);
    static QByteArray serializeFileEnd(const MsgFileEnd& msg);
    static QByteArray serializeFileAck(const MsgFileAck& msg);
    static QByteArray serializeTransferPause(const MsgTransferPause& msg);
    static QByteArray serializeTransferResume(const MsgTransferResume& msg);
    static QByteArray serializeTransferCancel(const MsgTransferCancel& msg);
    static QByteArray serializeTransferComplete(const MsgTransferComplete& msg);
    static QByteArray serializeErrorAlert(const MsgErrorAlert& msg);

    // Deserialization functions
    static std::optional<MsgHandshakeReq> parseHandshakeReq(const QByteArray& data);
    static std::optional<MsgHandshakeResp> parseHandshakeResp(const QByteArray& data);
    static std::optional<MsgTransferOffer> parseTransferOffer(const QByteArray& data);
    static std::optional<MsgTransferAccept> parseTransferAccept(const QByteArray& data);
    static std::optional<MsgTransferReject> parseTransferReject(const QByteArray& data);
    static std::optional<MsgFileStart> parseFileStart(const QByteArray& data);
    static std::optional<MsgFileDataChunk> parseFileDataChunk(const QByteArray& data);
    static std::optional<MsgFileEnd> parseFileEnd(const QByteArray& data);
    static std::optional<MsgFileAck> parseFileAck(const QByteArray& data);
    static std::optional<MsgTransferPause> parseTransferPause(const QByteArray& data);
    static std::optional<MsgTransferResume> parseTransferResume(const QByteArray& data);
    static std::optional<MsgTransferCancel> parseTransferCancel(const QByteArray& data);
    static std::optional<MsgTransferComplete> parseTransferComplete(const QByteArray& data);
    static std::optional<MsgErrorAlert> parseErrorAlert(const QByteArray& data);
};

} // namespace FastTransfer

#endif // FASTTRANSFER_PROTOCOL_H
