#ifndef FASTTRANSFER_TRANSFER_SESSION_H
#define FASTTRANSFER_TRANSFER_SESSION_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <memory>
#include "TransferTypes.h"
#include "../network/TcpConnection.h"
#include "../network/Protocol.h"
#include "../filesystem/FileEnumerator.h"
#include "../filesystem/FileReader.h"
#include "../filesystem/FileWriter.h"

namespace FastTransfer {

class TransferSession : public QObject {
    Q_OBJECT
public:
    explicit TransferSession(TransferDirection direction, QObject* parent = nullptr);
    explicit TransferSession(TcpConnection* connection, QObject* parent = nullptr);
    ~TransferSession() override;

    TransferDirection direction() const { return m_direction; }
    TransferStatus status() const { return m_status; }
    QString transferId() const { return m_transferId; }
    QString remoteDevice() const { return m_remoteDevice; }
    QString sessionSubfolder() const { return m_sessionSubfolder; }
    QHostAddress remoteIp() const;
    TransferMetrics metrics() const { return m_metrics; }

    // Sender API
    void startSend(const QHostAddress& targetAddress,
                   uint16_t port,
                   const FileManifest& manifest,
                   const QString& localDeviceName,
                   uint32_t chunkSize = DEFAULT_CHUNK_SIZE);

    // Receiver API
    void acceptTransfer(const QString& baseOutputDir, DuplicatePolicy policy = DuplicatePolicy::Rename);
    void rejectTransfer(const QString& reason = QStringLiteral("User declined transfer"));

    // Session controls
    void pause();
    void resume();
    void cancel();

signals:
    void statusChanged(TransferStatus status);
    void metricsUpdated(const TransferMetrics& metrics);
    void speedSampleRecorded(double speedBps);
    void incomingOfferReceived(const QString& transferId, const QString& senderDevice, uint64_t totalFiles, uint64_t totalBytes);
    void fileStarted(uint64_t fileIndex, const QString& relativePath, uint64_t fileSize);
    void fileCompleted(uint64_t fileIndex, const QString& relativePath, bool verified);
    void transferCompleted();
    void transferFailed(const QString& errorMessage);

private slots:
    void onPacketReceived(MessageType type, uint16_t flags, const QByteArray& payload);
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(const QString& errorMsg);
    void onBytesTransferred(qint64 bytes);
    void calculateMetrics();
    void sendNextChunk();

private:
    void setStatus(TransferStatus newStatus);
    void startSendingFile(uint64_t fileIndex, uint64_t startOffset = 0);
    void finishCurrentFileSend();

    TransferDirection m_direction;
    TransferStatus m_status = TransferStatus::Idle;
    TcpConnection* m_connection = nullptr;
    bool m_ownsConnection = true;

    QString m_transferId;
    QString m_localDeviceName;
    QString m_remoteDevice;

    // Sender state
    FileManifest m_sendManifest;
    std::unique_ptr<FileReader> m_fileReader;
    uint32_t m_chunkSize = DEFAULT_CHUNK_SIZE;
    uint64_t m_currentFileIndex = 0;
    bool m_waitingFileAck = false;

    // Receiver state
    std::optional<MsgTransferOffer> m_receivedOffer;
    std::unique_ptr<FileWriter> m_fileWriter;
    QString m_baseOutputDir;
    QString m_sessionSubfolder;
    DuplicatePolicy m_duplicatePolicy = DuplicatePolicy::Rename;
    uint64_t m_currentReceivingFileIndex = 0;
    uint64_t m_expectedFileSize = 0;
    QString m_expectedFileChecksum;

    // Telemetry & metrics
    TransferMetrics m_metrics;
    QTimer* m_metricsTimer = nullptr;
    QElapsedTimer m_sessionTimer;
    QElapsedTimer m_sampleTimer;
    uint64_t m_sessionStartBytes = 0;
    qint64 m_lastBytesCount = 0;
    double m_smoothedSpeed = 0.0;
    double m_peakSpeed = 0.0;
    int m_consecutiveZeroTicks = 0;
};

} // namespace FastTransfer

#endif // FASTTRANSFER_TRANSFER_SESSION_H
