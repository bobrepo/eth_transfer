#include "SettingsPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QMessageBox>

namespace FastTransfer {

SettingsPage::SettingsPage(TransferManager* manager, QWidget* parent)
    : QWidget(parent)
    , m_manager(manager) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(18);

    // Page Title
    QLabel* title = new QLabel(QStringLiteral("Settings"), this);
    title->setProperty("class", "PageHeaderTitle");
    QLabel* subtitle = new QLabel(QStringLiteral("Configure device identity, network interface policies, and transfer parameters."), this);
    subtitle->setProperty("class", "PageHeaderSubtitle");

    mainLayout->addWidget(title);
    mainLayout->addWidget(subtitle);

    // Device Settings Card
    QFrame* devCard = new QFrame(this);
    devCard->setProperty("class", "CardFrame");
    QFormLayout* devForm = new QFormLayout(devCard);
    devForm->setSpacing(12);

    QLabel* devTitle = new QLabel(QStringLiteral("Device Identity"), devCard);
    devTitle->setProperty("class", "SectionTitle");
    devForm->addRow(devTitle);

    m_deviceNameEdit = new QLineEdit(m_manager ? m_manager->deviceName() : "", devCard);
    devForm->addRow(QStringLiteral("Device Name:"), m_deviceNameEdit);
    mainLayout->addWidget(devCard);

    // Network Card
    QFrame* netCard = new QFrame(this);
    netCard->setProperty("class", "CardFrame");
    QFormLayout* netForm = new QFormLayout(netCard);
    netForm->setSpacing(12);

    QLabel* netTitle = new QLabel(QStringLiteral("Network & Adapter Policy"), netCard);
    netTitle->setProperty("class", "SectionTitle");
    netForm->addRow(netTitle);

    m_interfacePolicyCombo = new QComboBox(netCard);
    m_interfacePolicyCombo->addItem(QStringLiteral("Automatic (Prefer Highest-Speed Physical Adapter)"), false);
    m_interfacePolicyCombo->addItem(QStringLiteral("Ethernet Only (Strictly Ignore Wi-Fi & Virtual Adapters)"), true);
    if (m_manager && m_manager->isEthernetOnly()) {
        m_interfacePolicyCombo->setCurrentIndex(1);
    }
    netForm->addRow(QStringLiteral("Preferred Interface:"), m_interfacePolicyCombo);
    mainLayout->addWidget(netCard);

    // Transfer & Files Card
    QFrame* transCard = new QFrame(this);
    transCard->setProperty("class", "CardFrame");
    QFormLayout* transForm = new QFormLayout(transCard);
    transForm->setSpacing(12);

    QLabel* transTitle = new QLabel(QStringLiteral("Transfer Engine & Storage"), transCard);
    transTitle->setProperty("class", "SectionTitle");
    transForm->addRow(transTitle);

    m_chunkSizeCombo = new QComboBox(transCard);
    m_chunkSizeCombo->addItem(QStringLiteral("4 MB"), 4 * 1024 * 1024);
    m_chunkSizeCombo->addItem(QStringLiteral("8 MB (Recommended Default)"), 8 * 1024 * 1024);
    m_chunkSizeCombo->addItem(QStringLiteral("16 MB (Optimized for 2.5GbE / 10GbE)"), 16 * 1024 * 1024);
    m_chunkSizeCombo->addItem(QStringLiteral("32 MB"), 32 * 1024 * 1024);
    m_chunkSizeCombo->addItem(QStringLiteral("64 MB"), 64 * 1024 * 1024);

    uint32_t currentChunk = m_manager ? m_manager->chunkSize() : 8 * 1024 * 1024;
    int chunkIdx = m_chunkSizeCombo->findData(currentChunk);
    if (chunkIdx >= 0) m_chunkSizeCombo->setCurrentIndex(chunkIdx);
    transForm->addRow(QStringLiteral("Streaming Buffer Size:"), m_chunkSizeCombo);

    m_duplicatePolicyCombo = new QComboBox(transCard);
    m_duplicatePolicyCombo->addItem(QStringLiteral("Rename automatically, e.g. 'Game (1).zip'"), static_cast<int>(DuplicatePolicy::Rename));
    m_duplicatePolicyCombo->addItem(QStringLiteral("Replace existing files"), static_cast<int>(DuplicatePolicy::Replace));
    m_duplicatePolicyCombo->addItem(QStringLiteral("Skip if file exists"), static_cast<int>(DuplicatePolicy::Skip));

    int currentPolicy = static_cast<int>(m_manager ? m_manager->duplicatePolicy() : DuplicatePolicy::Rename);
    int polIdx = m_duplicatePolicyCombo->findData(currentPolicy);
    if (polIdx >= 0) m_duplicatePolicyCombo->setCurrentIndex(polIdx);
    transForm->addRow(QStringLiteral("Duplicate Policy:"), m_duplicatePolicyCombo);

    QHBoxLayout* dirRow = new QHBoxLayout();
    m_downloadDirEdit = new QLineEdit(m_manager ? m_manager->defaultDownloadDir() : "", transCard);
    QPushButton* browseBtn = new QPushButton(QStringLiteral("Browse..."), transCard);
    dirRow->addWidget(m_downloadDirEdit);
    dirRow->addWidget(browseBtn);
    transForm->addRow(QStringLiteral("Download Directory:"), dirRow);

    connect(browseBtn, &QPushButton::clicked, this, &SettingsPage::onBrowseFolderClicked);

    m_autoAcceptCheck = new QCheckBox(QStringLiteral("Auto-accept incoming transfers from local Ethernet devices (skip approval prompt)"), transCard);
    m_autoAcceptCheck->setChecked(m_manager ? m_manager->autoAccept() : false);
    transForm->addRow(QStringLiteral("Auto-Accept Mode:"), m_autoAcceptCheck);

    connect(m_autoAcceptCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_manager) {
            m_manager->setAutoAccept(checked);
        }
        emit autoAcceptChanged(checked);
    });

    mainLayout->addWidget(transCard);

    // Appearance & Diagnostics Card
    QFrame* diagCard = new QFrame(this);
    diagCard->setProperty("class", "CardFrame");
    QVBoxLayout* diagLayout = new QVBoxLayout(diagCard);
    diagLayout->setSpacing(10);

    QLabel* diagTitle = new QLabel(QStringLiteral("Appearance & Logs"), diagCard);
    diagTitle->setProperty("class", "SectionTitle");
    diagLayout->addWidget(diagTitle);

    QHBoxLayout* diagRow = new QHBoxLayout();
    m_darkModeCheck = new QCheckBox(QStringLiteral("Dark Theme"), diagCard);
    m_darkModeCheck->setChecked(true);
    diagRow->addWidget(m_darkModeCheck);
    diagRow->addStretch();

    QPushButton* openLogsBtn = new QPushButton(QStringLiteral("Open App Data Folder"), diagCard);
    diagRow->addWidget(openLogsBtn);
    diagLayout->addLayout(diagRow);

    connect(m_darkModeCheck, &QCheckBox::toggled, this, &SettingsPage::themeChanged);
    connect(openLogsBtn, &QPushButton::clicked, this, &SettingsPage::onOpenLogsFolderClicked);

    mainLayout->addWidget(diagCard);

    // Save Button
    QHBoxLayout* saveRow = new QHBoxLayout();
    saveRow->addStretch();
    QPushButton* saveBtn = new QPushButton(QStringLiteral("SAVE SETTINGS"), this);
    saveBtn->setObjectName(QStringLiteral("PrimaryButton"));
    saveBtn->setMinimumWidth(160);
    saveRow->addWidget(saveBtn);
    mainLayout->addLayout(saveRow);

    connect(saveBtn, &QPushButton::clicked, this, &SettingsPage::onSaveClicked);
    mainLayout->addStretch();
}

void SettingsPage::onBrowseFolderClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Default Download Folder"), m_downloadDirEdit->text());
    if (!dir.isEmpty()) {
        m_downloadDirEdit->setText(dir);
    }
}

void SettingsPage::onOpenLogsFolderClicked() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void SettingsPage::onSaveClicked() {
    if (!m_manager) return;

    QString name = m_deviceNameEdit->text().trimmed();
    if (!name.isEmpty()) {
        m_manager->setDeviceName(name);
    }

    bool ethOnly = m_interfacePolicyCombo->currentData().toBool();
    m_manager->setEthernetOnly(ethOnly);

    uint32_t chunkBytes = m_chunkSizeCombo->currentData().toUInt();
    m_manager->setChunkSize(chunkBytes);

    int polInt = m_duplicatePolicyCombo->currentData().toInt();
    m_manager->setDuplicatePolicy(static_cast<DuplicatePolicy>(polInt));

    QString downDir = m_downloadDirEdit->text().trimmed();
    if (!downDir.isEmpty()) {
        m_manager->setDefaultDownloadDir(downDir);
    }

    bool autoAccept = m_autoAcceptCheck ? m_autoAcceptCheck->isChecked() : false;
    m_manager->setAutoAccept(autoAccept);
    emit autoAcceptChanged(autoAccept);

    QMessageBox::information(this, QStringLiteral("Settings Saved"),
                             QStringLiteral("Your application configuration preferences have been saved successfully."));
}

void SettingsPage::setAutoAccept(bool enabled) {
    if (m_autoAcceptCheck && m_autoAcceptCheck->isChecked() != enabled) {
        m_autoAcceptCheck->setChecked(enabled);
    }
}

} // namespace FastTransfer
