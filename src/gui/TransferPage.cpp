#include "TransferPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

namespace FastTransfer {

TransferPage::TransferPage(QWidget* parent)
    : QWidget(parent) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    // Header with status badge
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* pageTitle = new QLabel(QStringLiteral("Active Transfer"), this);
    pageTitle->setProperty("class", "PageHeaderTitle");

    m_statusBadge = new QLabel(QStringLiteral("IDLE"), this);
    m_statusBadge->setStyleSheet("background-color: #232733; color: #94A3B8; font-weight: 700; font-size: 11px; padding: 4px 10px; border-radius: 6px;");

    headerLayout->addWidget(pageTitle);
    headerLayout->addWidget(m_statusBadge);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    m_remoteInfoLabel = new QLabel(QStringLiteral("No transfer currently in progress."), this);
    m_remoteInfoLabel->setStyleSheet("color: #64748B; font-size: 13px;");
    mainLayout->addWidget(m_remoteInfoLabel);

    // Hero Speed Card
    QFrame* speedCard = new QFrame(this);
    speedCard->setProperty("class", "CardFrame");
    QVBoxLayout* speedCardLayout = new QVBoxLayout(speedCard);
    speedCardLayout->setSpacing(12);

    QHBoxLayout* speedTopRow = new QHBoxLayout();
    QVBoxLayout* bigSpeedCol = new QVBoxLayout();

    QLabel* speedTitle = new QLabel(QStringLiteral("TRANSFER SPEED"), speedCard);
    speedTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #64748B; letter-spacing: 1px;");

    m_largeSpeedLabel = new QLabel(QStringLiteral("0.0 MB/s"), speedCard);
    m_largeSpeedLabel->setStyleSheet("font-size: 42px; font-weight: 800; color: #38BDF8; letter-spacing: -0.5px;");

    bigSpeedCol->addWidget(speedTitle);
    bigSpeedCol->addWidget(m_largeSpeedLabel);
    speedTopRow->addLayout(bigSpeedCol);
    speedTopRow->addStretch();

    // Stats Grid
    QGridLayout* statsGrid = new QGridLayout();
    statsGrid->setHorizontalSpacing(24);
    statsGrid->setVerticalSpacing(6);

    QLabel* curTitle = new QLabel(QStringLiteral("Current:"), speedCard);
    curTitle->setStyleSheet("color: #64748B;");
    m_currentSpeedSub = new QLabel(QStringLiteral("0.0 MB/s"), speedCard);
    m_currentSpeedSub->setStyleSheet("color: #E2E8F0; font-weight: 600;");

    QLabel* avgTitle = new QLabel(QStringLiteral("Average:"), speedCard);
    avgTitle->setStyleSheet("color: #64748B;");
    m_avgSpeedSub = new QLabel(QStringLiteral("0.0 MB/s"), speedCard);
    m_avgSpeedSub->setStyleSheet("color: #E2E8F0; font-weight: 600;");

    QLabel* peakTitle = new QLabel(QStringLiteral("Peak:"), speedCard);
    peakTitle->setStyleSheet("color: #64748B;");
    m_peakSpeedSub = new QLabel(QStringLiteral("0.0 MB/s"), speedCard);
    m_peakSpeedSub->setStyleSheet("color: #E2E8F0; font-weight: 600;");

    QLabel* etaTitle = new QLabel(QStringLiteral("ETA:"), speedCard);
    etaTitle->setStyleSheet("color: #64748B;");
    m_etaLabel = new QLabel(QStringLiteral("Calculating..."), speedCard);
    m_etaLabel->setStyleSheet("color: #38BDF8; font-weight: 700;");

    QLabel* elTitle = new QLabel(QStringLiteral("Elapsed:"), speedCard);
    elTitle->setStyleSheet("color: #64748B;");
    m_elapsedLabel = new QLabel(QStringLiteral("00:00"), speedCard);
    m_elapsedLabel->setStyleSheet("color: #E2E8F0; font-weight: 600;");

    statsGrid->addWidget(curTitle, 0, 0);
    statsGrid->addWidget(m_currentSpeedSub, 0, 1);
    statsGrid->addWidget(avgTitle, 1, 0);
    statsGrid->addWidget(m_avgSpeedSub, 1, 1);
    statsGrid->addWidget(peakTitle, 2, 0);
    statsGrid->addWidget(m_peakSpeedSub, 2, 1);
    statsGrid->addWidget(etaTitle, 0, 2);
    statsGrid->addWidget(m_etaLabel, 0, 3);
    statsGrid->addWidget(elTitle, 1, 2);
    statsGrid->addWidget(m_elapsedLabel, 1, 3);

    speedTopRow->addLayout(statsGrid);
    speedCardLayout->addLayout(speedTopRow);

    // Live Speed Graph
    m_speedGraph = new SpeedGraph(speedCard);
    speedCardLayout->addWidget(m_speedGraph);

    mainLayout->addWidget(speedCard);

    // Dual Progress Bars Card
    QFrame* progressCard = new QFrame(this);
    progressCard->setProperty("class", "CardFrame");
    QVBoxLayout* progLayout = new QVBoxLayout(progressCard);
    progLayout->setSpacing(14);

    // Current File Progress
    QVBoxLayout* curFileLayout = new QVBoxLayout();
    curFileLayout->setSpacing(4);
    QHBoxLayout* curFileHeader = new QHBoxLayout();
    m_currentFileLabel = new QLabel(QStringLiteral("Current File: None"), progressCard);
    m_currentFileLabel->setStyleSheet("font-weight: 600; color: #F1F5F9;");
    m_currentFileStats = new QLabel(QStringLiteral("0 B / 0 B"), progressCard);
    m_currentFileStats->setStyleSheet("color: #94A3B8; font-size: 12px;");

    curFileHeader->addWidget(m_currentFileLabel);
    curFileHeader->addStretch();
    curFileHeader->addWidget(m_currentFileStats);
    curFileLayout->addLayout(curFileHeader);

    m_currentFileBar = new QProgressBar(progressCard);
    m_currentFileBar->setRange(0, 1000);
    m_currentFileBar->setValue(0);
    curFileLayout->addWidget(m_currentFileBar);
    progLayout->addLayout(curFileLayout);

    // Overall Progress
    QVBoxLayout* overallLayout = new QVBoxLayout();
    overallLayout->setSpacing(4);
    QHBoxLayout* overallHeader = new QHBoxLayout();
    m_overallLabel = new QLabel(QStringLiteral("Overall Progress"), progressCard);
    m_overallLabel->setStyleSheet("font-weight: 600; color: #F1F5F9;");
    m_overallStats = new QLabel(QStringLiteral("0 B / 0 B (0%)"), progressCard);
    m_overallStats->setStyleSheet("color: #94A3B8; font-size: 12px;");

    overallHeader->addWidget(m_overallLabel);
    overallHeader->addStretch();
    overallHeader->addWidget(m_overallStats);
    overallLayout->addLayout(overallHeader);

    m_overallBar = new QProgressBar(progressCard);
    m_overallBar->setRange(0, 1000);
    m_overallBar->setValue(0);
    overallLayout->addWidget(m_overallBar);
    progLayout->addLayout(overallLayout);

    mainLayout->addWidget(progressCard);

    // Bottom Action Row
    QHBoxLayout* actionRow = new QHBoxLayout();
    m_integrityLabel = new QLabel(QStringLiteral("🛡️ BLAKE3 Integrity Verification Active"), this);
    m_integrityLabel->setStyleSheet("color: #10B981; font-weight: 600; font-size: 12px;");
    actionRow->addWidget(m_integrityLabel);
    actionRow->addStretch();

    m_pauseResumeBtn = new QPushButton(QStringLiteral("PAUSE"), this);
    m_pauseResumeBtn->setEnabled(false);
    m_cancelBtn = new QPushButton(QStringLiteral("CANCEL"), this);
    m_cancelBtn->setObjectName(QStringLiteral("DangerButton"));
    m_cancelBtn->setEnabled(false);

    actionRow->addWidget(m_pauseResumeBtn);
    actionRow->addWidget(m_cancelBtn);
    mainLayout->addLayout(actionRow);

    connect(m_pauseResumeBtn, &QPushButton::clicked, this, &TransferPage::onPauseResumeClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &TransferPage::onCancelClicked);
}

void TransferPage::setSession(TransferSession* session) {
    m_session = session;
    if (m_session) {
        updateStatus(m_session->status());
        m_remoteInfoLabel->setText(QString("Remote Computer: %1 (%2)")
                                       .arg(m_session->remoteDevice(), m_session->remoteIp().toString()));
        m_pauseResumeBtn->setEnabled(true);
        m_cancelBtn->setEnabled(true);
    } else {
        m_remoteInfoLabel->setText(QStringLiteral("No transfer currently in progress."));
        m_pauseResumeBtn->setEnabled(false);
        m_cancelBtn->setEnabled(false);
    }
}

void TransferPage::updateStatus(TransferStatus status) {
    switch (status) {
        case TransferStatus::Transferring:
            m_statusBadge->setText(QStringLiteral("TRANSFERRING"));
            m_statusBadge->setStyleSheet("background-color: #1E3A8A; color: #60A5FA; font-weight: 700; font-size: 11px; padding: 4px 10px; border-radius: 6px;");
            m_pauseResumeBtn->setText(QStringLiteral("PAUSE"));
            m_pauseResumeBtn->setEnabled(true);
            m_cancelBtn->setEnabled(true);
            break;
        case TransferStatus::Paused:
            m_statusBadge->setText(QStringLiteral("PAUSED"));
            m_statusBadge->setStyleSheet("background-color: #78350F; color: #FBBF24; font-weight: 700; font-size: 11px; padding: 4px 10px; border-radius: 6px;");
            m_pauseResumeBtn->setText(QStringLiteral("RESUME"));
            m_largeSpeedLabel->setText(QStringLiteral("0.0 MB/s"));
            break;
        case TransferStatus::Completed:
            m_statusBadge->setText(QStringLiteral("COMPLETED"));
            m_statusBadge->setStyleSheet("background-color: #064E3B; color: #34D399; font-weight: 700; font-size: 11px; padding: 4px 10px; border-radius: 6px;");
            m_pauseResumeBtn->setEnabled(false);
            m_cancelBtn->setEnabled(false);
            m_integrityLabel->setText(QStringLiteral("✓ Transfer Complete & BLAKE3 Verified"));
            break;
        case TransferStatus::Failed:
            m_statusBadge->setText(QStringLiteral("FAILED"));
            m_statusBadge->setStyleSheet("background-color: #7F1D1D; color: #F87171; font-weight: 700; font-size: 11px; padding: 4px 10px; border-radius: 6px;");
            m_pauseResumeBtn->setEnabled(false);
            m_cancelBtn->setEnabled(false);
            break;
        case TransferStatus::Cancelled:
            m_statusBadge->setText(QStringLiteral("CANCELLED"));
            m_statusBadge->setStyleSheet("background-color: #232733; color: #94A3B8; font-weight: 700; font-size: 11px; padding: 4px 10px; border-radius: 6px;");
            m_pauseResumeBtn->setEnabled(false);
            m_cancelBtn->setEnabled(false);
            break;
        default:
            break;
    }
}

void TransferPage::updateMetrics(const TransferMetrics& metrics) {
    m_largeSpeedLabel->setText(metrics.formattedCurrentSpeed());
    m_currentSpeedSub->setText(metrics.formattedCurrentSpeed());
    m_avgSpeedSub->setText(metrics.formattedAverageSpeed());
    m_peakSpeedSub->setText(metrics.formattedPeakSpeed());
    m_etaLabel->setText(metrics.formattedEta());
    m_elapsedLabel->setText(metrics.formattedElapsed());

    // Current File
    if (!metrics.currentFileName.isEmpty()) {
        m_currentFileLabel->setText(QString("File %1 of %2: %3")
                                        .arg(metrics.currentFileIndex)
                                        .arg(metrics.totalFiles)
                                        .arg(metrics.currentFileName));
    }
    m_currentFileStats->setText(QString("%1 / %2 (%3%)")
                                    .arg(formatBytes(metrics.currentFileTransferred),
                                         formatBytes(metrics.currentFileSize),
                                         QString::number(metrics.currentFileProgressRatio() * 100.0, 'f', 1)));
    m_currentFileBar->setValue(static_cast<int>(metrics.currentFileProgressRatio() * 1000.0));

    // Overall
    m_overallStats->setText(QString("%1 / %2 (%3%)")
                                .arg(formatBytes(metrics.totalTransferred),
                                     formatBytes(metrics.totalBytes),
                                     QString::number(metrics.overallProgressRatio() * 100.0, 'f', 1)));
    m_overallBar->setValue(static_cast<int>(metrics.overallProgressRatio() * 1000.0));
}

void TransferPage::addSpeedSample(double speedBps) {
    if (m_speedGraph) {
        m_speedGraph->addSpeedSample(speedBps);
    }
}

void TransferPage::onPauseResumeClicked() {
    if (!m_session) return;
    if (m_session->status() == TransferStatus::Transferring) {
        m_session->pause();
    } else if (m_session->status() == TransferStatus::Paused) {
        m_session->resume();
    }
}

void TransferPage::onCancelClicked() {
    if (m_session) {
        m_session->cancel();
    }
}

} // namespace FastTransfer
