#include "app/Application.h"
#include "app/MainWindow.h"

#include <QLocale>

int main(int argc, char* argv[]) {
    Application::ensureDpiAwareness();
    QLocale::setDefault(QLocale(QLocale::Korean, QLocale::SouthKorea));
    Application app(argc, argv);
    MainWindow window;
    window.ensureInitialWindowPlacement();
    window.show();
    return app.exec();
}
