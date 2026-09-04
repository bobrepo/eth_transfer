#ifndef FASTTRANSFER_RECEIVE_PAGE_H
#define FASTTRANSFER_RECEIVE_PAGE_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QFrame>
#include <QCheckBox>
#include "../network/DiscoveryService.h"
#include "../core/TransferTypes.h"

namespace FastTransfer {

class ReceivePage : public QWidget {
    Q_OBJECT
public:
    explicit ReceivePage(QWidget* parent = nullptr);
    ~ReceivePage() override = default;

    void setDownloadDir(const QString& dir);
    void updateDiscoveredSenders(const QList<DiscoveredDevice>& senders);

    // Show incoming offer prompt
    void showIncomingOffer(const QString& senderDevice, uint64_t totalFiles, uint64_t totalBytes);
    void hideIncomingOffer();

    bool isAutoAcceptEnabled() const;
    void setAutoAccept(bool enabled);

signals:
    void downloadDirChanged(const QString& newDir);
    void autoAcceptToggled(bool enabled);
    void offerAccepted();
    void offerRejected();

private slots:
    void onChangeFolderClicked();
    void onOpenFolderClicked();

private:
    void updateDiskSpace();

    QString m_downloadDir;

    QLabel* m_folderPathLabel = nullptr;
    QLabel* m_freeSpaceLabel = nullptr;
    QListWidget* m_sendersList = nullptr;

    // Incoming offer card
    QFrame* m_offerCard = nullptr;
    QLabel* m_offerSenderLabel = nullptr;
    QLabel* m_offerStatsLabel = nullptr;
    QLabel* m_offerDestLabel = nullptr;
    QCheckBox* m_autoAcceptCheck = nullptr;
};

} // namespace FastTransfer

#endif // FASTTRANSFER_RECEIVE_PAGE_H
