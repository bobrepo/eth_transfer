#ifndef FASTTRANSFER_DEVICES_PAGE_H
#define FASTTRANSFER_DEVICES_PAGE_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include "../network/DiscoveryService.h"
#include "../platform/PlatformNetwork.h"

namespace FastTransfer {

class DevicesPage : public QWidget {
    Q_OBJECT
public:
    explicit DevicesPage(DiscoveryService* discovery, QWidget* parent = nullptr);
    ~DevicesPage() override = default;

    void refreshAdapters();
    void updateDiscoveredDevices(const QList<DiscoveredDevice>& devices);

signals:
    void sendToDeviceRequested(const QHostAddress& ip, uint16_t port);

private slots:
    void onRefreshClicked();
    void onManualConnectClicked();

private:
    DiscoveryService* m_discovery = nullptr;

    QListWidget* m_adaptersList = nullptr;
    QListWidget* m_devicesList = nullptr;
    QLineEdit* m_manualIpEdit = nullptr;
    QLineEdit* m_manualPortEdit = nullptr;
    QLabel* m_manualStatusLabel = nullptr;
};

} // namespace FastTransfer

#endif // FASTTRANSFER_DEVICES_PAGE_H
