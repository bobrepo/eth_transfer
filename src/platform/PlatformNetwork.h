#ifndef FASTTRANSFER_PLATFORM_NETWORK_H
#define FASTTRANSFER_PLATFORM_NETWORK_H

#include <QString>
#include <QList>
#include <QHostAddress>
#include <cstdint>

namespace FastTransfer {

struct NetworkAdapterInfo {
    QString id;                 // System ID or GUID
    QString name;               // Interface name (e.g., "eth0", "Ethernet 2")
    QString displayName;        // Friendly descriptive name
    bool isEthernet = false;
    bool isWifi = false;
    bool isLoopback = false;
    bool isVirtual = false;     // Filter out VMware, VirtualBox, WSL, TAP, Hyper-V, VPN
    bool isUp = false;
    uint64_t linkSpeedMbps = 0; // 1000 = 1.0 Gbps, 2500 = 2.5 Gbps, 10000 = 10.0 Gbps
    QList<QHostAddress> ipv4Addresses;

    QString formattedSpeed() const {
        if (linkSpeedMbps >= 10000) {
            return QString::asprintf("%.0f Gbps", linkSpeedMbps / 1000.0);
        } else if (linkSpeedMbps >= 1000) {
            return QString::asprintf("%.1f Gbps", linkSpeedMbps / 1000.0);
        } else if (linkSpeedMbps > 0) {
            return QString::asprintf("%llu Mbps", static_cast<unsigned long long>(linkSpeedMbps));
        }
        return QStringLiteral("Unknown Speed");
    }

    QString adapterTypeString() const {
        if (isEthernet) return QStringLiteral("Ethernet");
        if (isWifi) return QStringLiteral("Wi-Fi");
        if (isLoopback) return QStringLiteral("Loopback");
        return QStringLiteral("Other");
    }
};

class PlatformNetwork {
public:
    // Enumerate active network adapters with physical classification and link speed
    static QList<NetworkAdapterInfo> getAdapters();

    // Find the highest-speed physical Ethernet adapter available
    static NetworkAdapterInfo getBestEthernetAdapter();

    // Query operating system hostname (clean for default device naming)
    static QString getDeviceHostname();
};

} // namespace FastTransfer

#endif // FASTTRANSFER_PLATFORM_NETWORK_H
