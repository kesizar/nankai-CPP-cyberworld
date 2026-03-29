#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("AI-MUD Cyberworld"));
    QApplication::setOrganizationName(QStringLiteral("CyberpunkVibe"));

    MainWindow w;
    w.resize(1180, 720);
    w.show();
    return app.exec();
}
