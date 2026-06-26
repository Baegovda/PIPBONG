#include "app/Application.h"

#include <QFile>
#include <QIcon>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

void migrateLegacyAppData(const QString& newDataDir) {
    const QString newProjectFile = QDir(newDataDir).filePath(QStringLiteral("project.json"));
    if (QFileInfo::exists(newProjectFile)) {
        return;
    }

    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (localAppData.isEmpty()) {
        return;
    }

    const QString legacyDir = QDir(localAppData).filePath(QStringLiteral("poez/poez"));
    const QString legacyProjectFile = QDir(legacyDir).filePath(QStringLiteral("project.json"));
    if (!QFileInfo::exists(legacyProjectFile)) {
        return;
    }

    QDir().mkpath(newDataDir);
    QFile::copy(legacyProjectFile, newProjectFile);

    const QDir legacyTemplates(QDir(legacyDir).filePath(QStringLiteral("templates")));
    if (!legacyTemplates.exists()) {
        return;
    }

    const QDir newTemplates(QDir(newDataDir).filePath(QStringLiteral("templates")));
    QDir().mkpath(newTemplates.path());
    for (const QString& fileName : legacyTemplates.entryList(QDir::Files)) {
        const QString destination = newTemplates.filePath(fileName);
        if (!QFileInfo::exists(destination)) {
            QFile::copy(legacyTemplates.filePath(fileName), destination);
        }
    }
}

} // namespace

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv) {
    setApplicationName(QStringLiteral("SuckbongMachine"));
    setOrganizationName(QStringLiteral("SuckbongMachine"));
#ifndef SBM_VERSION
#define SBM_VERSION "0.1.0"
#endif
    setApplicationVersion(QStringLiteral(SBM_VERSION));

    const QIcon appIcon(QStringLiteral(":/app/Sbm.ico"));
    if (!appIcon.isNull()) {
        setWindowIcon(appIcon);
    }

    migrateLegacyAppData(dataDirectory());

    QSettings settings;
    m_projectDirectory = settings.value(QStringLiteral("project/directory"), dataDirectory()).toString();
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
