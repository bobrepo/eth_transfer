#include "DevicesPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QTcpSocket>

namespace FastTransfer {

DevicesPage::DevicesPage(DiscoveryService* discovery, QWidget* parent)
    : QWidget(parent)
    , m_discovery(discovery) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(18);

    // Header
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QVBoxLayout* titleCol = new QVBoxLayout();

    QLabel* title = new QLabel(QStringLiteral("Network & Devices"), this);
    title->setProperty("class", "PageHeaderTitle");
    QLabel* subtitle = new QLabel(QStringLiteral("Monitor active Ethernet interfaces and discovered peers on the local network."), this);
    subtitle->setProperty("class", "PageHeaderSubtitle");

    titleCol->addWidget(title);
    titleCol->addWidget(subtitle);
    headerLayout->addLayout(titleCol);
    headerLayout->addStretch();

    QPushButton* refreshBtn = new QPushButton(QStringLiteral("Scan Now"), this);
    headerLayout->addWidget(refreshBtn);
    mainLayout->addLayout(headerLayout);

    connect(refreshBtn, &QPushButton::clicked, this, &DevicesPage::onRefreshClicked);

    // Local Network Adapters Card
    QFrame* adapterCard = new QFrame(this);
    adapterCard->setProperty("class", "CardFrame");
    QVBoxLayout* adapterLayout = new QVBoxLayout(adapterCard);
    adapterLayout->setSpacing(10);

    QLabel* adapterTitle = new QLabel(QStringLiteral("Local Network Interfaces"), adapterCard);
    adapterTitle->setProperty("class", "SectionTitle");
    adapterLayout->addWidget(adapterTitle);

    m_adaptersList = new QListWidget(adapterCard);
    m_adaptersList->setMaximumHeight(110);
    adapterLayout->addWidget(m_adaptersList);
    mainLayout->addWidget(adapterCard);

    // Discovered Devices Card
    QFrame* devCard = new QFrame(this);
    devCard->setProperty("class", "CardFrame");
    QVBoxLayout* devLayout = new QVBoxLayout(devCard);
    devLayout->setSpacing(10);

    QLabel* devTitle = new QLabel(QStringLiteral("Available FastTransfer Computers"), devCard);
    devTitle->setProperty("class", "SectionTitle");
    devLayout->addWidget(devTitle);

    m_devicesList = new QListWidget(devCard);
    m_devicesList->setMinimumHeight(160);
    devLayout->addWidget(m_devicesList);
    mainLayout->addWidget(devCard, 1);

    // Direct M2M Manual Connect Card
    QFrame* manualCard = new QFrame(this);
    manualCard->setProperty("class", "CardFrame");
    QVBoxLayout* manualLayout = new QVBoxLayout(manualCard);
    manualLayout->setSpacing(10);

    QLabel* manualTitle = new QLabel(QStringLiteral("Direct PC-to-PC Ethernet Connection"), manualCard);
    manualTitle->setProperty("class", "SectionTitle");
    manualLayout->addWidget(manualTitle);

    QHBoxLayout* manualRow = new QHBoxLayout();
    m_manualIpEdit = new QLineEdit(manualCard);
    m_manualIpEdit->setPlaceholderText(QStringLiteral("Target IP (e.g., 192.168.10.2)"));

    m_manualPortEdit = new QLineEdit(QString::number(DEFAULT_TCP_PORT), manualCard);
    m_manualPortEdit->setMaximumWidth(80);

    QPushButton* connectBtn = new QPushButton(QStringLiteral("Test Connection"), manualCard);
    QPushButton* sendBtn = new QPushButton(QStringLiteral("Send Files to This IP"), manualCard);
    sendBtn->setObjectName(QStringLiteral("PrimaryButton"));

    manualRow->addWidget(m_manualIpEdit);
    manualRow->addWidget(m_manualPortEdit);
    manualRow->addWidget(connectBtn);
    manualRow->addWidget(sendBtn);
    manualLayout->addLayout(manualRow);

    m_manualStatusLabel = new QLabel(manualCard);
    m_manualStatusLabel->setStyleSheet("color: #64748B; font-size: 12px;");
    manualLayout->addWidget(m_manualStatusLabel);

    mainLayout->addWidget(manualCard);

    connect(connectBtn, &QPushButton::clicked, this, &DevicesPage::onManualConnectClicked);
    connect(sendBtn, &QPushButton::clicked, this, [this]() {
        QHostAddress ip;
        if (ip.setAddress(m_manualIpEdit->text().trimmed())) {
            uint16_t port = static_cast<uint16_t>(m_manualPortEdit->text().toUShort());
            if (port == 0) port = DEFAULT_TCP_PORT;
            emit sendToDeviceRequested(ip, port);
        } else {
            QMessageBox::warning(this, QStringLiteral("Invalid IP"), QStringLiteral("Please enter a valid target IPv4 address."));
        }
    });

    refreshAdapters();
}

void DevicesPage::refreshAdapters() {
    m_adaptersList->clear();
    auto adapters = PlatformNetwork::getAdapters();

    for (const auto& a : adapters) {
        QString typeIcon = a.isEthernet ? QStringLiteral("🔌 Ethernet") : (a.isWifi ? QStringLiteral("📶 Wi-Fi") : QStringLiteral("🌐 Loopback"));
        QString speedStr = a.formattedSpeed();
        QString ipStr = a.ipv4Addresses.isEmpty() ? QStringLiteral("No IPv4") : a.ipv4Addresses.first().toString();

        QString line = QString("%1  •  %2  •  IP: %3  •  Link Speed: %4  (%5)")
                           .arg(typeIcon, a.displayName, ipStr, speedStr, a.isVirtual ? QStringLiteral("Virtual/Filtered") : QStringLiteral("Physical"));

        QListWidgetItem* item = new QListWidgetItem(line, m_adaptersList);
        if (a.isEthernet && !a.isVirtual && a.isUp) {
            item->setForeground(QColor("#10B981")); // Preferred green
        }
    }
}

void DevicesPage::updateDiscoveredDevices(const QList<DiscoveredDevice>& devices) {
    m_devicesList->clear();
    for (const auto& d : devices) {
        QString line = QString("🟢 %1  •  IP: %2:%3  •  OS: %4  •  Interface: %5  •  Link: %6")
                           .arg(d.deviceName, d.ip.toString())
                           .arg(d.tcpPort)
                           .arg(d.osType, d.interfaceType, d.formattedSpeed());

        QListWidgetItem* item = new QListWidgetItem(line, m_devicesList);
        item->setData(Qt::UserRole, d.ip.toString());
        item->setData(Qt::UserRole + 1, d.tcpPort);
    }
}

void DevicesPage::onRefreshClicked() {
    refreshAdapters();
    if (m_discovery) {
        m_discovery->refresh();
    }
}

void DevicesPage::onManualConnectClicked() {
    QHostAddress ip;
    if (!ip.setAddress(m_manualIpEdit->text().trimmed())) {
        m_manualStatusLabel->setText(QStringLiteral("❌ Invalid IPv4 address format"));
        m_manualStatusLabel->setStyleSheet("color: #EF4444; font-weight: 600; font-size: 12px;");
        return;
    }

    uint16_t port = static_cast<uint16_t>(m_manualPortEdit->text().toUShort());
    if (port == 0) port = DEFAULT_TCP_PORT;

    m_manualStatusLabel->setText(QStringLiteral("Testing TCP socket connection..."));
    m_manualStatusLabel->setStyleSheet("color: #38BDF8; font-size: 12px;");

    QTcpSocket* testSocket = new QTcpSocket(this);
    connect(testSocket, &QTcpSocket::connected, this, [this, testSocket, ip, port]() {
        m_manualStatusLabel->setText(QString("✓ Successfully connected to %1:%2 over TCP!").arg(ip.toString()).arg(port));
        m_manualStatusLabel->setStyleSheet("color: #10B981; font-weight: 600; font-size: 12px;");
        testSocket->disconnectFromHost();
        testSocket->deleteLater();
    });

    connect(testSocket, &QTcpSocket::errorOccurred, this, [this, testSocket](QAbstractSocket::SocketError) {
        m_manualStatusLabel->setText(QString("❌ Connection failed: %1").arg(testSocket->errorString()));
        m_manualStatusLabel->setStyleSheet("color: #EF4444; font-weight: 600; font-size: 12px;");
        testSocket->deleteLater();
    });

    testSocket->connectToHost(ip, port);
}

} // namespace FastTransfer
