#ifndef FASTTRANSFER_TCP_CONNECTION_H
#define FASTTRANSFER_TCP_CONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <cstdint>
#include "Protocol.h"

namespace FastTransfer {

class TcpConnection : public QObject {
    Q_OBJECT
public:
    explicit TcpConnection(QObject* parent = nullptr);
    explicit TcpConnection(QTcpSocket* socket, QObject* parent = nullptr);
    ~TcpConnection() override;

    void connectToHost(const QHostAddress& address, uint16_t port);
    void disconnectFromHost();

    bool isConnected() const;
    QHostAddress peerAddress() const;
    uint16_t peerPort() const;

    // Send framed protocol message
    bool sendPacket(MessageType type, const QByteArray& payload = QByteArray(), uint16_t flags = 0);

    // Raw bytes written counter
    uint64_t totalBytesSent() const { return m_totalBytesSent; }
    uint64_t totalBytesReceived() const { return m_totalBytesReceived; }

    QTcpSocket* socket() { return m_socket; }

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& errorMessage);
    void packetReceived(MessageType type, uint16_t flags, const QByteArray& payload);
    void bytesTransferred(qint64 bytesSent);

private slots:
    void onReadyRead();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    void configureSocketOptions();

    QTcpSocket* m_socket = nullptr;
    QByteArray m_readBuffer;
    bool m_headerParsed = false;
    ProtocolHeader m_currentHeader;

    uint64_t m_totalBytesSent = 0;
    uint64_t m_totalBytesReceived = 0;
};

} // namespace FastTransfer

#endif // FASTTRANSFER_TCP_CONNECTION_H
