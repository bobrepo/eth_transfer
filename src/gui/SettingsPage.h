#ifndef FASTTRANSFER_SETTINGS_PAGE_H
#define FASTTRANSFER_SETTINGS_PAGE_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include "../core/TransferManager.h"

namespace FastTransfer {

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(TransferManager* manager, QWidget* parent = nullptr);
    ~SettingsPage() override = default;

signals:
    void themeChanged(bool isDark);

private slots:
    void onSaveClicked();
    void onBrowseFolderClicked();
    void onOpenLogsFolderClicked();

private:
    TransferManager* m_manager = nullptr;

    QLineEdit* m_deviceNameEdit = nullptr;
    QComboBox* m_interfacePolicyCombo = nullptr;
    QComboBox* m_chunkSizeCombo = nullptr;
    QComboBox* m_duplicatePolicyCombo = nullptr;
    QLineEdit* m_downloadDirEdit = nullptr;
    QCheckBox* m_darkModeCheck = nullptr;
};

} // namespace FastTransfer

#endif // FASTTRANSFER_SETTINGS_PAGE_H
