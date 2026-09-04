#include "ThemeManager.h"
#include <QPalette>
#include <QColor>

namespace FastTransfer {

void ThemeManager::applyDarkTheme(QApplication& app) {
    app.setStyleSheet(darkStyleSheet());
}

void ThemeManager::applyLightTheme(QApplication& app) {
    app.setStyleSheet(lightStyleSheet());
}

QString ThemeManager::darkStyleSheet() {
    return QStringLiteral(R"(
        QWidget {
            background-color: #0F1216;
            color: #E2E8F0;
            font-family: "Segoe UI", "SF Pro Display", -apple-system, sans-serif;
            font-size: 13px;
        }

        QMainWindow {
            background-color: #0F1216;
        }

        /* Sidebar Navigation */
        #SidebarFrame {
            background-color: #171A21;
            border-right: 1px solid #232733;
            min-width: 210px;
            max-width: 210px;
        }

        #AppTitleLabel {
            font-size: 17px;
            font-weight: 700;
            color: #FFFFFF;
            letter-spacing: 0.5px;
            padding-left: 6px;
        }

        #NavButton {
            background-color: transparent;
            color: #94A3B8;
            border: none;
            border-radius: 8px;
            padding: 10px 14px;
            font-size: 13px;
            font-weight: 600;
            text-align: left;
        }
        #NavButton:hover {
            background-color: #202531;
            color: #F1F5F9;
        }
        #NavButton:checked {
            background-color: #2563EB;
            color: #FFFFFF;
        }

        /* Content Area & Cards */
        #ContentFrame {
            background-color: #0F1216;
            padding: 24px;
        }

        .CardFrame {
            background-color: #171A21;
            border: 1px solid #232733;
            border-radius: 12px;
            padding: 18px;
        }

        .SubCardFrame {
            background-color: #1E222C;
            border: 1px solid #2D3342;
            border-radius: 10px;
            padding: 14px;
        }

        /* Typography */
        .PageHeaderTitle {
            font-size: 22px;
            font-weight: 700;
            color: #F8FAFC;
        }

        .PageHeaderSubtitle {
            font-size: 13px;
            color: #64748B;
        }

        .SectionTitle {
            font-size: 15px;
            font-weight: 600;
            color: #E2E8F0;
        }

        /* Drag & Drop Zone */
        #DropZoneWidget {
            background-color: #13171F;
            border: 2px dashed #2D3748;
            border-radius: 12px;
            padding: 28px;
        }
        #DropZoneWidget:hover {
            background-color: #171C26;
            border-color: #3B82F6;
        }

        /* Buttons */
        QPushButton {
            background-color: #222733;
            color: #E2E8F0;
            border: 1px solid #333A4C;
            border-radius: 8px;
            padding: 8px 16px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #2A3140;
            border-color: #3B82F6;
            color: #FFFFFF;
        }
        QPushButton:pressed {
            background-color: #1E232E;
        }
        QPushButton:disabled {
            background-color: #151821;
            border-color: #222733;
            color: #475569;
        }

        QPushButton#PrimaryButton {
            background-color: #2563EB;
            color: #FFFFFF;
            border: 1px solid #3B82F6;
            padding: 10px 20px;
            font-size: 14px;
        }
        QPushButton#PrimaryButton:hover {
            background-color: #1D4ED8;
            border-color: #60A5FA;
        }
        QPushButton#PrimaryButton:pressed {
            background-color: #1E40AF;
        }

        QPushButton#SuccessButton {
            background-color: #059669;
            color: #FFFFFF;
            border: 1px solid #10B981;
            padding: 9px 18px;
        }
        QPushButton#SuccessButton:hover {
            background-color: #047857;
        }

        QPushButton#DangerButton {
            background-color: #DC2626;
            color: #FFFFFF;
            border: 1px solid #EF4444;
            padding: 8px 16px;
        }
        QPushButton#DangerButton:hover {
            background-color: #B91C1C;
        }

        /* Inputs & Combos */
        QLineEdit, QComboBox, QSpinBox {
            background-color: #13161D;
            border: 1px solid #2A303F;
            border-radius: 8px;
            padding: 8px 12px;
            color: #F1F5F9;
            selection-background-color: #2563EB;
        }
        QLineEdit:focus, QComboBox:focus {
            border-color: #3B82F6;
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }

        /* Lists & Tables */
        QTreeWidget, QListWidget, QTableWidget {
            background-color: #13161D;
            border: 1px solid #232835;
            border-radius: 10px;
            padding: 6px;
            outline: none;
            color: #E2E8F0;
        }
        QTreeWidget::item, QListWidget::item, QTableWidget::item {
            padding: 8px 10px;
            border-radius: 6px;
        }
        QTreeWidget::item:hover, QListWidget::item:hover, QTableWidget::item:hover {
            background-color: #1C212B;
        }
        QTreeWidget::item:selected, QListWidget::item:selected, QTableWidget::item:selected {
            background-color: #2563EB;
            color: #FFFFFF;
        }

        QHeaderView::section {
            background-color: #171A21;
            color: #94A3B8;
            border: none;
            border-bottom: 1px solid #232835;
            padding: 8px 10px;
            font-weight: 600;
        }

        /* Progress Bars */
        QProgressBar {
            background-color: #13161D;
            border: 1px solid #262B37;
            border-radius: 6px;
            text-align: center;
            color: #F8FAFC;
            font-weight: 600;
            font-size: 11px;
            height: 18px;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                              stop:0 #2563EB, stop:1 #38BDF8);
            border-radius: 5px;
        }

        /* Scrollbars */
        QScrollBar:vertical {
            background: #0F1216;
            width: 8px;
            margin: 0;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #2D3342;
            min-height: 24px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #475569;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }

        QScrollBar:horizontal {
            background: #0F1216;
            height: 8px;
            margin: 0;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal {
            background: #2D3342;
            min-width: 24px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #475569;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
    )");
}

QString ThemeManager::lightStyleSheet() {
    return QStringLiteral(R"(
        QWidget {
            background-color: #F8FAFC;
            color: #1E293B;
            font-family: "Segoe UI", -apple-system, sans-serif;
            font-size: 13px;
        }
        #SidebarFrame {
            background-color: #FFFFFF;
            border-right: 1px solid #E2E8F0;
        }
        #NavButton {
            color: #64748B;
            border: none;
            border-radius: 8px;
            padding: 10px 14px;
            text-align: left;
        }
        #NavButton:hover {
            background-color: #F1F5F9;
            color: #0F172A;
        }
        #NavButton:checked {
            background-color: #2563EB;
            color: #FFFFFF;
        }
        .CardFrame {
            background-color: #FFFFFF;
            border: 1px solid #E2E8F0;
            border-radius: 12px;
            padding: 18px;
        }
    )");
}

} // namespace FastTransfer
