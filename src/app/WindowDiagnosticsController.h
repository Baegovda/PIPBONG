#pragma once
#include <QObject>
#include <QPointer>
class MainWindow;
// GUI snapshot assembly and owned registration. Workers/watchdog read only
// CrashReporter's cached text; they never inspect this window or its registry.
class WindowDiagnosticsController : public QObject {
public:
    explicit WindowDiagnosticsController(MainWindow&);
    ~WindowDiagnosticsController()override;
    QString snapshot()const;
private:
    QPointer<MainWindow> m_window;
    quint64 m_registration=0;
};
