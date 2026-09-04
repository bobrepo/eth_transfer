#include "TransferManager.h"
#include <QStandardPaths>
#include <QDir>
#include "../filesystem/FileEnumerator.h"
#include "../platform/PlatformNetwork.h"

namespace FastTransfer {

TransferManager::TransferManager(QObject* parent)
    : QObject(parent)
    , m_tcpServer(new QTcpServer(this))
    , m_discoveryService(new DiscoveryService(this))
    , m_database(new Database(this)) {
    connect(m_tcpServer, &QTcpServer::newConnection, this, &TransferManager::onNewIncomingConnection);
}

TransferManager::~TransferManager() {
    shutdown();
}

bool TransferManager::initialize() {
    m_database->init();

    // Restore saved settings or defaults
    m_deviceName = m_database->getSetting("device_name", PlatformNetwork::getDeviceHostname());
    m_downloadDir = m_database->getSetting("download_dir", "");
    if (m_downloadDir.isEmpty()) {
        QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        m_downloadDir = QDir(downloads).filePath("FastTransfer_Incoming");
    }
    QDir().mkpath(m_downloadDir);

    int policyInt = m_database->getSetting("duplicate_policy", "3").toInt();
    m_duplicatePolicy = static_cast<DuplicatePolicy>(policyInt);

    m_ethernetOnly = (m_database->getSetting("ethernet_only", "0") == "1");
    m_chunkSize = static_cast<uint32_t>(m_database->getSetting("chunk_size", QString::number(DEFAULT_CHUNK_SIZE)).toUInt());

    m_discoveryService->setDeviceName(m_deviceName);
    m_discoveryService->setTcpPort(m_listenPort);
    m_discoveryService->setEthernetOnly(m_ethernetOnly);
    m_discoveryService->start();

    bool listening = m_tcpServer->listen(QHostAddress::AnyIPv4, m_listenPort);
    if (!listening) {
        // Fallback to automatic port
        m_tcpServer->listen(QHostAddress::AnyIPv4, 0);
        m_listenPort = m_tcpServer->serverPort();
        m_discoveryService->setTcpPort(m_listenPort);
    }

    return true;
}

void TransferManager::shutdown() {
    m_discoveryService->stop();
    if (m_tcpServer->isListening()) {
        m_tcpServer->close();
    }
    if (m_activeSession) {
        m_activeSession->cancel();
        m_activeSession.reset();
    }
}

void TransferManager::setDeviceName(const QString& name) {
    if (!name.isEmpty() && m_deviceName != name) {
        m_deviceName = name;
        m_database->setSetting("device_name", name);
        m_discoveryService->setDeviceName(name);
    }
}

void TransferManager::setDefaultDownloadDir(const QString& dir) {
    if (!dir.isEmpty() && m_downloadDir != dir) {
        m_downloadDir = dir;
        QDir().mkpath(m_downloadDir);
        m_database->setSetting("download_dir", dir);
    }
}

void TransferManager::setDuplicatePolicy(DuplicatePolicy policy) {
    m_duplicatePolicy = policy;
    m_database->setSetting("duplicate_policy", QString::number(static_cast<int>(policy)));
}

void TransferManager::setEthernetOnly(bool ethernetOnly) {
    m_ethernetOnly = ethernetOnly;
    m_database->setSetting("ethernet_only", ethernetOnly ? "1" : "0");
    m_discoveryService->setEthernetOnly(ethernetOnly);
}

void TransferManager::setChunkSize(uint32_t bytes) {
    if (bytes >= 1024 * 1024 && bytes <= MAX_CHUNK_SIZE) {
        m_chunkSize = bytes;
        m_database->setSetting("chunk_size", QString::number(bytes));
    }
}

TransferSession* TransferManager::initiateSend(const QHostAddress& targetIp,
                                             uint16_t port,
                                             const QStringList& fileOrFolderPaths) {
    FileManifest manifest = FileEnumerator::enumeratePaths(fileOrFolderPaths);
    if (manifest.items.isEmpty()) {
        return nullptr;
    }

    if (m_activeSession) {
        m_activeSession->cancel();
        m_activeSession.reset();
    }

    m_activeSession = std::make_unique<TransferSession>(TransferDirection::Send, this);

    // Record in SQLite
    TransferRecord rec;
    rec.id = m_activeSession->transferId();
    rec.direction = "SEND";
    rec.remoteDevice = targetIp.toString();
    rec.remoteIp = targetIp.toString();
    rec.status = "Active";
    rec.totalFiles = manifest.totalFiles;
    rec.totalBytes = manifest.totalBytes;
    rec.transferredBytes = 0;
    rec.startTime = QDateTime::currentDateTime();
    m_database->recordTransferStarted(rec);
    emit historyUpdated();

    connect(m_activeSession.get(), &TransferSession::statusChanged, this, &TransferManager::onSessionStatusChanged);
    connect(m_activeSession.get(), &TransferSession::transferCompleted, this, &TransferManager::onSessionCompleted);
    connect(m_activeSession.get(), &TransferSession::transferFailed, this, &TransferManager::onSessionFailed);
    connect(m_activeSession.get(), &TransferSession::metricsUpdated, this, &TransferManager::metricsUpdated);

    emit sessionStarted(m_activeSession.get());

    m_activeSession->startSend(targetIp, port, manifest, m_deviceName, m_chunkSize);
    return m_activeSession.get();
}

void TransferManager::onNewIncomingConnection() {
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket* socket = m_tcpServer->nextPendingConnection();
        if (!socket) continue;

        // If an active session is already running, reject
        if (m_activeSession && m_activeSession->status() == TransferStatus::Transferring) {
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }

        TcpConnection* conn = new TcpConnection(socket);
        m_activeSession = std::make_unique<TransferSession>(conn, this);

        connect(m_activeSession.get(), &TransferSession::statusChanged, this, &TransferManager::onSessionStatusChanged);
        connect(m_activeSession.get(), &TransferSession::transferCompleted, this, &TransferManager::onSessionCompleted);
        connect(m_activeSession.get(), &TransferSession::transferFailed, this, &TransferManager::onSessionFailed);
        connect(m_activeSession.get(), &TransferSession::metricsUpdated, this, &TransferManager::metricsUpdated);

        connect(m_activeSession.get(), &TransferSession::incomingOfferReceived, this,
                [this](const QString& transferId, const QString& senderDevice, uint64_t totalFiles, uint64_t totalBytes) {
            Q_UNUSED(transferId);
            // Record in SQLite
            TransferRecord rec;
            rec.id = m_activeSession->transferId();
            rec.direction = "RECEIVE";
            rec.remoteDevice = senderDevice;
            rec.remoteIp = m_activeSession->remoteIp().toString();
            rec.status = "Active";
            rec.totalFiles = totalFiles;
            rec.totalBytes = totalBytes;
            rec.transferredBytes = 0;
            rec.startTime = QDateTime::currentDateTime();
            m_database->recordTransferStarted(rec);
            emit historyUpdated();

            emit incomingTransferOffered(m_activeSession.get(), senderDevice, totalFiles, totalBytes);
        });

        emit sessionStarted(m_activeSession.get());
    }
}

void TransferManager::acceptIncomingTransfer(TransferSession* session) {
    if (session && session == m_activeSession.get()) {
        session->acceptTransfer(m_downloadDir, m_duplicatePolicy);
    }
}

void TransferManager::rejectIncomingTransfer(TransferSession* session, const QString& reason) {
    if (session && session == m_activeSession.get()) {
        session->rejectTransfer(reason);
        m_database->recordTransferCompleted(session->transferId(), "Rejected", reason);
        emit historyUpdated();
    }
}

void TransferManager::onSessionStatusChanged(TransferStatus status) {
    if (m_activeSession) {
        QString statusStr = "Active";
        if (status == TransferStatus::Completed) statusStr = "Completed";
        else if (status == TransferStatus::Failed) statusStr = "Failed";
        else if (status == TransferStatus::Cancelled) statusStr = "Cancelled";
        else if (status == TransferStatus::Paused) statusStr = "Paused";

        m_database->updateTransferProgress(m_activeSession->transferId(),
                                           m_activeSession->metrics().totalTransferred,
                                           statusStr);
        emit historyUpdated();
    }
}

void TransferManager::onSessionCompleted() {
    if (m_activeSession) {
        m_database->recordTransferCompleted(m_activeSession->transferId(), "Completed");
        emit historyUpdated();
        emit sessionCompleted(m_activeSession.get(), true);
    }
}

void TransferManager::onSessionFailed(const QString& errorMsg) {
    if (m_activeSession) {
        m_database->recordTransferCompleted(m_activeSession->transferId(), "Failed", errorMsg);
        emit historyUpdated();
        emit sessionCompleted(m_activeSession.get(), false);
    }
}

} // namespace FastTransfer
