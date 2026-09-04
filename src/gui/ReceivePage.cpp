#include "ReceivePage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include "../platform/PlatformFilesystem.h"

namespace FastTransfer {

ReceivePage::ReceivePage(QWidget* parent)
    : QWidget(parent) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(20);

    // Page Title
    QLabel* title = new QLabel(QStringLiteral("Receive Transfers"), this);
    title->setProperty("class", "PageHeaderTitle");
    QLabel* subtitle = new QLabel(QStringLiteral("Files received from other devices are automatically placed in a dedicated folder per sender."), this);
    subtitle->setProperty("class", "PageHeaderSubtitle");

    mainLayout->addWidget(title);
    mainLayout->addWidget(subtitle);

    // Incoming Transfer Alert Card (Hidden by default)
    m_offerCard = new QFrame(this);
    m_offerCard->setStyleSheet("background-color: #1A2234; border: 2px solid #2563EB; border-radius: 12px; padding: 18px;");
    QVBoxLayout* offerLayout = new QVBoxLayout(m_offerCard);
    offerLayout->setSpacing(10);

    QLabel* alertHeader = new QLabel(QStringLiteral("🔔 Incoming Transfer Request"), m_offerCard);
    alertHeader->setStyleSheet("font-size: 16px; font-weight: 700; color: #38BDF8;");
    offerLayout->addWidget(alertHeader);

    m_offerSenderLabel = new QLabel(m_offerCard);
    m_offerSenderLabel->setStyleSheet("font-size: 14px; font-weight: 600; color: #F1F5F9;");
    offerLayout->addWidget(m_offerSenderLabel);

    m_offerStatsLabel = new QLabel(m_offerCard);
    m_offerStatsLabel->setStyleSheet("font-size: 13px; color: #94A3B8;");
    offerLayout->addWidget(m_offerStatsLabel);

    m_offerDestLabel = new QLabel(m_offerCard);
    m_offerDestLabel->setStyleSheet("font-size: 12px; color: #64748B; font-family: monospace;");
    offerLayout->addWidget(m_offerDestLabel);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* acceptBtn = new QPushButton(QStringLiteral("ACCEPT TRANSFER"), m_offerCard);
    acceptBtn->setObjectName(QStringLiteral("SuccessButton"));
    QPushButton* rejectBtn = new QPushButton(QStringLiteral("REJECT"), m_offerCard);
    rejectBtn->setObjectName(QStringLiteral("DangerButton"));

    btnLayout->addWidget(acceptBtn);
    btnLayout->addWidget(rejectBtn);
    btnLayout->addStretch();
    offerLayout->addLayout(btnLayout);

    connect(acceptBtn, &QPushButton::clicked, this, &ReceivePage::offerAccepted);
    connect(rejectBtn, &QPushButton::clicked, this, &ReceivePage::offerRejected);

    m_offerCard->hide();
    mainLayout->addWidget(m_offerCard);

    // Output Directory Card
    QFrame* dirCard = new QFrame(this);
    dirCard->setProperty("class", "CardFrame");
    QVBoxLayout* dirLayout = new QVBoxLayout(dirCard);
    dirLayout->setSpacing(10);

    QLabel* dirTitle = new QLabel(QStringLiteral("Output Storage Location"), dirCard);
    dirTitle->setProperty("class", "SectionTitle");
    dirLayout->addWidget(dirTitle);

    QHBoxLayout* pathRow = new QHBoxLayout();
    m_folderPathLabel = new QLabel(dirCard);
    m_folderPathLabel->setStyleSheet("font-weight: 600; color: #F8FAFC; background-color: #13161D; padding: 10px 14px; border-radius: 8px; border: 1px solid #282D3B;");
    m_folderPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QPushButton* changeFolderBtn = new QPushButton(QStringLiteral("Change..."), dirCard);
    QPushButton* openFolderBtn = new QPushButton(QStringLiteral("Open Folder"), dirCard);

    pathRow->addWidget(m_folderPathLabel, 1);
    pathRow->addWidget(changeFolderBtn);
    pathRow->addWidget(openFolderBtn);
    dirLayout->addLayout(pathRow);

    m_freeSpaceLabel = new QLabel(dirCard);
    m_freeSpaceLabel->setStyleSheet("font-size: 12px; color: #10B981;");
    dirLayout->addWidget(m_freeSpaceLabel);

    m_autoAcceptCheck = new QCheckBox(QStringLiteral("Auto-accept incoming transfers from local Ethernet devices"), dirCard);
    m_autoAcceptCheck->setStyleSheet("font-size: 13px; font-weight: 500; color: #E2E8F0; margin-top: 6px;");
    dirLayout->addWidget(m_autoAcceptCheck);

    connect(m_autoAcceptCheck, &QCheckBox::toggled, this, &ReceivePage::autoAcceptToggled);
    connect(changeFolderBtn, &QPushButton::clicked, this, &ReceivePage::onChangeFolderClicked);
    connect(openFolderBtn, &QPushButton::clicked, this, &ReceivePage::onOpenFolderClicked);

    mainLayout->addWidget(dirCard);

    // Active Status Card
    QFrame* statusCard = new QFrame(this);
    statusCard->setProperty("class", "CardFrame");
    QVBoxLayout* statusLayout = new QVBoxLayout(statusCard);
    statusLayout->setSpacing(12);

    QHBoxLayout* statusHeader = new QHBoxLayout();
    QLabel* listeningDot = new QLabel(QStringLiteral("🟢"), statusCard);
    QLabel* listeningText = new QLabel(QStringLiteral("Listening for incoming connections on TCP Port 45821..."), statusCard);
    listeningText->setStyleSheet("font-weight: 600; color: #10B981; font-size: 14px;");
    statusHeader->addWidget(listeningDot);
    statusHeader->addWidget(listeningText);
    statusHeader->addStretch();
    statusLayout->addLayout(statusHeader);

    QLabel* sendersTitle = new QLabel(QStringLiteral("Available Senders on Local Network"), statusCard);
    sendersTitle->setProperty("class", "SectionTitle");
    statusLayout->addWidget(sendersTitle);

    m_sendersList = new QListWidget(statusCard);
    m_sendersList->setMinimumHeight(150);
    statusLayout->addWidget(m_sendersList);

    mainLayout->addWidget(statusCard, 1);
}

void ReceivePage::setDownloadDir(const QString& dir) {
    m_downloadDir = dir;
    m_folderPathLabel->setText(dir);
    updateDiskSpace();
}

void ReceivePage::updateDiskSpace() {
    uint64_t freeBytes = PlatformFilesystem::getAvailableDiskSpace(m_downloadDir);
    m_freeSpaceLabel->setText(QString("Free Disk Space Available: %1").arg(formatBytes(freeBytes)));
}

void ReceivePage::onChangeFolderClicked() {
    QString selected = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Destination Folder"), m_downloadDir);
    if (!selected.isEmpty() && selected != m_downloadDir) {
        setDownloadDir(selected);
        emit downloadDirChanged(selected);
    }
}

void ReceivePage::onOpenFolderClicked() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadDir));
}

void ReceivePage::updateDiscoveredSenders(const QList<DiscoveredDevice>& senders) {
    m_sendersList->clear();
    for (const auto& s : senders) {
        QString label = QString("🟢 %1  |  IP: %2  |  Interface: %3  |  Link: %4")
                            .arg(s.deviceName, s.ip.toString(), s.interfaceType, s.formattedSpeed());
        m_sendersList->addItem(label);
    }
}

void ReceivePage::showIncomingOffer(const QString& senderDevice, uint64_t totalFiles, uint64_t totalBytes) {
    m_offerSenderLabel->setText(QString("From: %1").arg(senderDevice));
    m_offerStatsLabel->setText(QString("Files: %1  •  Total Size: %2").arg(totalFiles).arg(formatBytes(totalBytes)));
    m_offerDestLabel->setText(QString("Destination: %1/%2_<timestamp>/").arg(m_downloadDir, PlatformFilesystem::sanitizeDeviceName(senderDevice)));
    m_offerCard->show();
}

void ReceivePage::hideIncomingOffer() {
    m_offerCard->hide();
}

bool ReceivePage::isAutoAcceptEnabled() const {
    return m_autoAcceptCheck && m_autoAcceptCheck->isChecked();
}

void ReceivePage::setAutoAccept(bool enabled) {
    if (m_autoAcceptCheck && m_autoAcceptCheck->isChecked() != enabled) {
        m_autoAcceptCheck->setChecked(enabled);
    }
}

} // namespace FastTransfer
