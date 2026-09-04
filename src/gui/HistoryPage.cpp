#include "HistoryPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>
#include "../core/TransferTypes.h"

namespace FastTransfer {

HistoryPage::HistoryPage(Database* db, QWidget* parent)
    : QWidget(parent)
    , m_database(db) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    QVBoxLayout* titleCol = new QVBoxLayout();

    QLabel* title = new QLabel(QStringLiteral("Transfer History"), this);
    title->setProperty("class", "PageHeaderTitle");
    QLabel* subtitle = new QLabel(QStringLiteral("Log of all past and interrupted file transfer sessions stored in local SQLite database."), this);
    subtitle->setProperty("class", "PageHeaderSubtitle");

    titleCol->addWidget(title);
    titleCol->addWidget(subtitle);
    headerLayout->addLayout(titleCol);
    headerLayout->addStretch();

    QPushButton* clearBtn = new QPushButton(QStringLiteral("Clear History"), this);
    clearBtn->setObjectName(QStringLiteral("DangerButton"));
    headerLayout->addWidget(clearBtn);

    mainLayout->addLayout(headerLayout);

    // History Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Type"),
        QStringLiteral("Remote Device"),
        QStringLiteral("IP Address"),
        QStringLiteral("Files"),
        QStringLiteral("Size"),
        QStringLiteral("Status"),
        QStringLiteral("Date & Time")
    });

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    mainLayout->addWidget(m_table, 1);

    connect(clearBtn, &QPushButton::clicked, this, &HistoryPage::onClearClicked);

    refreshHistory();
}

void HistoryPage::refreshHistory() {
    if (!m_database) return;

    auto records = m_database->getTransferHistory(150);
    m_table->setRowCount(records.size());

    for (int r = 0; r < records.size(); ++r) {
        const auto& rec = records[r];

        QString typeStr = (rec.direction == "SEND") ? QStringLiteral("📤 Send") : QStringLiteral("📥 Receive");
        QTableWidgetItem* itemType = new QTableWidgetItem(typeStr);
        QTableWidgetItem* itemDevice = new QTableWidgetItem(rec.remoteDevice);
        QTableWidgetItem* itemIp = new QTableWidgetItem(rec.remoteIp);
        QTableWidgetItem* itemFiles = new QTableWidgetItem(QString::number(rec.totalFiles));
        QTableWidgetItem* itemSize = new QTableWidgetItem(formatBytes(rec.totalBytes));

        QTableWidgetItem* itemStatus = new QTableWidgetItem(rec.status);
        if (rec.status == "Completed") {
            itemStatus->setForeground(QColor("#10B981")); // Green
        } else if (rec.status == "Failed") {
            itemStatus->setForeground(QColor("#EF4444")); // Red
        } else {
            itemStatus->setForeground(QColor("#F59E0B")); // Amber
        }

        QTableWidgetItem* itemDate = new QTableWidgetItem(rec.startTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));

        m_table->setItem(r, 0, itemType);
        m_table->setItem(r, 1, itemDevice);
        m_table->setItem(r, 2, itemIp);
        m_table->setItem(r, 3, itemFiles);
        m_table->setItem(r, 4, itemSize);
        m_table->setItem(r, 5, itemStatus);
        m_table->setItem(r, 6, itemDate);
    }
}

void HistoryPage::onClearClicked() {
    if (!m_database) return;

    auto ret = QMessageBox::question(this, QStringLiteral("Clear Transfer History"),
                                     QStringLiteral("Are you sure you want to delete all transfer history records?"),
                                     QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        m_database->clearHistory();
        refreshHistory();
        emit historyCleared();
    }
}

} // namespace FastTransfer
