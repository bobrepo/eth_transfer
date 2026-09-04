#ifndef FASTTRANSFER_DISCOVERY_SERVICE_H
#define FASTTRANSFER_DISCOVERY_SERVICE_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QDateTime>
#include <QHostAddress>
#include <QMap>
#include <QList>
#include "Protocol.h"
#include "../platform/PlatformNetwork.h"

namespace FastTransfer {

struct DiscoveredDevice {
    QString id;                 // Key: ip + ":" + port
    QString deviceName;
    QHostAddress ip;
    uint16_t tcpPort = DEFAULT_TCP_PORT;
    QString osType;
    QString interfaceType;      // "Ethernet", "Wi-Fi", "Other"
    uint64_t linkSpeedMbps = 0;
    QString mode;               // "READY", "RECEIVE", "BUSY"
    QDateTime lastSeen;

    QString formattedSpeed() const {
        if (linkSpeedMbps >= 1000) {
            return QString::asprintf("%.1f Gbps", linkSpeedMbps / 1000.0);
        } else if (linkSpeedMbps > 0) {
            return QString::asprintf("%llu Mbps", static_cast<unsigned long long>(linkSpeedMbps));
        }
        return QStringLiteral("Unknown Speed");
    }

    QString summary() const {
        QString speed = linkSpeedMbps > 0 ? QString(" (%1)").arg(
            linkSpeedMbps >= 1000 ? QString("%1 Gbps").arg(linkSpeedMbps / 1000.0, 0, 'f', 1)
                                  : QString("%1 Mbps").arg(linkSpeedMbps)) : "";
        return QString("%1 — %2%3").arg(deviceName, interfaceType, speed);
    }
};

class DiscoveryService : public QObject {
    Q_OBJECT
public:
    explicit DiscoveryService(QObject* parent = nullptr);
    ~DiscoveryService() override;

    void start();
    void stop();
    void refresh();

    void setDeviceName(const QString& name);
    QString deviceName() const { return m_deviceName; }

    void setTcpPort(uint16_t port) { m_tcpPort = port; }
    uint16_t tcpPort() const { return m_tcpPort; }

    void setEthernetOnly(bool ethernetOnly);
    bool isEthernetOnly() const { return m_ethernetOnly; }

    QList<DiscoveredDevice> activeDevices() const;

signals:
    void deviceDiscovered(const DiscoveredDevice& device);
    void deviceUpdated(const DiscoveredDevice& device);
    void deviceLost(const QString& deviceId);
    void devicesChanged();

private slots:
    void onReadyRead();
    void sendBroadcastAnnouncement();
    void cleanupStaleDevices();

private:
    void broadcastJson(const QByteArray& jsonData);

    QUdpSocket* m_udpSocket = nullptr;
    QTimer* m_broadcastTimer = nullptr;
    QTimer* m_cleanupTimer = nullptr;

    QString m_deviceName;
    uint16_t m_tcpPort = DEFAULT_TCP_PORT;
    bool m_ethernetOnly = false;

    QMap<QString, DiscoveredDevice> m_devices;
};

} // namespace FastTransfer

#endif // FASTTRANSFER_DISCOVERY_SERVICE_H
