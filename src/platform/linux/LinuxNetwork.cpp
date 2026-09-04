#include "../PlatformNetwork.h"

#if defined(Q_OS_LINUX)

#include <QNetworkInterface>
#include <QFile>
#include <QTextStream>
#include <QHostInfo>

namespace FastTransfer {

static bool isLinuxVirtualAdapter(const QString& name) {
    QString lower = name.toLower();
    return lower.startsWith("docker") ||
           lower.startsWith("veth") ||
           lower.startsWith("br-") ||
           lower.startsWith("virbr") ||
           lower.startsWith("vmnet") ||
           lower.startsWith("tap") ||
           lower.startsWith("tun") ||
           lower.startsWith("wg") ||
           lower.startsWith("tailscale") ||
           lower.startsWith("zt");
}

static uint64_t readLinuxLinkSpeed(const QString& ifaceName) {
    QFile speedFile(QString("/sys/class/net/%1/speed").arg(ifaceName));
    if (speedFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&speedFile);
        qint64 speed = 0;
        in >> speed;
        if (speed > 0) {
            return static_cast<uint64_t>(speed); // In Mbps
        }
    }
    return 0;
}

QList<NetworkAdapterInfo> PlatformNetwork::getAdapters() {
    QList<NetworkAdapterInfo> results;

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : interfaces) {
        if (!iface.flags().testFlag(QNetworkInterface::IsUp)) {
            continue;
        }

        NetworkAdapterInfo info;
        info.id = iface.name();
        info.name = iface.name();
        info.displayName = iface.humanReadableName();
        info.isUp = iface.flags().testFlag(QNetworkInterface::IsRunning);
        info.isLoopback = iface.flags().testFlag(QNetworkInterface::IsLoopBack);
        info.isVirtual = isLinuxVirtualAdapter(info.name);

        // Check wireless presence in sysfs
        bool hasWireless = QFile::exists(QString("/sys/class/net/%1/wireless").arg(iface.name())) ||
                           QFile::exists(QString("/sys/class/net/%1/phy80211").arg(iface.name()));

        if (info.isLoopback) {
            info.isEthernet = false;
            info.isWifi = false;
        } else if (hasWireless) {
            info.isWifi = true;
            info.isEthernet = false;
        } else {
            info.isEthernet = true;
            info.isWifi = false;
        }

        info.linkSpeedMbps = readLinuxLinkSpeed(iface.name());

        for (const auto& entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                info.ipv4Addresses.append(entry.ip());
            }
        }

        if (!info.ipv4Addresses.isEmpty()) {
            results.append(info);
        }
    }

    return results;
}

NetworkAdapterInfo PlatformNetwork::getBestEthernetAdapter() {
    auto adapters = getAdapters();
    NetworkAdapterInfo best;
    uint64_t maxSpeed = 0;

    for (const auto& a : adapters) {
        if (a.isEthernet && !a.isVirtual && a.isUp && !a.ipv4Addresses.isEmpty()) {
            if (a.linkSpeedMbps >= maxSpeed) {
                maxSpeed = a.linkSpeedMbps;
                best = a;
            }
        }
    }
    return best;
}

QString PlatformNetwork::getDeviceHostname() {
    return QHostInfo::localHostName();
}

} // namespace FastTransfer

#endif // Q_OS_LINUX
