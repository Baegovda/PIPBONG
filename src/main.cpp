#include "tools/NativeCaptionProbe.h"
#include "app/Application.h"
#include "app/MainWindow.h"
#include "app/PointerFeedbackSettings.h"
#include "core/diagnostics/CrashReporter.h"
#include "core/diagnostics/AppStutterProfiler.h"

#include <QLocale>
#include <QMetaType>
#include <QTimer>
#include <QThread>
#include <QGroupBox>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QFont>
#include <QDateTime>

int runGuiContextCacheProbe();
int runEngineStopBoundaryProbe();
int runUiLayoutChecks();
int runProfileRemovalCrashProbe();
int runWindowShellExitProbe();

int main(int argc, char* argv[]) {
    Application::ensureDpiAwareness();
    QLocale::setDefault(QLocale(QLocale::Korean, QLocale::SouthKorea));
    Application app(argc, argv);
    if(qEnvironmentVariableIntValue("PIPBONG_SELF_TEST_CONTEXT_CACHE")==1 && !qEnvironmentVariableIsEmpty("PIPBONG_DATA_DIR"))return runGuiContextCacheProbe();
    if(qEnvironmentVariableIntValue("PIPBONG_SELF_TEST_ENGINE_STOP")==1 && qEnvironmentVariable("QT_QPA_PLATFORM")=="offscreen"
        && !qEnvironmentVariableIsEmpty("PIPBONG_DATA_DIR"))return runEngineStopBoundaryProbe();
    if (qEnvironmentVariableIntValue("PIPBONG_SELF_TEST_UI") == 1
        && !qEnvironmentVariableIsEmpty("PIPBONG_DATA_DIR")
        && qEnvironmentVariableIntValue("PIPBONG_SELF_TEST_SHELL_EXIT") == 1) return runWindowShellExitProbe();
    if (qEnvironmentVariableIntValue("PIPBONG_SELF_TEST_UI") == 1
        && !qEnvironmentVariableIsEmpty("PIPBONG_DATA_DIR")
        && !qEnvironmentVariableIsEmpty("PIPBONG_SELF_TEST_PROFILE_REMOVAL_PHASE")) return runProfileRemovalCrashProbe();
    if (qEnvironmentVariableIntValue("PIPBONG_SELF_TEST_UI") == 1
        && !qEnvironmentVariableIsEmpty("PIPBONG_DATA_DIR")) return runUiLayoutChecks();
    AppStutterProfiler::reloadFromSettings();
    qRegisterMetaType<ClickPointerFeedbackSettings>();
    if (CrashReporter::runViewerModeIfRequested()) {
        return 0;
    }
    CrashReporter::install();
    CrashReporter::installGuiHangWatchdog();
    MainWindow window;
    const bool smokeTest = qEnvironmentVariableIntValue("PIPBONG_SMOKE_TEST") == 1
        && !qEnvironmentVariableIsEmpty("PIPBONG_DATA_DIR");
    if (smokeTest) window.setAttribute(Qt::WA_ShowWithoutActivating);
    window.ensureInitialWindowPlacement();
    window.show();
    window.showPendingCrashReportIfAny();
    if (smokeTest) {
        const bool hangProbe = qEnvironmentVariableIntValue("PIPBONG_SMOKE_HANG_PROBE") == 1
            && QGuiApplication::platformName() == QStringLiteral("offscreen");
        if (hangProbe) {
            // Diagnostic regression only: no desktop window/input manipulation.
            // Starts after the watchdog's ordinary 20-second startup grace.
            QTimer::singleShot(23000, &window, [] { QThread::msleep(3500); });
        }
        const QString capturePath = qEnvironmentVariable("PIPBONG_SMOKE_CAPTURE");
        if (!capturePath.isEmpty()) {
            QTimer::singleShot(1000, &window, [&window, capturePath]() {
                window.grab().save(capturePath);
                // Read our own widget geometry; no activation, native movement or resizing.
                auto* group=window.findChild<QGroupBox*>(QStringLiteral("workflowAddBlocks"));
                QJsonArray buttons;bool contained=group!=nullptr;
                if(group)for(auto* button:group->findChildren<QPushButton*>(QString(),Qt::FindDirectChildrenOnly)) {
                    const QRect rect(button->mapTo(&window,QPoint()),button->size());
                    const QRect local(button->pos(),button->size());
                    const bool valid=window.rect().contains(rect) && group->rect().contains(local)
                        && button->width()>=button->minimumSizeHint().width();
                    contained &= valid;
                    buttons.append(QJsonObject{{"text",button->text()},{"left",rect.left()},{"right",rect.right()},
                        {"top",rect.top()},{"bottom",rect.bottom()},{"width",rect.width()},
                        {"minimumWidth",button->minimumSizeHint().width()},{"font",button->font().family()},
                        {"pointSize",button->font().pointSizeF()},{"contained",valid}});
                }
                QSaveFile report(capturePath+QStringLiteral(".layout.json"));
                if(report.open(QIODevice::WriteOnly)) {
                    report.write(QJsonDocument(QJsonObject{{"processId",QCoreApplication::applicationPid()},
                        {"capturedAtUTC",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                        {"windowWidth",window.width()},{"windowHeight",window.height()},
                        {"devicePixelRatio",window.devicePixelRatioF()},{"found",group!=nullptr},
                        {"buttons",buttons},{"contained",contained},{"nativeCaption",NativeCaptionProbe::inspect(window)}}).toJson());report.commit();
                }
            });
        }
        QTimer::singleShot(hangProbe ? 30000 : 60000, &window, &MainWindow::close);
    }
    return app.exec();
}
