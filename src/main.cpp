#include "app/Application.h"
#include "app/MainWindow.h"
#include "core/diagnostics/CrashReporter.h"

#include <QLocale>

int main(int argc, char* argv[]) {
    Application::ensureDpiAwareness();
    QLocale::setDefault(QLocale(QLocale::Korean, QLocale::SouthKorea));
    Application app(argc, argv);
    CrashReporter::install();
    MainWindow window;
    window.ensureInitialWindowPlacement();
    window.show();
    window.showPendingCrashReportIfAny();
    return app.exec();
}
