#ifndef FASTTRANSFER_MAIN_WINDOW_H
#define FASTTRANSFER_MAIN_WINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QButtonGroup>
#include "../core/TransferManager.h"
#include "SendPage.h"
#include "ReceivePage.h"
#include "TransferPage.h"
#include "HistoryPage.h"
#include "DevicesPage.h"
#include "SettingsPage.h"

namespace FastTransfer {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(TransferManager* manager, QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onNavButtonClicked(int id);
    void onSendRequested(const QHostAddress& ip, uint16_t port, const QStringList& paths);
    void onIncomingTransferOffered(TransferSession* session, const QString& senderDevice, uint64_t totalFiles, uint64_t totalBytes);
    void onOfferAccepted();
    void onOfferRejected();
    void onSessionStarted(TransferSession* session);
    void onSessionCompleted(TransferSession* session, bool success);

private:
    void setupUi();
    void connectSignals();

    TransferManager* m_manager = nullptr;

    QStackedWidget* m_stackedWidget = nullptr;
    QButtonGroup* m_navGroup = nullptr;

    QPushButton* m_btnSend = nullptr;
    QPushButton* m_btnReceive = nullptr;
    QPushButton* m_btnTransfers = nullptr;
    QPushButton* m_btnHistory = nullptr;
    QPushButton* m_btnDevices = nullptr;
    QPushButton* m_btnSettings = nullptr;

    SendPage* m_sendPage = nullptr;
    ReceivePage* m_receivePage = nullptr;
    TransferPage* m_transferPage = nullptr;
    HistoryPage* m_historyPage = nullptr;
    DevicesPage* m_devicesPage = nullptr;
    SettingsPage* m_settingsPage = nullptr;

    TransferSession* m_currentOfferSession = nullptr;
};

} // namespace FastTransfer

#endif // FASTTRANSFER_MAIN_WINDOW_H
