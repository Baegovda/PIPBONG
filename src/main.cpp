#include "app/Application.h"
#include "app/MainWindow.h"
#include "app/PointerFeedbackSettings.h"
#include "core/diagnostics/CrashReporter.h"
#include "core/diagnostics/AppSpikeProfiler.h"

#include <QLocale>
#include <QMetaType>

int main(int argc, char* argv[]) {
    Application::ensureDpiAwareness();
    QLocale::setDefault(QLocale(QLocale::Korean, QLocale::SouthKorea));
    Application app(argc, argv);
    AppSpikeProfiler::reloadFromSettings();
    qRegisterMetaType<ClickPointerFeedbackSettings>();
    if (CrashReporter::runViewerModeIfRequested()) {
        return 0;
    }
    CrashReporter::install();
    CrashReporter::installGuiHangWatchdog();
    MainWindow window;
    window.ensureInitialWindowPlacement();
    window.show();
    window.showPendingCrashReportIfAny();
    return app.exec();
}
