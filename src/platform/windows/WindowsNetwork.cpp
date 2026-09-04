#include "../PlatformNetwork.h"

#ifdef Q_OS_WIN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include <vector>
#include <QHostInfo>
#include <QNetworkInterface>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace FastTransfer {

static bool isVirtualAdapter(const QString& desc, const QString& friendly) {
    QString full = (desc + " " + friendly).toLower();
    return full.contains("virtual") ||
           full.contains("vmware") ||
           full.contains("vbox") ||
           full.contains("virtualbox") ||
           full.contains("hyper-v") ||
           full.contains("vethernet") ||
           full.contains("wsl") ||
           full.contains("tap") ||
           full.contains("vpn") ||
           full.contains("tailscale") ||
           full.contains("zerotier") ||
           full.contains("nordlynx") ||
           full.contains("wireguard");
}

QList<NetworkAdapterInfo> PlatformNetwork::getAdapters() {
    QList<NetworkAdapterInfo> results;

    ULONG bufferSize = 15000;
    std::vector<BYTE> buffer(bufferSize);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    DWORD ret = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, pAddresses, &bufferSize);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(bufferSize);
        pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        ret = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, pAddresses, &bufferSize);
    }

    if (ret != NO_ERROR) {
        // Fallback to Qt Network
        for (const auto& qIface : QNetworkInterface::allInterfaces()) {
            if (!qIface.flags().testFlag(QNetworkInterface::IsUp)) continue;
            NetworkAdapterInfo info;
            info.id = qIface.name();
            info.name = qIface.name();
            info.displayName = qIface.humanReadableName();
            info.isEthernet = (qIface.type() == QNetworkInterface::Ethernet);
            info.isWifi = (qIface.type() == QNetworkInterface::Wifi);
            info.isLoopback = (qIface.type() == QNetworkInterface::Loopback);
            info.isUp = qIface.flags().testFlag(QNetworkInterface::IsRunning);
            info.isVirtual = isVirtualAdapter(info.displayName, info.name);
            for (const auto& entry : qIface.addressEntries()) {
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

    for (PIP_ADAPTER_ADDRESSES curr = pAddresses; curr != nullptr; curr = curr->Next) {
        if (curr->OperStatus != IfOperStatusUp) {
            continue; // Skip inactive interfaces
        }

        NetworkAdapterInfo info;
        info.id = QString::fromLatin1(curr->AdapterName);
        info.name = QString::fromWCharArray(curr->FriendlyName);
        info.displayName = QString::fromWCharArray(curr->Description);
        info.isUp = (curr->OperStatus == IfOperStatusUp);

        // Classify adapter physical type
        switch (curr->IfType) {
            case IF_TYPE_ETHERNET_CSMACD:
            case IF_TYPE_FASTETHER:
            case IF_TYPE_GIGABITETHERNET:
                info.isEthernet = true;
                break;
            case IF_TYPE_IEEE80211:
                info.isWifi = true;
                break;
            case IF_TYPE_SOFTWARE_LOOPBACK:
                info.isLoopback = true;
                break;
            default:
                break;
        }

        info.isVirtual = isVirtualAdapter(info.displayName, info.name);

        // Get link speed: TransmitLinkSpeed is in bits per second
        uint64_t bps = curr->TransmitLinkSpeed;
        info.linkSpeedMbps = bps / 1000000ULL;

        // Parse IPv4 addresses
        for (PIP_ADAPTER_UNICAST_ADDRESS uAddr = curr->FirstUnicastAddress; uAddr != nullptr; uAddr = uAddr->Next) {
            if (uAddr->Address.lpSockaddr->sa_family == AF_INET) {
                sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(uAddr->Address.lpSockaddr);
                info.ipv4Addresses.append(QHostAddress(ntohl(sa_in->sin_addr.s_addr)));
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
    WCHAR computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(computerName, &size)) {
        return QString::fromWCharArray(computerName);
    }
    return QHostInfo::localHostName();
}

} // namespace FastTransfer

#endif // Q_OS_WIN
