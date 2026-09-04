#ifndef FASTTRANSFER_THEME_MANAGER_H
#define FASTTRANSFER_THEME_MANAGER_H

#include <QString>
#include <QApplication>

namespace FastTransfer {

class ThemeManager {
public:
    static void applyDarkTheme(QApplication& app);
    static void applyLightTheme(QApplication& app);
    static QString darkStyleSheet();
    static QString lightStyleSheet();
};

} // namespace FastTransfer

#endif // FASTTRANSFER_THEME_MANAGER_H
