#include "app/Application.h"

#include "PipbongVersion.h"

#include <QDir>
#include <QIcon>
#include <QSettings>
#include <QStandardPaths>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv) {
    setApplicationName(QStringLiteral("PIPBONG"));
    setOrganizationName(QStringLiteral("PIPBONG"));
    setApplicationVersion(QStringLiteral(PIPBONG_VERSION));

    const QIcon appIcon(QStringLiteral(":/app/Pipbong.ico"));
    if (!appIcon.isNull()) {
        setWindowIcon(appIcon);
    }

    const QString dataDir = dataDirectory();
    QSettings settings;
    m_projectDirectory = settings.value(QStringLiteral("project/directory"), dataDir).toString();
    QDir().mkpath(m_projectDirectory);
}

QString Application::dataDirectory() {
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    return path;
}

QString Application::autoSaveFilePath() {
    return QDir(dataDirectory()).filePath(QStringLiteral("project.json"));
}

Application* Application::instance() {
    return qobject_cast<Application*>(QApplication::instance());
}

QString Application::projectDirectory() const {
    return m_projectDirectory;
}

void Application::setProjectDirectory(const QString& path) {
    m_projectDirectory = path;
    QSettings settings;
    settings.setValue(QStringLiteral("project/directory"), path);
}

void Application::ensureDpiAwareness() {
#ifdef _WIN32
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        using SetDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto fn = reinterpret_cast<SetDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (fn) {
            fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }
#endif
}
