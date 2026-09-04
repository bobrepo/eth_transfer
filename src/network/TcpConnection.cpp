#include "TcpConnection.h"
#include <QtEndian>

namespace FastTransfer {

TcpConnection::TcpConnection(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this)) {
    configureSocketOptions();
}

TcpConnection::TcpConnection(QTcpSocket* socket, QObject* parent)
    : QObject(parent)
    , m_socket(socket) {
    m_socket->setParent(this);
    configureSocketOptions();
}

TcpConnection::~TcpConnection() {
    disconnectFromHost();
}

void TcpConnection::configureSocketOptions() {
    if (!m_socket) return;

    // High throughput TCP options for Ethernet transfer
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1); // Disable Nagle algorithm
    m_socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 8 * 1024 * 1024); // 8 MB buffer
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 8 * 1024 * 1024); // 8 MB buffer
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    connect(m_socket, &QTcpSocket::readyRead, this, &TcpConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &TcpConnection::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpConnection::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &TcpConnection::onSocketError);
    connect(m_socket, &QTcpSocket::bytesWritten, this, [this](qint64 bytes) {
        m_totalBytesSent += static_cast<uint64_t>(bytes);
        emit bytesTransferred(bytes);
    });
}

void TcpConnection::connectToHost(const QHostAddress& address, uint16_t port) {
    if (m_socket) {
        m_socket->connectToHost(address, port);
    }
}

void TcpConnection::disconnectFromHost() {
    if (m_socket && m_socket->isOpen()) {
        m_socket->disconnectFromHost();
    }
}

bool TcpConnection::isConnected() const {
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

QHostAddress TcpConnection::peerAddress() const {
    return m_socket ? m_socket->peerAddress() : QHostAddress();
}

uint16_t TcpConnection::peerPort() const {
    return m_socket ? m_socket->peerPort() : 0;
}

bool TcpConnection::sendPacket(MessageType type, const QByteArray& payload, uint16_t flags) {
    if (!isConnected()) {
        return false;
    }

    QByteArray packet = Protocol::createPacket(type, payload, flags);
    qint64 written = m_socket->write(packet);
    return (written == packet.size());
}

void TcpConnection::onReadyRead() {
    qint64 available = m_socket->bytesAvailable();
    if (available <= 0) return;

    m_readBuffer.append(m_socket->readAll());
    m_totalBytesReceived += static_cast<uint64_t>(available);

    constexpr size_t kHeaderSize = sizeof(ProtocolHeader);

    while (true) {
        if (!m_headerParsed) {
            if (m_readBuffer.size() < static_cast<qsizetype>(kHeaderSize)) {
                return; // Wait for header bytes
            }

            ProtocolHeader rawHeader;
            memcpy(&rawHeader, m_readBuffer.constData(), kHeaderSize);

            m_currentHeader.magic = qFromBigEndian(rawHeader.magic);
            m_currentHeader.msgType = qFromBigEndian(rawHeader.msgType);
            m_currentHeader.flags = qFromBigEndian(rawHeader.flags);
            m_currentHeader.payloadLength = qFromBigEndian(rawHeader.payloadLength);

            if (m_currentHeader.magic != PROTOCOL_MAGIC) {
                emit errorOccurred(QStringLiteral("Invalid protocol magic header received. Closing connection."));
                m_socket->disconnectFromHost();
                return;
            }

            // Enforce safe payload size bounds
            if (m_currentHeader.payloadLength > MAX_METADATA_PAYLOAD_SIZE &&
                static_cast<MessageType>(m_currentHeader.msgType) != MessageType::FileDataChunk) {
                emit errorOccurred(QStringLiteral("Oversized protocol message rejected."));
                m_socket->disconnectFromHost();
                return;
            }

            m_headerParsed = true;
        }

        // Check if full payload is present
        uint64_t totalPacketSize = kHeaderSize + m_currentHeader.payloadLength;
        if (static_cast<uint64_t>(m_readBuffer.size()) < totalPacketSize) {
            return; // Wait for remaining payload bytes
        }

        // Extract payload
        QByteArray payload;
        if (m_currentHeader.payloadLength > 0) {
            payload = m_readBuffer.mid(static_cast<qsizetype>(kHeaderSize),
                                       static_cast<qsizetype>(m_currentHeader.payloadLength));
        }

        // Remove packet from read buffer
        m_readBuffer.remove(0, static_cast<qsizetype>(totalPacketSize));
        m_headerParsed = false;

        emit packetReceived(static_cast<MessageType>(m_currentHeader.msgType),
                            m_currentHeader.flags,
                            payload);
    }
}

void TcpConnection::onSocketConnected() {
    emit connected();
}

void TcpConnection::onSocketDisconnected() {
    emit disconnected();
}

void TcpConnection::onSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    emit errorOccurred(m_socket->errorString());
}

} // namespace FastTransfer
