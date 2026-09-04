#ifndef FASTTRANSFER_DATABASE_H
#define FASTTRANSFER_DATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QDateTime>
#include <QList>
#include <cstdint>

namespace FastTransfer {

struct TransferRecord {
    QString id;
    QString direction;      // "SEND" or "RECEIVE"
    QString remoteDevice;
    QString remoteIp;
    QString status;         // "Completed", "Failed", "Interrupted", "Cancelled"
    uint64_t totalFiles = 0;
    uint64_t totalBytes = 0;
    uint64_t transferredBytes = 0;
    QDateTime startTime;
    QDateTime endTime;
    QString errorMessage;
};

class Database : public QObject {
    Q_OBJECT
public:
    explicit Database(QObject* parent = nullptr);
    ~Database() override;

    bool init(const QString& dbPath = QString());
    void close();

    // Transfer history CRUD
    bool recordTransferStarted(const TransferRecord& record);
    bool updateTransferProgress(const QString& transferId, uint64_t transferredBytes, const QString& status);
    bool recordTransferCompleted(const QString& transferId, const QString& status, const QString& errorMessage = QString());
    QList<TransferRecord> getTransferHistory(int limit = 100);
    bool clearHistory();

    // App settings persistence
    QString getSetting(const QString& key, const QString& defaultValue = QString());
    bool setSetting(const QString& key, const QString& value);

private:
    bool createTables();

    QSqlDatabase m_db;
    QString m_connectionName;
};

} // namespace FastTransfer

#endif // FASTTRANSFER_DATABASE_H
