#ifndef FASTTRANSFER_HISTORY_PAGE_H
#define FASTTRANSFER_HISTORY_PAGE_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include "../database/Database.h"

namespace FastTransfer {

class HistoryPage : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPage(Database* db, QWidget* parent = nullptr);
    ~HistoryPage() override = default;

    void refreshHistory();

signals:
    void historyCleared();

private slots:
    void onClearClicked();

private:
    Database* m_database = nullptr;
    QTableWidget* m_table = nullptr;
};

} // namespace FastTransfer

#endif // FASTTRANSFER_HISTORY_PAGE_H
