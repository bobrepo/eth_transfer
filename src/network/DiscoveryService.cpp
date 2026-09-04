#include "DiscoveryService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QNetworkDatagram>

namespace FastTransfer {

DiscoveryService::DiscoveryService(QObject* parent)
    : QObject(parent)
    , m_deviceName(PlatformNetwork::getDeviceHostname()) {
    m_udpSocket = new QUdpSocket(this);
    m_broadcastTimer = new QTimer(this);
    m_cleanupTimer = new QTimer(this);

    connect(m_udpSocket, &QUdpSocket::readyRead, this, &DiscoveryService::onReadyRead);
    connect(m_broadcastTimer, &QTimer::timeout, this, &DiscoveryService::sendBroadcastAnnouncement);
    connect(m_cleanupTimer, &QTimer::timeout, this, &DiscoveryService::cleanupStaleDevices);
}

DiscoveryService::~DiscoveryService() {
    stop();
}

void DiscoveryService::start() {
    if (m_udpSocket->state() == QAbstractSocket::BoundState) {
        m_udpSocket->close();
    }

    bool bound = m_udpSocket->bind(QHostAddress::AnyIPv4, DEFAULT_DISCOVERY_PORT,
                                   QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (!bound) {
        // Retry binding with normal options
        m_udpSocket->bind(DEFAULT_DISCOVERY_PORT);
    }

    // Join local multicast group on all active interfaces
    const QHostAddress multicastAddr("239.255.45.82");
    m_udpSocket->joinMulticastGroup(multicastAddr);
    for (const auto& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsUp) &&
            !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            m_udpSocket->joinMulticastGroup(multicastAddr, iface);
        }
    }

    m_broadcastTimer->start(3000); // Announcement every 3 seconds
    m_cleanupTimer->start(4000);   // Sweep stale devices every 4 seconds

    // Immediately send first broadcast
    sendBroadcastAnnouncement();
}

void DiscoveryService::stop() {
    m_broadcastTimer->stop();
    m_cleanupTimer->stop();
    if (m_udpSocket->isOpen()) {
        m_udpSocket->close();
    }
}

void DiscoveryService::refresh() {
    sendBroadcastAnnouncement();
}

void DiscoveryService::setDeviceName(const QString& name) {
    if (!name.isEmpty() && m_deviceName != name) {
        m_deviceName = name;
        sendBroadcastAnnouncement();
    }
}

void DiscoveryService::setEthernetOnly(bool ethernetOnly) {
    m_ethernetOnly = ethernetOnly;
    refresh();
}

QList<DiscoveredDevice> DiscoveryService::activeDevices() const {
    return m_devices.values();
}

void DiscoveryService::sendBroadcastAnnouncement() {
    auto adapters = PlatformNetwork::getAdapters();
    NetworkAdapterInfo primaryAdapter = PlatformNetwork::getBestEthernetAdapter();

    if (primaryAdapter.name.isEmpty() && !adapters.isEmpty()) {
        primaryAdapter = adapters.first();
    }

    QJsonObject obj;
    obj["app"] = "FastTransfer";
    obj["version"] = static_cast<int>(PROTOCOL_VERSION);
    obj["device_name"] = m_deviceName;
    obj["tcp_port"] = static_cast<int>(m_tcpPort);
#ifdef Q_OS_WIN
    obj["os"] = "Windows";
#elif defined(Q_OS_LINUX)
    obj["os"] = "Linux";
#else
    obj["os"] = "macOS";
#endif
    obj["interface"] = primaryAdapter.adapterTypeString();
    obj["link_speed_mbps"] = static_cast<qint64>(primaryAdapter.linkSpeedMbps);
    obj["mode"] = "READY";

    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    broadcastJson(data);
}

void DiscoveryService::broadcastJson(const QByteArray& jsonData) {
    const QHostAddress multicastAddr("239.255.45.82");

    // 1. Send to global broadcast (255.255.255.255)
    m_udpSocket->writeDatagram(jsonData, QHostAddress::Broadcast, DEFAULT_DISCOVERY_PORT);

    // 2. Send to dedicated local multicast group
    m_udpSocket->writeDatagram(jsonData, multicastAddr, DEFAULT_DISCOVERY_PORT);

    // 3. Send interface-directed broadcasts and multicasts for every active interface
    const auto allIfaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : allIfaces) {
        if (!iface.flags().testFlag(QNetworkInterface::IsUp) ||
            iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }

        // Set interface for multicast output
        m_udpSocket->setMulticastInterface(iface);
        m_udpSocket->writeDatagram(jsonData, multicastAddr, DEFAULT_DISCOVERY_PORT);

        // Subnet-specific broadcast addresses
        for (const auto& entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                if (!entry.broadcast().isNull()) {
                    m_udpSocket->writeDatagram(jsonData, entry.broadcast(), DEFAULT_DISCOVERY_PORT);
                }
                // Handle link-local APIPA addresses (169.254.x.x) for direct PC-to-PC cables
                if (entry.ip().isInSubnet(QHostAddress("169.254.0.0"), 16)) {
                    m_udpSocket->writeDatagram(jsonData, QHostAddress("169.254.255.255"), DEFAULT_DISCOVERY_PORT);
                }
            }
        }
    }
}

void DiscoveryService::onReadyRead() {
    while (m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        QByteArray data = datagram.data();
        QHostAddress senderIp = datagram.senderAddress();

        // Convert IPv4-mapped IPv6 back to clean IPv4
        bool ok = false;
        quint32 ipv4Val = senderIp.toIPv4Address(&ok);
        if (ok) {
            senderIp = QHostAddress(ipv4Val);
        }

        auto doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) continue;
        auto obj = doc.object();

        if (obj["app"].toString() != "FastTransfer") continue;

        QString remoteDeviceName = obj["device_name"].toString();
        uint16_t remotePort = static_cast<uint16_t>(obj["tcp_port"].toInt(DEFAULT_TCP_PORT));

        // Skip packets from self on same machine/port
        if (remoteDeviceName == m_deviceName && remotePort == m_tcpPort) {
            // Check if IP is our own
            bool isOwnIp = false;
            for (const auto& a : PlatformNetwork::getAdapters()) {
                if (a.ipv4Addresses.contains(senderIp)) {
                    isOwnIp = true;
                    break;
                }
            }
            if (isOwnIp) continue;
        }

        QString deviceId = QString("%1:%2").arg(senderIp.toString()).arg(remotePort);

        DiscoveredDevice dev;
        dev.id = deviceId;
        dev.deviceName = remoteDeviceName;
        dev.ip = senderIp;
        dev.tcpPort = remotePort;
        dev.osType = obj["os"].toString();
        dev.interfaceType = obj["interface"].toString("Ethernet");
        dev.linkSpeedMbps = static_cast<uint64_t>(obj["link_speed_mbps"].toVariant().toULongLong());
        dev.mode = obj["mode"].toString("READY");
        dev.lastSeen = QDateTime::currentDateTime();

        bool isNew = !m_devices.contains(deviceId);
        m_devices[deviceId] = dev;

        if (isNew) {
            emit deviceDiscovered(dev);
            emit devicesChanged();
        } else {
            emit deviceUpdated(dev);
        }
    }
}

void DiscoveryService::cleanupStaleDevices() {
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-10); // 10s timeout
    QStringList toRemove;

    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        if (it.value().lastSeen < cutoff) {
            toRemove.append(it.key());
        }
    }

    if (!toRemove.isEmpty()) {
        for (const QString& id : toRemove) {
            m_devices.remove(id);
            emit deviceLost(id);
        }
        emit devicesChanged();
    }
}

} // namespace FastTransfer
