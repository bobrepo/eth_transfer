#include "SendPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QHeaderView>
#include <QMessageBox>
#include "../filesystem/FileEnumerator.h"

namespace FastTransfer {

SendPage::SendPage(QWidget* parent)
    : QWidget(parent) {
    setAcceptDrops(true);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(18);

    // Header
    QLabel* title = new QLabel(QStringLiteral("Send Files & Folders"), this);
    title->setProperty("class", "PageHeaderTitle");
    QLabel* subtitle = new QLabel(QStringLiteral("Transfer large files or entire directory trees directly over high-speed Ethernet."), this);
    subtitle->setProperty("class", "PageHeaderSubtitle");

    mainLayout->addWidget(title);
    mainLayout->addWidget(subtitle);

    // Top Action Bar & Drag Zone
    QHBoxLayout* actionLayout = new QHBoxLayout();

    QPushButton* addFilesBtn = new QPushButton(QStringLiteral("+ Add Files"), this);
    QPushButton* addFolderBtn = new QPushButton(QStringLiteral("+ Add Folder"), this);
    m_removeBtn = new QPushButton(QStringLiteral("Remove"), this);
    m_clearBtn = new QPushButton(QStringLiteral("Clear All"), this);

    m_removeBtn->setEnabled(false);
    m_clearBtn->setEnabled(false);

    actionLayout->addWidget(addFilesBtn);
    actionLayout->addWidget(addFolderBtn);
    actionLayout->addWidget(m_removeBtn);
    actionLayout->addWidget(m_clearBtn);
    actionLayout->addStretch();

    mainLayout->addLayout(actionLayout);

    // Items Tree Widget
    m_itemsTree = new QTreeWidget(this);
    m_itemsTree->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Type"), QStringLiteral("Size"), QStringLiteral("Path")});
    m_itemsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_itemsTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_itemsTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_itemsTree->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_itemsTree->setSelectionMode(QAbstractItemView::ExtendedSelection);

    mainLayout->addWidget(m_itemsTree, 1);

    // Summary Bar
    QFrame* summaryCard = new QFrame(this);
    summaryCard->setProperty("class", "CardFrame");
    QHBoxLayout* summaryLayout = new QHBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(14, 10, 14, 10);

    m_summaryLabel = new QLabel(QStringLiteral("Total: 0 files (0 B)"), summaryCard);
    m_summaryLabel->setStyleSheet("font-weight: 600; font-size: 14px; color: #38BDF8;");
    summaryLayout->addWidget(m_summaryLabel);
    summaryLayout->addStretch();

    mainLayout->addWidget(summaryCard);

    // Destination Device Selection Card
    QFrame* destCard = new QFrame(this);
    destCard->setProperty("class", "CardFrame");
    QVBoxLayout* destLayout = new QVBoxLayout(destCard);
    destLayout->setSpacing(12);

    QLabel* destTitle = new QLabel(QStringLiteral("Destination Device"), destCard);
    destTitle->setProperty("class", "SectionTitle");
    destLayout->addWidget(destTitle);

    QHBoxLayout* deviceSelectLayout = new QHBoxLayout();
    m_deviceCombo = new QComboBox(destCard);
    m_deviceCombo->setMinimumWidth(280);
    deviceSelectLayout->addWidget(m_deviceCombo);

    m_manualIpCheck = new QCheckBox(QStringLiteral("Manual IP Fallback"), destCard);
    deviceSelectLayout->addWidget(m_manualIpCheck);

    m_manualIpEdit = new QLineEdit(destCard);
    m_manualIpEdit->setPlaceholderText(QStringLiteral("e.g. 192.168.10.2"));
    m_manualIpEdit->setEnabled(false);
    deviceSelectLayout->addWidget(m_manualIpEdit);

    m_manualPortEdit = new QLineEdit(QString::number(DEFAULT_TCP_PORT), destCard);
    m_manualPortEdit->setMaximumWidth(70);
    m_manualPortEdit->setEnabled(false);
    deviceSelectLayout->addWidget(m_manualPortEdit);

    deviceSelectLayout->addStretch();
    destLayout->addLayout(deviceSelectLayout);

    // Start Button
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    m_startBtn = new QPushButton(QStringLiteral("START TRANSFER"), this);
    m_startBtn->setObjectName(QStringLiteral("PrimaryButton"));
    m_startBtn->setEnabled(false);
    m_startBtn->setMinimumWidth(180);
    m_startBtn->setMinimumHeight(42);
    bottomLayout->addWidget(m_startBtn);

    destLayout->addLayout(bottomLayout);
    mainLayout->addWidget(destCard);

    // Connect events
    connect(addFilesBtn, &QPushButton::clicked, this, &SendPage::onAddFilesClicked);
    connect(addFolderBtn, &QPushButton::clicked, this, &SendPage::onAddFolderClicked);
    connect(m_removeBtn, &QPushButton::clicked, this, &SendPage::onRemoveSelectedClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &SendPage::onClearAllClicked);
    connect(m_startBtn, &QPushButton::clicked, this, &SendPage::onStartTransferClicked);
    connect(m_manualIpCheck, &QCheckBox::toggled, this, &SendPage::onManualIpToggled);

    connect(m_itemsTree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        m_removeBtn->setEnabled(!m_itemsTree->selectedItems().isEmpty());
    });
}

void SendPage::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void SendPage::dropEvent(QDropEvent* event) {
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        QString local = url.toLocalFile();
        if (!local.isEmpty() && QFile::exists(local)) {
            paths.append(local);
        }
    }
    if (!paths.isEmpty()) {
        addPaths(paths);
        event->acceptProposedAction();
    }
}

void SendPage::onAddFilesClicked() {
    QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("Select Files to Transfer"));
    if (!files.isEmpty()) {
        addPaths(files);
    }
}

void SendPage::onAddFolderClicked() {
    QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Folder to Transfer"));
    if (!folder.isEmpty()) {
        addPaths(QStringList() << folder);
    }
}

void SendPage::addPaths(const QStringList& paths) {
    for (const QString& p : paths) {
        if (!m_selectedPaths.contains(p)) {
            m_selectedPaths.append(p);
        }
    }

    m_itemsTree->clear();
    FileManifest manifest = FileEnumerator::enumeratePaths(m_selectedPaths);

    for (const auto& item : manifest.items) {
        QTreeWidgetItem* treeItem = new QTreeWidgetItem(m_itemsTree);
        treeItem->setText(0, QFileInfo(item.relativePath).fileName());
        treeItem->setText(1, QFileInfo(item.sourceAbsolutePath).isDir() ? QStringLiteral("Folder") : QStringLiteral("File"));
        treeItem->setText(2, formatBytes(item.fileSize));
        treeItem->setText(3, item.relativePath);
        treeItem->setData(0, Qt::UserRole, item.sourceAbsolutePath);
    }

    m_totalFiles = manifest.totalFiles;
    m_totalBytes = manifest.totalBytes;
    updateSummary();
}

void SendPage::onRemoveSelectedClicked() {
    auto selected = m_itemsTree->selectedItems();
    for (auto* item : selected) {
        QString fullPath = item->data(0, Qt::UserRole).toString();
        m_selectedPaths.removeAll(fullPath);
    }
    addPaths(QStringList()); // Re-evaluate
}

void SendPage::onClearAllClicked() {
    m_selectedPaths.clear();
    m_itemsTree->clear();
    m_totalFiles = 0;
    m_totalBytes = 0;
    updateSummary();
}

void SendPage::updateSummary() {
    m_summaryLabel->setText(QString("Total: %1 files (%2)").arg(m_totalFiles).arg(formatBytes(m_totalBytes)));
    m_clearBtn->setEnabled(!m_selectedPaths.isEmpty());

    bool hasTarget = false;
    if (m_manualIpCheck->isChecked()) {
        hasTarget = !m_manualIpEdit->text().trimmed().isEmpty();
    } else {
        hasTarget = (m_deviceCombo->count() > 0 && m_deviceCombo->currentIndex() >= 0);
    }

    m_startBtn->setEnabled(!m_selectedPaths.isEmpty() && hasTarget);
}

void SendPage::onManualIpToggled(bool checked) {
    m_manualIpEdit->setEnabled(checked);
    m_manualPortEdit->setEnabled(checked);
    m_deviceCombo->setEnabled(!checked);
    updateSummary();
}

void SendPage::updateDeviceList(const QList<DiscoveredDevice>& devices) {
    m_cachedDevices = devices;
    QString currentSelection = m_deviceCombo->currentData().toString();
    m_deviceCombo->clear();

    for (const auto& dev : devices) {
        QString speed = dev.linkSpeedMbps > 0 ? QString(" • %1").arg(dev.formattedSpeed()) : "";
        QString label = QString("🟢 %1 (%2 — %3%4)")
                            .arg(dev.deviceName, dev.ip.toString(), dev.interfaceType, speed);
        m_deviceCombo->addItem(label, dev.id);
    }

    int idx = m_deviceCombo->findData(currentSelection);
    if (idx >= 0) {
        m_deviceCombo->setCurrentIndex(idx);
    }

    updateSummary();
}

void SendPage::onStartTransferClicked() {
    if (m_selectedPaths.isEmpty()) return;

    QHostAddress targetIp;
    uint16_t port = DEFAULT_TCP_PORT;

    if (m_manualIpCheck->isChecked()) {
        QString ipText = m_manualIpEdit->text().trimmed();
        if (!targetIp.setAddress(ipText)) {
            QMessageBox::warning(this, QStringLiteral("Invalid IP Address"),
                                 QStringLiteral("Please enter a valid IPv4 address for the target computer."));
            return;
        }
        port = static_cast<uint16_t>(m_manualPortEdit->text().toUShort());
        if (port == 0) port = DEFAULT_TCP_PORT;
    } else {
        int idx = m_deviceCombo->currentIndex();
        if (idx < 0 || idx >= m_cachedDevices.size()) {
            QMessageBox::warning(this, QStringLiteral("No Destination Selected"),
                                 QStringLiteral("Please select an available destination computer from the list or enter IP manually."));
            return;
        }
        const auto& dev = m_cachedDevices[idx];
        targetIp = dev.ip;
        port = dev.tcpPort;
    }

    emit sendRequested(targetIp, port, m_selectedPaths);
}

} // namespace FastTransfer
