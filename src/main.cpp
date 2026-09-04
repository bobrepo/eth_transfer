#include <QApplication>
#include "core/TransferManager.h"
#include "gui/MainWindow.h"
#include "gui/ThemeManager.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("FastTransfer"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("FastTransfer"));

    // Apply modern dark theme by default
    FastTransfer::ThemeManager::applyDarkTheme(app);

    // Initialize core transfer manager
    FastTransfer::TransferManager manager;
    if (!manager.initialize()) {
        qWarning("Failed to initialize FastTransfer Manager");
        return 1;
    }

    // Launch desktop main window
    FastTransfer::MainWindow mainWindow(&manager);
    mainWindow.show();

    return app.exec();
}
