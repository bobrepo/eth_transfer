#ifndef FASTTRANSFER_TRANSFER_MANAGER_H
#define FASTTRANSFER_TRANSFER_MANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QStringList>
#include <memory>
#include "TransferSession.h"
#include "TransferTypes.h"
#include "../network/DiscoveryService.h"
#include "../database/Database.h"

namespace FastTransfer {

class TransferManager : public QObject {
    Q_OBJECT
public:
    explicit TransferManager(QObject* parent = nullptr);
    ~TransferManager() override;

    bool initialize();
    void shutdown();

    // Configuration
    QString deviceName() const { return m_deviceName; }
    void setDeviceName(const QString& name);

    QString defaultDownloadDir() const { return m_downloadDir; }
    void setDefaultDownloadDir(const QString& dir);

    DuplicatePolicy duplicatePolicy() const { return m_duplicatePolicy; }
    void setDuplicatePolicy(DuplicatePolicy policy);

    bool isEthernetOnly() const { return m_ethernetOnly; }
    void setEthernetOnly(bool ethernetOnly);

    uint32_t chunkSize() const { return m_chunkSize; }
    void setChunkSize(uint32_t bytes);

    bool autoAccept() const { return m_autoAccept; }
    void setAutoAccept(bool enabled);

    // Active session access
    TransferSession* activeSession() { return m_activeSession.get(); }
    DiscoveryService* discoveryService() { return m_discoveryService; }
    Database* database() { return m_database; }

    // Initiate sending files/folders
    TransferSession* initiateSend(const QHostAddress& targetIp,
                                  uint16_t port,
                                  const QStringList& fileOrFolderPaths);

    // Accept incoming transfer
    void acceptIncomingTransfer(TransferSession* session);
    void rejectIncomingTransfer(TransferSession* session, const QString& reason = QString());

signals:
    void incomingTransferOffered(TransferSession* session, const QString& senderDevice, uint64_t totalFiles, uint64_t totalBytes);
    void sessionStarted(TransferSession* session);
    void sessionCompleted(TransferSession* session, bool success);
    void metricsUpdated(const TransferMetrics& metrics);
    void historyUpdated();

private slots:
    void onNewIncomingConnection();
    void onSessionStatusChanged(TransferStatus status);
    void onSessionCompleted();
    void onSessionFailed(const QString& errorMsg);

private:
    QTcpServer* m_tcpServer = nullptr;
    DiscoveryService* m_discoveryService = nullptr;
    Database* m_database = nullptr;

    std::unique_ptr<TransferSession> m_activeSession;

    QString m_deviceName;
    QString m_downloadDir;
    DuplicatePolicy m_duplicatePolicy = DuplicatePolicy::Rename;
    bool m_ethernetOnly = false;
    bool m_autoAccept = false;
    uint32_t m_chunkSize = DEFAULT_CHUNK_SIZE;
    uint16_t m_listenPort = DEFAULT_TCP_PORT;
};

} // namespace FastTransfer

#endif // FASTTRANSFER_TRANSFER_MANAGER_H
