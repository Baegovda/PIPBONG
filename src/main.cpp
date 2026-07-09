#include "app/Application.h"
#include "app/MainWindow.h"

#include <QEventLoop>
#include <QLocale>

int main(int argc, char* argv[]) {
    Application::ensureDpiAwareness();
    QLocale::setDefault(QLocale(QLocale::Korean, QLocale::SouthKorea));
    Application app(argc, argv);
    MainWindow window;
    window.show();
    // Paint the empty shell before deferred profile/project load runs.
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    return app.exec();
}
