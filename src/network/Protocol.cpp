#include "Protocol.h"
#include <QtEndian>
#include <QIODevice>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "../platform/PlatformFilesystem.h"

namespace FastTransfer {

QByteArray Protocol::createPacket(MessageType type, const QByteArray& payload, uint16_t flags) {
    ProtocolHeader header;
    header.magic = qToBigEndian(PROTOCOL_MAGIC);
    header.msgType = qToBigEndian(static_cast<uint16_t>(type));
    header.flags = qToBigEndian(flags);
    header.payloadLength = qToBigEndian(static_cast<uint64_t>(payload.size()));

    QByteArray packet;
    packet.resize(sizeof(ProtocolHeader) + payload.size());
    memcpy(packet.data(), &header, sizeof(ProtocolHeader));
    if (!payload.isEmpty()) {
        memcpy(packet.data() + sizeof(ProtocolHeader), payload.constData(), payload.size());
    }
    return packet;
}

// Handshake
QByteArray Protocol::serializeHandshakeReq(const MsgHandshakeReq& msg) {
    QJsonObject obj;
    obj["version"] = static_cast<int>(msg.version);
    obj["device"] = msg.deviceName;
    obj["os"] = msg.osType;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

std::optional<MsgHandshakeReq> Protocol::parseHandshakeReq(const QByteArray& data) {
    auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return std::nullopt;
    auto obj = doc.object();
    MsgHandshakeReq msg;
    msg.version = static_cast<uint32_t>(obj["version"].toInt(PROTOCOL_VERSION));
    msg.deviceName = obj["device"].toString();
    msg.osType = obj["os"].toString();
    return msg;
}

QByteArray Protocol::serializeHandshakeResp(const MsgHandshakeResp& msg) {
    QJsonObject obj;
    obj["version"] = static_cast<int>(msg.version);
    obj["device"] = msg.deviceName;
    obj["accepted"] = msg.accepted;
    obj["error"] = msg.errorMessage;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

std::optional<MsgHandshakeResp> Protocol::parseHandshakeResp(const QByteArray& data) {
    auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return std::nullopt;
    auto obj = doc.object();
    MsgHandshakeResp msg;
    msg.version = static_cast<uint32_t>(obj["version"].toInt(PROTOCOL_VERSION));
    msg.deviceName = obj["device"].toString();
    msg.accepted = obj["accepted"].toBool(false);
    msg.errorMessage = obj["error"].toString();
    return msg;
}

// Transfer Offer
QByteArray Protocol::serializeTransferOffer(const MsgTransferOffer& msg) {
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << msg.transferId;
    stream << msg.senderDevice;
    stream << static_cast<quint64>(msg.totalFiles);
    stream << static_cast<quint64>(msg.totalBytes);

    quint64 itemCount = static_cast<quint64>(msg.items.size());
    stream << itemCount;

    for (const auto& item : msg.items) {
        stream << static_cast<quint64>(item.fileIndex);
        stream << item.relativePath;
        stream << static_cast<quint64>(item.fileSize);
        stream << static_cast<quint64>(item.modifiedTime);
        stream << item.blake3Checksum;
    }

    return buffer;
}

std::optional<MsgTransferOffer> Protocol::parseTransferOffer(const QByteArray& data) {
    if (data.size() > MAX_METADATA_PAYLOAD_SIZE) return std::nullopt;

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    MsgTransferOffer msg;
    quint64 totalFiles = 0, totalBytes = 0, itemCount = 0;

    stream >> msg.transferId;
    stream >> msg.senderDevice;
    stream >> totalFiles;
    stream >> totalBytes;
    stream >> itemCount;

    if (stream.status() != QDataStream::Ok || itemCount > 1000000) {
        return std::nullopt;
    }

    msg.totalFiles = totalFiles;
    msg.totalBytes = totalBytes;
    msg.items.reserve(static_cast<qsizetype>(itemCount));

    for (quint64 i = 0; i < itemCount; ++i) {
        ManifestItem item;
        quint64 fileIdx = 0, fileSize = 0, modTime = 0;
        QString relPath;

        stream >> fileIdx;
        stream >> relPath;
        stream >> fileSize;
        stream >> modTime;
        stream >> item.blake3Checksum;

        if (stream.status() != QDataStream::Ok) {
            return std::nullopt;
        }

        // Validate relative path safety
        QString safePath = PlatformFilesystem::sanitizePath(relPath);
        if (safePath.isEmpty() && !relPath.isEmpty()) {
            return std::nullopt; // Insecure path rejected
        }

        item.fileIndex = fileIdx;
        item.relativePath = safePath;
        item.fileSize = fileSize;
        item.modifiedTime = modTime;

        msg.items.append(item);
    }

    return msg;
}

// Transfer Accept
QByteArray Protocol::serializeTransferAccept(const MsgTransferAccept& msg) {
    QJsonObject obj;
    obj["transfer_id"] = msg.transferId;
    obj["receiver_device"] = msg.receiverDevice;
    obj["accepted"] = msg.accepted;
    obj["start_file_index"] = static_cast<qint64>(msg.startFileIndex);
    obj["resume_offset"] = static_cast<qint64>(msg.resumeOffset);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

std::optional<MsgTransferAccept> Protocol::parseTransferAccept(const QByteArray& data) {
    auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return std::nullopt;
    auto obj = doc.object();

    MsgTransferAccept msg;
    msg.transferId = obj["transfer_id"].toString();
    msg.receiverDevice = obj["receiver_device"].toString();
    msg.accepted = obj["accepted"].toBool(false);
    msg.startFileIndex = static_cast<uint64_t>(obj["start_file_index"].toVariant().toULongLong());
    msg.resumeOffset = static_cast<uint64_t>(obj["resume_offset"].toVariant().toULongLong());
    return msg;
}

// Transfer Reject
QByteArray Protocol::serializeTransferReject(const MsgTransferReject& msg) {
    QJsonObject obj;
    obj["transfer_id"] = msg.transferId;
    obj["reason"] = msg.reason;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

std::optional<MsgTransferReject> Protocol::parseTransferReject(const QByteArray& data) {
    auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return std::nullopt;
    auto obj = doc.object();

    MsgTransferReject msg;
    msg.transferId = obj["transfer_id"].toString();
    msg.reason = obj["reason"].toString();
    return msg;
}

// File Start
QByteArray Protocol::serializeFileStart(const MsgFileStart& msg) {
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << msg.transferId;
    stream << static_cast<quint64>(msg.fileIndex);
    stream << msg.relativePath;
    stream << static_cast<quint64>(msg.fileSize);
    stream << static_cast<quint64>(msg.modifiedTime);
    stream << msg.expectedBlake3;
    return buffer;
}

std::optional<MsgFileStart> Protocol::parseFileStart(const QByteArray& data) {
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    MsgFileStart msg;
    quint64 fileIndex = 0, fileSize = 0, modifiedTime = 0;
    QString relPath;

    stream >> msg.transferId;
    stream >> fileIndex;
    stream >> relPath;
    stream >> fileSize;
    stream >> modifiedTime;
    stream >> msg.expectedBlake3;

    if (stream.status() != QDataStream::Ok) return std::nullopt;

    QString safe = PlatformFilesystem::sanitizePath(relPath);
    if (safe.isEmpty() && !relPath.isEmpty()) return std::nullopt;

    msg.fileIndex = fileIndex;
    msg.relativePath = safe;
    msg.fileSize = fileSize;
    msg.modifiedTime = modifiedTime;
    return msg;
}

// File Data Chunk: [fileIndex: 8B][offset: 8B][chunkSize: 4B][raw chunk bytes]
QByteArray Protocol::serializeFileDataChunk(const MsgFileDataChunk& msg) {
    uint32_t dataLen = static_cast<uint32_t>(msg.chunkData.size());
    QByteArray buffer;
    buffer.resize(20 + dataLen);

    uint64_t beFileIdx = qToBigEndian(msg.fileIndex);
    uint64_t beOffset = qToBigEndian(msg.offset);
    uint32_t beSize = qToBigEndian(dataLen);

    char* ptr = buffer.data();
    memcpy(ptr, &beFileIdx, 8);
    memcpy(ptr + 8, &beOffset, 8);
    memcpy(ptr + 16, &beSize, 4);
    if (dataLen > 0) {
        memcpy(ptr + 20, msg.chunkData.constData(), dataLen);
    }
    return buffer;
}

std::optional<MsgFileDataChunk> Protocol::parseFileDataChunk(const QByteArray& data) {
    if (data.size() < 20) return std::nullopt;

    const char* ptr = data.constData();
    uint64_t beFileIdx = 0, beOffset = 0;
    uint32_t beSize = 0;

    memcpy(&beFileIdx, ptr, 8);
    memcpy(&beOffset, ptr + 8, 8);
    memcpy(&beSize, ptr + 16, 4);

    MsgFileDataChunk msg;
    msg.fileIndex = qFromBigEndian(beFileIdx);
    msg.offset = qFromBigEndian(beOffset);
    msg.chunkSize = qFromBigEndian(beSize);

    if (20 + msg.chunkSize != static_cast<uint32_t>(data.size())) {
        return std::nullopt; // Corrupted chunk length
    }

    msg.chunkData = data.mid(20, msg.chunkSize);
    return msg;
}

// File End
QByteArray Protocol::serializeFileEnd(const MsgFileEnd& msg) {
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << static_cast<quint64>(msg.fileIndex);
    stream << msg.blake3Checksum;
    return buffer;
}

std::optional<MsgFileEnd> Protocol::parseFileEnd(const QByteArray& data) {
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    MsgFileEnd msg;
    quint64 fileIdx = 0;
    stream >> fileIdx;
    stream >> msg.blake3Checksum;

    if (stream.status() != QDataStream::Ok) return std::nullopt;
    msg.fileIndex = fileIdx;
    return msg;
}

// File Ack
QByteArray Protocol::serializeFileAck(const MsgFileAck& msg) {
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << static_cast<quint64>(msg.fileIndex);
    stream << static_cast<quint32>(msg.statusCode);
    stream << static_cast<quint64>(msg.bytesReceived);
    stream << msg.calculatedBlake3;
    return buffer;
}

std::optional<MsgFileAck> Protocol::parseFileAck(const QByteArray& data) {
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    MsgFileAck msg;
    quint64 fileIdx = 0, bytesReceived = 0;
    quint32 statusCode = 0;

    stream >> fileIdx;
    stream >> statusCode;
    stream >> bytesReceived;
    stream >> msg.calculatedBlake3;

    if (stream.status() != QDataStream::Ok) return std::nullopt;

    msg.fileIndex = fileIdx;
    msg.statusCode = statusCode;
    msg.bytesReceived = bytesReceived;
    return msg;
}

// Session Controls
QByteArray Protocol::serializeTransferPause(const MsgTransferPause& msg) {
    QJsonObject obj;
    obj["transfer_id"] = msg.transferId;
    obj["reason"] = msg.reason;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

std::optional<MsgTransferPause> Protocol::parseTransferPause(const QByteArray& data) {
    auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return std::nullopt;
    auto obj = doc.object();
    MsgTransferPause msg;
    msg.transferId = obj["transfer_id"].toString();
    msg.reason = obj["reason"].toString();
    return msg;
}

QByteArray Protocol::serializeTransferResume(const MsgTransferResume& msg) {
    QJsonObject obj;
    obj["transfer_id"] = msg.transferId;
    obj["file_index"] = static_cast<qint64>(msg.fileIndex);
    obj["resume_offset"] = static_cast<qint64>(msg.resumeOffset);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

std::optional<MsgTransferResume> Protocol::parseTransferResume(const QByteArray& data) {
    auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return std::nullopt;
    auto obj = doc.object();
    MsgTransferResume msg;
    msg.transferId = obj["transfer_id"].toString();
    msg.fileIndex = static_cast<uint64_t>(obj["file_index"].toVariant().toULongLong());
    msg.resumeOffset = static_cast<uint64_t>(obj["resume_offset"].toVariant().toULongLong());
    return msg;
}

QByteArray Protocol::serializeTransferCancel(const MsgTransferCancel& msg) {
    QJsonObject obj;
    obj["transfer_id"] = msg.transferId;
    obj["reason"] = msg.reason;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

std::optional<MsgTransferCancel> Protocol::parseTransferCancel(const QByteArray& data) {
    auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return std::nullopt;
    auto obj = doc.object();
    MsgTransferCancel msg;
    msg.transferId = obj["transfer_id"].toString();
    msg.reason = obj["reason"].toString();
    return msg;
}

QByteArray Protocol::serializeTransferComplete(const MsgTransferComplete& msg) {
    QJsonObject obj;
    obj["transfer_id"] = msg.transferId;
    obj["transferred_bytes"] = static_cast<qint64>(msg.totalBytesTransferred);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

std::optional<MsgTransferComplete> Protocol::parseTransferComplete(const QByteArray& data) {
    auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return std::nullopt;
    auto obj = doc.object();
    MsgTransferComplete msg;
    msg.transferId = obj["transfer_id"].toString();
    msg.totalBytesTransferred = static_cast<uint64_t>(obj["transferred_bytes"].toVariant().toULongLong());
    return msg;
}

QByteArray Protocol::serializeErrorAlert(const MsgErrorAlert& msg) {
    QJsonObject obj;
    obj["code"] = static_cast<int>(msg.errorCode);
    obj["msg"] = msg.message;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

std::optional<MsgErrorAlert> Protocol::parseErrorAlert(const QByteArray& data) {
    auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return std::nullopt;
    auto obj = doc.object();
    MsgErrorAlert msg;
    msg.errorCode = static_cast<uint32_t>(obj["code"].toInt(0));
    msg.message = obj["msg"].toString();
    return msg;
}

} // namespace FastTransfer
