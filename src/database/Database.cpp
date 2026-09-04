#include "Database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>

namespace FastTransfer {

Database::Database(QObject* parent)
    : QObject(parent)
    , m_connectionName(QString("FastTransferDb_%1").arg(QUuid::createUuid().toString())) {
}

Database::~Database() {
    close();
}

bool Database::init(const QString& dbPath) {
    QString actualPath = dbPath;
    if (actualPath.isEmpty()) {
        QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(appData);
        actualPath = QDir(appData).filePath("fasttransfer.db");
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(actualPath);

    if (!m_db.open()) {
        return false;
    }

    return createTables();
}

void Database::close() {
    if (m_db.isOpen()) {
        m_db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool Database::createTables() {
    QSqlQuery query(m_db);

    bool ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS transfers (
            id TEXT PRIMARY KEY,
            direction TEXT NOT NULL,
            remote_device TEXT,
            remote_ip TEXT,
            status TEXT NOT NULL,
            total_files INTEGER NOT NULL,
            total_bytes INTEGER NOT NULL,
            transferred_bytes INTEGER NOT NULL,
            start_time TEXT NOT NULL,
            end_time TEXT,
            error_message TEXT
        );
    )");
    if (!ok) return false;

    ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS transfer_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            transfer_id TEXT NOT NULL,
            file_index INTEGER NOT NULL,
            relative_path TEXT NOT NULL,
            file_size INTEGER NOT NULL,
            bytes_transferred INTEGER NOT NULL,
            checksum TEXT,
            status TEXT NOT NULL,
            FOREIGN KEY (transfer_id) REFERENCES transfers(id)
        );
    )");
    if (!ok) return false;

    ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )");
    return ok;
}

bool Database::recordTransferStarted(const TransferRecord& record) {
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO transfers (id, direction, remote_device, remote_ip, status,
                               total_files, total_bytes, transferred_bytes, start_time, error_message)
        VALUES (:id, :direction, :remote_device, :remote_ip, :status,
                :total_files, :total_bytes, :transferred_bytes, :start_time, :error_message);
    )");
    query.bindValue(":id", record.id);
    query.bindValue(":direction", record.direction);
    query.bindValue(":remote_device", record.remoteDevice);
    query.bindValue(":remote_ip", record.remoteIp);
    query.bindValue(":status", record.status);
    query.bindValue(":total_files", static_cast<qint64>(record.totalFiles));
    query.bindValue(":total_bytes", static_cast<qint64>(record.totalBytes));
    query.bindValue(":transferred_bytes", static_cast<qint64>(record.transferredBytes));
    query.bindValue(":start_time", record.startTime.toString(Qt::ISODate));
    query.bindValue(":error_message", record.errorMessage);
    return query.exec();
}

bool Database::updateTransferProgress(const QString& transferId, uint64_t transferredBytes, const QString& status) {
    QSqlQuery query(m_db);
    query.prepare(R"(
        UPDATE transfers
        SET transferred_bytes = :transferred_bytes, status = :status
        WHERE id = :id;
    )");
    query.bindValue(":transferred_bytes", static_cast<qint64>(transferredBytes));
    query.bindValue(":status", status);
    query.bindValue(":id", transferId);
    return query.exec();
}

bool Database::recordTransferCompleted(const QString& transferId, const QString& status, const QString& errorMessage) {
    QSqlQuery query(m_db);
    query.prepare(R"(
        UPDATE transfers
        SET status = :status, end_time = :end_time, error_message = :error_message
        WHERE id = :id;
    )");
    query.bindValue(":status", status);
    query.bindValue(":end_time", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":error_message", errorMessage);
    query.bindValue(":id", transferId);
    return query.exec();
}

QList<TransferRecord> Database::getTransferHistory(int limit) {
    QList<TransferRecord> records;
    QSqlQuery query(m_db);
    query.prepare(QString("SELECT id, direction, remote_device, remote_ip, status, total_files, "
                          "total_bytes, transferred_bytes, start_time, end_time, error_message "
                          "FROM transfers ORDER BY start_time DESC LIMIT %1").arg(limit));

    if (query.exec()) {
        while (query.next()) {
            TransferRecord r;
            r.id = query.value(0).toString();
            r.direction = query.value(1).toString();
            r.remoteDevice = query.value(2).toString();
            r.remoteIp = query.value(3).toString();
            r.status = query.value(4).toString();
            r.totalFiles = static_cast<uint64_t>(query.value(5).toLongLong());
            r.totalBytes = static_cast<uint64_t>(query.value(6).toLongLong());
            r.transferredBytes = static_cast<uint64_t>(query.value(7).toLongLong());
            r.startTime = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
            r.endTime = QDateTime::fromString(query.value(9).toString(), Qt::ISODate);
            r.errorMessage = query.value(10).toString();
            records.append(r);
        }
    }
    return records;
}

bool Database::clearHistory() {
    QSqlQuery q1(m_db);
    q1.exec("DELETE FROM transfer_files;");
    QSqlQuery q2(m_db);
    return q2.exec("DELETE FROM transfers;");
}

QString Database::getSetting(const QString& key, const QString& defaultValue) {
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM settings WHERE key = :key;");
    query.bindValue(":key", key);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return defaultValue;
}

bool Database::setSetting(const QString& key, const QString& value) {
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (:key, :value);");
    query.bindValue(":key", key);
    query.bindValue(":value", value);
    return query.exec();
}

} // namespace FastTransfer
