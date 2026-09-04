#ifndef FASTTRANSFER_SEND_PAGE_H
#define FASTTRANSFER_SEND_PAGE_H

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QStringList>
#include "../network/DiscoveryService.h"
#include "../core/TransferTypes.h"

namespace FastTransfer {

class SendPage : public QWidget {
    Q_OBJECT
public:
    explicit SendPage(QWidget* parent = nullptr);
    ~SendPage() override = default;

    void updateDeviceList(const QList<DiscoveredDevice>& devices);

signals:
    void sendRequested(const QHostAddress& targetIp, uint16_t port, const QStringList& filePaths);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onAddFilesClicked();
    void onAddFolderClicked();
    void onRemoveSelectedClicked();
    void onClearAllClicked();
    void onStartTransferClicked();
    void onManualIpToggled(bool checked);

private:
    void addPaths(const QStringList& paths);
    void updateSummary();

    QStringList m_selectedPaths;
    uint64_t m_totalBytes = 0;
    uint64_t m_totalFiles = 0;

    QTreeWidget* m_itemsTree = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QComboBox* m_deviceCombo = nullptr;
    QCheckBox* m_manualIpCheck = nullptr;
    QLineEdit* m_manualIpEdit = nullptr;
    QLineEdit* m_manualPortEdit = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_removeBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;

    QList<DiscoveredDevice> m_cachedDevices;
};

} // namespace FastTransfer

#endif // FASTTRANSFER_SEND_PAGE_H
