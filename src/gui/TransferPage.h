#ifndef FASTTRANSFER_TRANSFER_PAGE_H
#define FASTTRANSFER_TRANSFER_PAGE_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QFrame>
#include "SpeedGraph.h"
#include "../core/TransferTypes.h"
#include "../core/TransferSession.h"

namespace FastTransfer {

class TransferPage : public QWidget {
    Q_OBJECT
public:
    explicit TransferPage(QWidget* parent = nullptr);
    ~TransferPage() override = default;

    void setSession(TransferSession* session);
    void updateMetrics(const TransferMetrics& metrics);
    void updateStatus(TransferStatus status);
    void addSpeedSample(double speedBps);

signals:
    void pauseRequested();
    void resumeRequested();
    void cancelRequested();
    void acceptOfferClicked();
    void rejectOfferClicked();

private slots:
    void onPauseResumeClicked();
    void onCancelClicked();

private:
    TransferSession* m_session = nullptr;

    QLabel* m_statusBadge = nullptr;
    QLabel* m_remoteInfoLabel = nullptr;

    // Big Speed Display
    QLabel* m_largeSpeedLabel = nullptr;
    QLabel* m_currentSpeedSub = nullptr;
    QLabel* m_avgSpeedSub = nullptr;
    QLabel* m_peakSpeedSub = nullptr;
    QLabel* m_etaLabel = nullptr;
    QLabel* m_elapsedLabel = nullptr;

    SpeedGraph* m_speedGraph = nullptr;

    // Dual Progress Bars
    QLabel* m_currentFileLabel = nullptr;
    QLabel* m_currentFileStats = nullptr;
    QProgressBar* m_currentFileBar = nullptr;

    QLabel* m_overallLabel = nullptr;
    QLabel* m_overallStats = nullptr;
    QProgressBar* m_overallBar = nullptr;

    // Offer inline actions
    QFrame* m_offerActionFrame = nullptr;
    QPushButton* m_acceptOfferBtn = nullptr;
    QPushButton* m_rejectOfferBtn = nullptr;

    // Control buttons
    QPushButton* m_pauseResumeBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QLabel* m_integrityLabel = nullptr;
};

} // namespace FastTransfer

#endif // FASTTRANSFER_TRANSFER_PAGE_H
