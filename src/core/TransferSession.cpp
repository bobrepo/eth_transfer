#include "TransferSession.h"
#include <QUuid>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <algorithm>
#include "../platform/PlatformFilesystem.h"

namespace FastTransfer {

TransferSession::TransferSession(TransferDirection direction, QObject* parent)
    : QObject(parent)
    , m_direction(direction)
    , m_ownsConnection(true)
    , m_transferId(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
    m_connection = new TcpConnection(this);
    m_metricsTimer = new QTimer(this);

    connect(m_connection, &TcpConnection::packetReceived, this, &TransferSession::onPacketReceived);
    connect(m_connection, &TcpConnection::connected, this, &TransferSession::onSocketConnected);
    connect(m_connection, &TcpConnection::disconnected, this, &TransferSession::onSocketDisconnected);
    connect(m_connection, &TcpConnection::errorOccurred, this, &TransferSession::onSocketError);
    connect(m_connection, &TcpConnection::bytesTransferred, this, &TransferSession::onBytesTransferred);

    connect(m_metricsTimer, &QTimer::timeout, this, &TransferSession::calculateMetrics);
}

TransferSession::TransferSession(TcpConnection* connection, QObject* parent)
    : QObject(parent)
    , m_direction(TransferDirection::Receive)
    , m_connection(connection)
    , m_ownsConnection(false)
    , m_transferId(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
    m_connection->setParent(this);
    m_metricsTimer = new QTimer(this);

    connect(m_connection, &TcpConnection::packetReceived, this, &TransferSession::onPacketReceived);
    connect(m_connection, &TcpConnection::disconnected, this, &TransferSession::onSocketDisconnected);
    connect(m_connection, &TcpConnection::errorOccurred, this, &TransferSession::onSocketError);
    connect(m_connection, &TcpConnection::bytesTransferred, this, &TransferSession::onBytesTransferred);

    connect(m_metricsTimer, &QTimer::timeout, this, &TransferSession::calculateMetrics);
    setStatus(TransferStatus::Handshaking);
}

TransferSession::~TransferSession() {
    if (m_metricsTimer) m_metricsTimer->stop();
    if (m_connection && m_ownsConnection) {
        m_connection->disconnectFromHost();
    }
}

QHostAddress TransferSession::remoteIp() const {
    return m_connection ? m_connection->peerAddress() : QHostAddress();
}

void TransferSession::setStatus(TransferStatus newStatus) {
    if (m_status != newStatus) {
        m_status = newStatus;
        emit statusChanged(m_status);
    }
}

// ----------------- Sender Implementation -----------------

void TransferSession::startSend(const QHostAddress& targetAddress,
                               uint16_t port,
                               const FileManifest& manifest,
                               const QString& localDeviceName,
                               uint32_t chunkSize) {
    m_sendManifest = manifest;
    m_localDeviceName = localDeviceName;
    m_chunkSize = chunkSize > 0 ? chunkSize : DEFAULT_CHUNK_SIZE;
    m_currentFileIndex = 0;

    m_metrics = TransferMetrics();
    m_metrics.totalFiles = manifest.totalFiles;
    m_metrics.totalBytes = manifest.totalBytes;

    setStatus(TransferStatus::Connecting);
    m_connection->connectToHost(targetAddress, port);
}

void TransferSession::startSendingFile(uint64_t fileIndex, uint64_t startOffset) {
    if (fileIndex >= static_cast<uint64_t>(m_sendManifest.items.size())) {
        // All files sent!
        MsgTransferComplete completeMsg;
        completeMsg.transferId = m_transferId;
        completeMsg.totalBytesTransferred = m_metrics.totalTransferred;
        m_connection->sendPacket(MessageType::TransferComplete, Protocol::serializeTransferComplete(completeMsg));
        setStatus(TransferStatus::Completed);
        emit transferCompleted();
        return;
    }

    m_currentFileIndex = fileIndex;
    const auto& item = m_sendManifest.items[static_cast<qsizetype>(fileIndex)];

    m_metrics.currentFileIndex = fileIndex + 1;
    m_metrics.currentFileName = item.relativePath;
    m_metrics.currentFileSize = item.fileSize;
    m_metrics.currentFileTransferred = startOffset;

    emit fileStarted(fileIndex, item.relativePath, item.fileSize);

    m_fileReader = std::make_unique<FileReader>(m_chunkSize);
    if (!m_fileReader->open(item.sourceAbsolutePath, startOffset)) {
        setStatus(TransferStatus::Failed);
        emit transferFailed(QString("Failed to open source file: %1").arg(m_fileReader->errorString()));
        return;
    }

    // Send FileStart packet
    MsgFileStart startMsg;
    startMsg.transferId = m_transferId;
    startMsg.fileIndex = fileIndex;
    startMsg.relativePath = item.relativePath;
    startMsg.fileSize = item.fileSize;
    startMsg.modifiedTime = item.modifiedTime;
    startMsg.expectedBlake3 = item.blake3Checksum;

    m_connection->sendPacket(MessageType::FileStart, Protocol::serializeFileStart(startMsg));

    m_waitingFileAck = false;
    sendNextChunk();
}

void TransferSession::sendNextChunk() {
    if (m_status != TransferStatus::Transferring || m_waitingFileAck || !m_fileReader) {
        return;
    }

    qint64 maxPending = std::max<qint64>(static_cast<qint64>(m_chunkSize * 2), 16 * 1024 * 1024);

    // Flow control: keep socket write buffer managed
    if (m_connection->socket() && m_connection->socket()->bytesToWrite() >= maxPending) {
        // Will be triggered by bytesWritten slot via onBytesTransferred
        return;
    }

    if (m_fileReader->isAtEnd()) {
        // Finish file
        m_waitingFileAck = true;
        MsgFileEnd endMsg;
        endMsg.fileIndex = m_currentFileIndex;
        endMsg.blake3Checksum = m_fileReader->finalizeBlake3();
        m_connection->sendPacket(MessageType::FileEnd, Protocol::serializeFileEnd(endMsg));
        return;
    }

    uint64_t chunkOffset = m_fileReader->currentOffset();
    QByteArray chunk = m_fileReader->readNextChunk();

    if (chunk.isEmpty() && !m_fileReader->isAtEnd()) {
        setStatus(TransferStatus::Failed);
        emit transferFailed(QString("Error reading file: %1").arg(m_fileReader->errorString()));
        return;
    }

    MsgFileDataChunk chunkMsg;
    chunkMsg.fileIndex = m_currentFileIndex;
    chunkMsg.offset = chunkOffset;
    chunkMsg.chunkSize = static_cast<uint32_t>(chunk.size());
    chunkMsg.chunkData = chunk;

    bool sent = m_connection->sendPacket(MessageType::FileDataChunk, Protocol::serializeFileDataChunk(chunkMsg));
    if (!sent) {
        setStatus(TransferStatus::Failed);
        emit transferFailed(QStringLiteral("Network socket error: Failed to transmit data chunk"));
        return;
    }

    m_metrics.currentFileTransferred += static_cast<uint64_t>(chunk.size());
    m_metrics.totalTransferred += static_cast<uint64_t>(chunk.size());

    // Schedule next chunk execution asynchronously if buffer has room
    if (!m_connection->socket() || m_connection->socket()->bytesToWrite() < maxPending) {
        QMetaObject::invokeMethod(this, "sendNextChunk", Qt::QueuedConnection);
    }
}

// ----------------- Receiver Implementation -----------------

void TransferSession::acceptTransfer(const QString& baseOutputDir, DuplicatePolicy policy) {
    m_baseOutputDir = baseOutputDir;
    m_duplicatePolicy = policy;

    if (m_sessionSubfolder.isEmpty()) {
        QString cleanSender = PlatformFilesystem::sanitizeDeviceName(m_remoteDevice);
        if (cleanSender.isEmpty()) cleanSender = QStringLiteral("Sender");
        QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
        QString baseName = QStringLiteral("%1_%2").arg(cleanSender, timestamp);

        // Ensure folder uniqueness if multiple transfers initiated in the same second
        QDir baseDir(baseOutputDir);
        QString candidate = baseName;
        int counter = 1;
        while (baseDir.exists(candidate)) {
            candidate = QStringLiteral("%1_%2").arg(baseName, QString::number(counter++));
        }
        m_sessionSubfolder = candidate;
    }

    if (!m_receivedOffer) return;

    MsgTransferAccept acceptMsg;
    acceptMsg.transferId = m_transferId;
    acceptMsg.receiverDevice = m_localDeviceName;
    acceptMsg.accepted = true;
    acceptMsg.startFileIndex = 0;
    acceptMsg.resumeOffset = 0;

    m_connection->sendPacket(MessageType::TransferAccept, Protocol::serializeTransferAccept(acceptMsg));

    m_sessionTimer.restart();
    m_lastBytesCount = 0;
    m_metricsTimer->start(250); // Telemetry update every 250ms
    setStatus(TransferStatus::Transferring);
}

void TransferSession::rejectTransfer(const QString& reason) {
    MsgTransferReject rejectMsg;
    rejectMsg.transferId = m_transferId;
    rejectMsg.reason = reason;
    m_connection->sendPacket(MessageType::TransferReject, Protocol::serializeTransferReject(rejectMsg));

    setStatus(TransferStatus::Cancelled);
    m_connection->disconnectFromHost();
}

// ----------------- Controls -----------------

void TransferSession::pause() {
    if (m_status == TransferStatus::Transferring) {
        setStatus(TransferStatus::Paused);
        MsgTransferPause pauseMsg;
        pauseMsg.transferId = m_transferId;
        pauseMsg.reason = QStringLiteral("User paused transfer");
        m_connection->sendPacket(MessageType::TransferPause, Protocol::serializeTransferPause(pauseMsg));
    }
}

void TransferSession::resume() {
    if (m_status == TransferStatus::Paused) {
        setStatus(TransferStatus::Transferring);
        MsgTransferResume resumeMsg;
        resumeMsg.transferId = m_transferId;
        resumeMsg.fileIndex = m_currentFileIndex;
        resumeMsg.resumeOffset = m_metrics.currentFileTransferred;
        m_connection->sendPacket(MessageType::TransferResume, Protocol::serializeTransferResume(resumeMsg));

        if (m_direction == TransferDirection::Send) {
            sendNextChunk();
        }
    }
}

void TransferSession::cancel() {
    setStatus(TransferStatus::Cancelled);
    MsgTransferCancel cancelMsg;
    cancelMsg.transferId = m_transferId;
    cancelMsg.reason = QStringLiteral("User cancelled transfer");
    m_connection->sendPacket(MessageType::TransferCancel, Protocol::serializeTransferCancel(cancelMsg));

    if (m_fileWriter) {
        m_fileWriter->abort();
    }
    if (m_fileReader) {
        m_fileReader->close();
    }

    m_connection->disconnectFromHost();
}

// ----------------- Packet Handling -----------------

void TransferSession::onPacketReceived(MessageType type, uint16_t flags, const QByteArray& payload) {
    Q_UNUSED(flags);

    switch (type) {
        case MessageType::HandshakeReq: {
            auto req = Protocol::parseHandshakeReq(payload);
            if (req) {
                m_remoteDevice = req->deviceName;
                MsgHandshakeResp resp;
                resp.version = PROTOCOL_VERSION;
                resp.deviceName = m_localDeviceName;
                resp.accepted = true;
                m_connection->sendPacket(MessageType::HandshakeResp, Protocol::serializeHandshakeResp(resp));
                setStatus(TransferStatus::Offering);
            }
            break;
        }

        case MessageType::HandshakeResp: {
            auto resp = Protocol::parseHandshakeResp(payload);
            if (resp && resp->accepted) {
                m_remoteDevice = resp->deviceName;
                setStatus(TransferStatus::Offering);

                // Send Transfer Offer
                MsgTransferOffer offer;
                offer.transferId = m_transferId;
                offer.senderDevice = m_localDeviceName;
                offer.totalFiles = m_sendManifest.totalFiles;
                offer.totalBytes = m_sendManifest.totalBytes;
                offer.items = m_sendManifest.toProtocolItems();
                m_connection->sendPacket(MessageType::TransferOffer, Protocol::serializeTransferOffer(offer));
                setStatus(TransferStatus::WaitingApproval);
            } else {
                setStatus(TransferStatus::Failed);
                emit transferFailed(resp ? resp->errorMessage : QStringLiteral("Handshake rejected"));
            }
            break;
        }

        case MessageType::TransferOffer: {
            auto offer = Protocol::parseTransferOffer(payload);
            if (offer) {
                m_receivedOffer = offer;
                m_transferId = offer->transferId;
                m_remoteDevice = offer->senderDevice;
                m_metrics.totalFiles = offer->totalFiles;
                m_metrics.totalBytes = offer->totalBytes;
                setStatus(TransferStatus::WaitingApproval);

                emit incomingOfferReceived(offer->transferId, offer->senderDevice, offer->totalFiles, offer->totalBytes);
            }
            break;
        }

        case MessageType::TransferAccept: {
            auto accept = Protocol::parseTransferAccept(payload);
            if (accept && accept->accepted) {
                setStatus(TransferStatus::Transferring);
                m_sessionTimer.restart();
                m_lastBytesCount = 0;
                m_metricsTimer->start(250);
                startSendingFile(accept->startFileIndex, accept->resumeOffset);
            }
            break;
        }

        case MessageType::TransferReject: {
            auto reject = Protocol::parseTransferReject(payload);
            setStatus(TransferStatus::Cancelled);
            emit transferFailed(reject ? reject->reason : QStringLiteral("Transfer rejected by receiver"));
            break;
        }

        case MessageType::FileStart: {
            auto start = Protocol::parseFileStart(payload);
            if (start) {
                m_currentReceivingFileIndex = start->fileIndex;
                m_expectedFileSize = start->fileSize;
                m_expectedFileChecksum = start->expectedBlake3;

                m_metrics.currentFileIndex = start->fileIndex + 1;
                m_metrics.currentFileName = start->relativePath;
                m_metrics.currentFileSize = start->fileSize;
                m_metrics.currentFileTransferred = 0;

                emit fileStarted(start->fileIndex, start->relativePath, start->fileSize);

                QString subFolder = m_sessionSubfolder.isEmpty() ? m_remoteDevice : m_sessionSubfolder;
                m_fileWriter = std::make_unique<FileWriter>();
                if (!m_fileWriter->open(m_baseOutputDir, subFolder, start->relativePath,
                                        start->fileSize, 0, m_duplicatePolicy)) {
                    MsgFileAck ack;
                    ack.fileIndex = start->fileIndex;
                    ack.statusCode = 2; // Write error
                    m_connection->sendPacket(MessageType::FileAck, Protocol::serializeFileAck(ack));
                    setStatus(TransferStatus::Failed);
                    emit transferFailed(QString("Failed to prepare destination file: %1").arg(m_fileWriter->errorString()));
                    m_connection->disconnectFromHost();
                    return;
                }
            }
            break;
        }

        case MessageType::FileDataChunk: {
            auto chunk = Protocol::parseFileDataChunk(payload);
            if (chunk && m_fileWriter) {
                bool ok = m_fileWriter->writeChunk(chunk->offset, chunk->chunkData);
                if (ok) {
                    m_metrics.currentFileTransferred += static_cast<uint64_t>(chunk->chunkData.size());
                    m_metrics.totalTransferred += static_cast<uint64_t>(chunk->chunkData.size());
                } else {
                    setStatus(TransferStatus::Failed);
                    emit transferFailed(QString("Disk write error: %1").arg(m_fileWriter->errorString()));
                    m_connection->disconnectFromHost();
                    return;
                }
            }
            break;
        }

        case MessageType::FileEnd: {
            auto end = Protocol::parseFileEnd(payload);
            if (end && m_fileWriter) {
                bool verified = m_fileWriter->finalizeAndCommit(end->blake3Checksum);

                MsgFileAck ack;
                ack.fileIndex = end->fileIndex;
                ack.statusCode = verified ? 0 : 1; // 0=OK, 1=Mismatch
                ack.bytesReceived = m_fileWriter->bytesWritten();
                ack.calculatedBlake3 = m_fileWriter->calculatedChecksum();

                m_connection->sendPacket(MessageType::FileAck, Protocol::serializeFileAck(ack));
                emit fileCompleted(end->fileIndex, m_metrics.currentFileName, verified);
            }
            break;
        }

        case MessageType::FileAck: {
            auto ack = Protocol::parseFileAck(payload);
            if (ack) {
                bool ok = (ack->statusCode == 0);
                emit fileCompleted(ack->fileIndex, m_metrics.currentFileName, ok);

                if (ok) {
                    m_fileReader->close();
                    startSendingFile(m_currentFileIndex + 1, 0);
                } else {
                    setStatus(TransferStatus::Failed);
                    emit transferFailed(QString("File verification failed for file index %1").arg(ack->fileIndex));
                }
            }
            break;
        }

        case MessageType::TransferPause: {
            setStatus(TransferStatus::Paused);
            break;
        }

        case MessageType::TransferResume: {
            auto resumeMsg = Protocol::parseTransferResume(payload);
            if (resumeMsg) {
                setStatus(TransferStatus::Transferring);
                if (m_direction == TransferDirection::Send) {
                    startSendingFile(resumeMsg->fileIndex, resumeMsg->resumeOffset);
                }
            }
            break;
        }

        case MessageType::TransferCancel: {
            setStatus(TransferStatus::Cancelled);
            if (m_fileWriter) m_fileWriter->abort();
            if (m_fileReader) m_fileReader->close();
            break;
        }

        case MessageType::TransferComplete: {
            setStatus(TransferStatus::Completed);
            emit transferCompleted();
            break;
        }

        default:
            break;
    }
}

void TransferSession::onSocketConnected() {
    setStatus(TransferStatus::Handshaking);

    // Send Handshake Request
    MsgHandshakeReq req;
    req.version = PROTOCOL_VERSION;
    req.deviceName = m_localDeviceName;
#ifdef Q_OS_WIN
    req.osType = "Windows";
#elif defined(Q_OS_LINUX)
    req.osType = "Linux";
#else
    req.osType = "macOS";
#endif
    m_connection->sendPacket(MessageType::HandshakeReq, Protocol::serializeHandshakeReq(req));
}

void TransferSession::onSocketDisconnected() {
    if (m_status == TransferStatus::Transferring) {
        setStatus(TransferStatus::Failed);
        emit transferFailed(QStringLiteral("Connection to remote host was lost unexpectedly. Partial transfer preserved for resume."));
    }
}

void TransferSession::onSocketError(const QString& errorMsg) {
    if (m_status == TransferStatus::Transferring || m_status == TransferStatus::Connecting) {
        setStatus(TransferStatus::Failed);
        emit transferFailed(errorMsg);
    }
}

void TransferSession::onBytesTransferred(qint64 bytes) {
    Q_UNUSED(bytes);
    if (m_direction == TransferDirection::Send && m_status == TransferStatus::Transferring && !m_waitingFileAck) {
        qint64 maxPending = std::max<qint64>(static_cast<qint64>(m_chunkSize * 2), 16 * 1024 * 1024);
        if (!m_connection->socket() || m_connection->socket()->bytesToWrite() < maxPending) {
            sendNextChunk();
        }
    }
}

void TransferSession::calculateMetrics() {
    if (!m_sessionTimer.isValid()) return;

    qint64 elapsedMs = m_sessionTimer.elapsed();
    m_metrics.elapsedSeconds = elapsedMs / 1000;

    // For sender, measuring real bytes transmitted by socket reflects true wire speed
    uint64_t currentTotal = (m_direction == TransferDirection::Send && m_connection)
                                ? m_connection->totalBytesSent()
                                : m_metrics.totalTransferred;
    uint64_t deltaBytes = (currentTotal >= static_cast<uint64_t>(m_lastBytesCount))
                              ? (currentTotal - static_cast<uint64_t>(m_lastBytesCount))
                              : 0;
    m_lastBytesCount = static_cast<qint64>(currentTotal);

    // 250ms interval => multiply delta by 4 for bytes/sec
    double instantSpeed = static_cast<double>(deltaBytes) * 4.0;

    // Exponential Moving Average (EMA) smoothing: 70% previous, 30% instant
    if (m_smoothedSpeed <= 0.0) {
        m_smoothedSpeed = instantSpeed;
    } else {
        m_smoothedSpeed = 0.70 * m_smoothedSpeed + 0.30 * instantSpeed;
    }

    m_peakSpeed = std::max(m_peakSpeed, instantSpeed);

    double averageSpeed = 0.0;
    if (elapsedMs > 500) {
        averageSpeed = static_cast<double>(currentTotal) / (static_cast<double>(elapsedMs) / 1000.0);
    }

    m_metrics.currentSpeedBps = m_smoothedSpeed;
    m_metrics.averageSpeedBps = averageSpeed;
    m_metrics.peakSpeedBps = m_peakSpeed;

    // ETA calculation
    if (m_metrics.totalBytes > currentTotal && m_smoothedSpeed > 1024.0) {
        uint64_t remainingBytes = m_metrics.totalBytes - currentTotal;
        m_metrics.etaSeconds = static_cast<int64_t>(static_cast<double>(remainingBytes) / m_smoothedSpeed);
    } else if (currentTotal >= m_metrics.totalBytes && m_metrics.totalBytes > 0) {
        m_metrics.etaSeconds = 0;
    } else {
        m_metrics.etaSeconds = -1; // Calculating...
    }

    emit metricsUpdated(m_metrics);
    emit speedSampleRecorded(instantSpeed);
}

} // namespace FastTransfer
