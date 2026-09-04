#include "MainWindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include "ThemeManager.h"

namespace FastTransfer {

MainWindow::MainWindow(TransferManager* manager, QWidget* parent)
    : QMainWindow(parent)
    , m_manager(manager) {
    setWindowTitle(QStringLiteral("FastTransfer — High-Speed Ethernet File Transfer"));
    setMinimumSize(1000, 700);
    resize(1100, 750);

    setupUi();
    connectSignals();

    // Initial state sync
    if (m_manager) {
        m_receivePage->setDownloadDir(m_manager->defaultDownloadDir());
        auto peers = m_manager->discoveryService()->activeDevices();
        m_sendPage->updateDeviceList(peers);
        m_receivePage->updateDiscoveredSenders(peers);
        m_devicesPage->updateDiscoveredDevices(peers);
    }
}

void MainWindow::setupUi() {
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout* rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Sidebar
    QFrame* sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("SidebarFrame"));
    QVBoxLayout* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(16, 24, 16, 24);
    sideLayout->setSpacing(8);

    // Branding header
    QHBoxLayout* brandLayout = new QHBoxLayout();
    QLabel* brandIcon = new QLabel(QStringLiteral("⚡"), sidebar);
    brandIcon->setStyleSheet("font-size: 22px;");
    QLabel* brandTitle = new QLabel(QStringLiteral("FastTransfer"), sidebar);
    brandTitle->setObjectName(QStringLiteral("AppTitleLabel"));

    brandLayout->addWidget(brandIcon);
    brandLayout->addWidget(brandTitle);
    brandLayout->addStretch();
    sideLayout->addLayout(brandLayout);

    QLabel* versionLbl = new QLabel(QStringLiteral("v1.0.0 • Ethernet M2M"), sidebar);
    versionLbl->setStyleSheet("color: #64748B; font-size: 11px; padding-left: 6px; margin-bottom: 16px;");
    sideLayout->addWidget(versionLbl);

    // Nav Buttons
    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);

    auto createNavBtn = [this, sidebar](const QString& text, int id) -> QPushButton* {
        QPushButton* btn = new QPushButton(text, sidebar);
        btn->setObjectName(QStringLiteral("NavButton"));
        btn->setCheckable(true);
        m_navGroup->addButton(btn, id);
        return btn;
    };

    m_btnSend = createNavBtn(QStringLiteral("📤  Send Files"), 0);
    m_btnReceive = createNavBtn(QStringLiteral("📥  Receive"), 1);
    m_btnTransfers = createNavBtn(QStringLiteral("⚡  Transfers"), 2);
    m_btnHistory = createNavBtn(QStringLiteral("📜  History"), 3);
    m_btnDevices = createNavBtn(QStringLiteral("🖥️  Devices"), 4);
    m_btnSettings = createNavBtn(QStringLiteral("⚙️  Settings"), 5);

    m_btnSend->setChecked(true);

    sideLayout->addWidget(m_btnSend);
    sideLayout->addWidget(m_btnReceive);
    sideLayout->addWidget(m_btnTransfers);
    sideLayout->addWidget(m_btnHistory);
    sideLayout->addWidget(m_btnDevices);
    sideLayout->addStretch();
    sideLayout->addWidget(m_btnSettings);

    rootLayout->addWidget(sidebar);

    // Pages Stack
    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->setObjectName(QStringLiteral("ContentFrame"));

    m_sendPage = new SendPage(m_stackedWidget);
    m_receivePage = new ReceivePage(m_stackedWidget);
    m_transferPage = new TransferPage(m_stackedWidget);
    m_historyPage = new HistoryPage(m_manager ? m_manager->database() : nullptr, m_stackedWidget);
    m_devicesPage = new DevicesPage(m_manager ? m_manager->discoveryService() : nullptr, m_stackedWidget);
    m_settingsPage = new SettingsPage(m_manager, m_stackedWidget);

    m_stackedWidget->addWidget(m_sendPage);      // 0
    m_stackedWidget->addWidget(m_receivePage);   // 1
    m_stackedWidget->addWidget(m_transferPage);  // 2
    m_stackedWidget->addWidget(m_historyPage);   // 3
    m_stackedWidget->addWidget(m_devicesPage);   // 4
    m_stackedWidget->addWidget(m_settingsPage);  // 5

    rootLayout->addWidget(m_stackedWidget, 1);
}

void MainWindow::connectSignals() {
    connect(m_navGroup, &QButtonGroup::idClicked, this, &MainWindow::onNavButtonClicked);

    connect(m_sendPage, &SendPage::sendRequested, this, &MainWindow::onSendRequested);

    connect(m_receivePage, &ReceivePage::downloadDirChanged, this, [this](const QString& dir) {
        if (m_manager) m_manager->setDefaultDownloadDir(dir);
    });
    connect(m_receivePage, &ReceivePage::offerAccepted, this, &MainWindow::onOfferAccepted);
    connect(m_receivePage, &ReceivePage::offerRejected, this, &MainWindow::onOfferRejected);

    connect(m_devicesPage, &DevicesPage::sendToDeviceRequested, this, [this](const QHostAddress& ip, uint16_t port) {
        m_btnSend->setChecked(true);
        m_stackedWidget->setCurrentIndex(0);
        // Page switched to send, user can pick files and target is ready
        Q_UNUSED(ip); Q_UNUSED(port);
    });

    connect(m_settingsPage, &SettingsPage::themeChanged, this, [this](bool isDark) {
        if (isDark) {
            ThemeManager::applyDarkTheme(*qApp);
        } else {
            ThemeManager::applyLightTheme(*qApp);
        }
    });

    if (m_manager) {
        connect(m_manager, &TransferManager::incomingTransferOffered, this, &MainWindow::onIncomingTransferOffered);
        connect(m_manager, &TransferManager::sessionStarted, this, &MainWindow::onSessionStarted);
        connect(m_manager, &TransferManager::sessionCompleted, this, &MainWindow::onSessionCompleted);

        connect(m_manager->discoveryService(), &DiscoveryService::devicesChanged, this, [this]() {
            auto devices = m_manager->discoveryService()->activeDevices();
            m_sendPage->updateDeviceList(devices);
            m_receivePage->updateDiscoveredSenders(devices);
            m_devicesPage->updateDiscoveredDevices(devices);
        });

        connect(m_receivePage, &ReceivePage::autoAcceptToggled, m_manager, &TransferManager::setAutoAccept);
        m_receivePage->setAutoAccept(m_manager->autoAccept());

        connect(m_manager, &TransferManager::historyUpdated, this, [this]() {
            m_historyPage->refreshHistory();
        });
    }
}

void MainWindow::onNavButtonClicked(int id) {
    m_stackedWidget->setCurrentIndex(id);
}

void MainWindow::onSendRequested(const QHostAddress& ip, uint16_t port, const QStringList& paths) {
    if (!m_manager) return;

    TransferSession* session = m_manager->initiateSend(ip, port, paths);
    if (session) {
        m_transferPage->setSession(session);
        m_btnTransfers->setChecked(true);
        m_stackedWidget->setCurrentIndex(2); // Switch to Active Transfers page
    }
}

void MainWindow::onIncomingTransferOffered(TransferSession* session, const QString& senderDevice, uint64_t totalFiles, uint64_t totalBytes) {
    m_currentOfferSession = session;
    m_receivePage->showIncomingOffer(senderDevice, totalFiles, totalBytes);
    m_btnReceive->setChecked(true);
    m_stackedWidget->setCurrentIndex(1); // Switch to Receive page so user can approve
}

void MainWindow::onOfferAccepted() {
    if (m_manager && m_currentOfferSession) {
        m_manager->acceptIncomingTransfer(m_currentOfferSession);
        m_receivePage->hideIncomingOffer();
        m_transferPage->setSession(m_currentOfferSession);
        m_btnTransfers->setChecked(true);
        m_stackedWidget->setCurrentIndex(2); // Switch to Active Transfers page
        m_currentOfferSession = nullptr;
    }
}

void MainWindow::onOfferRejected() {
    if (m_manager && m_currentOfferSession) {
        m_manager->rejectIncomingTransfer(m_currentOfferSession, QStringLiteral("User rejected transfer request"));
        m_receivePage->hideIncomingOffer();
        m_currentOfferSession = nullptr;
    }
}

void MainWindow::onSessionStarted(TransferSession* session) {
    if (session) {
        connect(session, &TransferSession::metricsUpdated, m_transferPage, &TransferPage::updateMetrics);
        connect(session, &TransferSession::statusChanged, m_transferPage, &TransferPage::updateStatus);
        connect(session, &TransferSession::speedSampleRecorded, m_transferPage, &TransferPage::addSpeedSample);

        if (m_manager && m_manager->autoAccept() && session->direction() == TransferDirection::Receive) {
            m_transferPage->setSession(session);
            m_btnTransfers->setChecked(true);
            m_stackedWidget->setCurrentIndex(2);
        }
    }
}

void MainWindow::onSessionCompleted(TransferSession* session, bool success) {
    Q_UNUSED(session);
    if (success) {
        // Can trigger sound / tray notification
    }
}

} // namespace FastTransfer
