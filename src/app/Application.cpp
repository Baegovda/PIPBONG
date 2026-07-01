#include "app/Application.h"

#include "PipbongVersion.h"

#include <nlohmann/json.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QSettings>
#include <QStandardPaths>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

struct PriorInstallIdentity {
    const char* organization;
    const char* application;
};

constexpr PriorInstallIdentity kPriorInstalls[] = {
    {"SuckbongMachine", "SuckbongMachine"},
    {"poez", "poez"},
};

QString priorAppDataDirectory(const PriorInstallIdentity& identity) {
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (localAppData.isEmpty()) {
        return {};
    }
    return QDir(localAppData).filePath(
        QString::fromLatin1(identity.organization) + QLatin1Char('/')
        + QString::fromLatin1(identity.application));
}

bool readProjectJson(const QString& path, nlohmann::json& out) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    try {
        out = nlohmann::json::parse(file.readAll().constData());
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

int featureCount(const nlohmann::json& project) {
    if (!project.contains("features") || !project["features"].is_array()) {
        return 0;
    }
    return static_cast<int>(project["features"].size());
}

bool isLikelyFreshStubProject(const nlohmann::json& project) {
    if (!project.contains("features") || !project["features"].is_array()) {
        return true;
    }
    const auto& features = project["features"];
    if (features.empty()) {
        return true;
    }
    if (features.size() != 1 || !features[0].is_object()) {
        return false;
    }
    const auto& feature = features[0];
    if (feature.value("name", std::string()) != "예시 기능") {
        return false;
    }
    if (!feature.contains("workflow")) {
        return true;
    }
    const auto& workflow = feature["workflow"];
    if (workflow.is_array()) {
        return workflow.empty();
    }
    if (workflow.is_object() && workflow.contains("blocks") && workflow["blocks"].is_array()) {
        return workflow["blocks"].empty();
    }
    return false;
}

bool copyProjectTree(const QString& sourceDir, const QString& destinationDir) {
    const QString sourceProject = QDir(sourceDir).filePath(QStringLiteral("project.json"));
    if (!QFileInfo::exists(sourceProject)) {
        return false;
    }

    QDir().mkpath(destinationDir);
    const QString destinationProject = QDir(destinationDir).filePath(QStringLiteral("project.json"));
    if (QFileInfo::exists(destinationProject)) {
        QFile::remove(destinationProject);
    }

    if (!QFile::copy(sourceProject, destinationProject)) {
        return false;
    }

    const QDir sourceTemplates(QDir(sourceDir).filePath(QStringLiteral("templates")));
    if (!sourceTemplates.exists()) {
        return true;
    }

    const QDir destinationTemplates(QDir(destinationDir).filePath(QStringLiteral("templates")));
    QDir().mkpath(destinationTemplates.path());
    for (const QString& fileName : sourceTemplates.entryList(QDir::Files)) {
        const QString destination = destinationTemplates.filePath(fileName);
        if (!QFileInfo::exists(destination)) {
            QFile::copy(sourceTemplates.filePath(fileName), destination);
        }
    }
    return true;
}

void migratePriorProjectFiles(const QString& pipbongDataDir) {
    QSettings pipbongSettings;
    if (pipbongSettings.value(QStringLiteral("migration/priorProjectImported")).toBool()) {
        return;
    }

    const QString pipbongProject = QDir(pipbongDataDir).filePath(QStringLiteral("project.json"));
    nlohmann::json pipbongJson;
    const bool hasPipbongProject = readProjectJson(pipbongProject, pipbongJson);
    const bool pipbongNeedsImport = !hasPipbongProject || isLikelyFreshStubProject(pipbongJson);

    for (const PriorInstallIdentity& identity : kPriorInstalls) {
        const QString priorDir = priorAppDataDirectory(identity);
        const QString priorProject = QDir(priorDir).filePath(QStringLiteral("project.json"));
        nlohmann::json priorJson;
        if (!readProjectJson(priorProject, priorJson) || featureCount(priorJson) == 0) {
            continue;
        }

        if (!pipbongNeedsImport) {
            pipbongSettings.setValue(QStringLiteral("migration/priorProjectImported"), true);
            return;
        }

        if (hasPipbongProject) {
            QFile::rename(pipbongProject, pipbongProject + QStringLiteral(".before-import"));
        }

        if (!copyProjectTree(priorDir, pipbongDataDir)) {
            continue;
        }

        pipbongSettings.setValue(QStringLiteral("migration/priorProjectImported"), true);
        pipbongSettings.setValue(QStringLiteral("migration/priorProjectSource"),
                                 QString::fromLatin1(identity.organization) + QLatin1Char('/')
                                     + QString::fromLatin1(identity.application));
        return;
    }

    if (hasPipbongProject && !pipbongNeedsImport) {
        pipbongSettings.setValue(QStringLiteral("migration/priorProjectImported"), true);
    }
}

void migratePriorSettings() {
    QSettings pipbongSettings;
    if (pipbongSettings.value(QStringLiteral("migration/priorSettingsImported")).toBool()) {
        return;
    }

    for (const PriorInstallIdentity& identity : kPriorInstalls) {
        QSettings priorSettings(QString::fromLatin1(identity.organization),
                                QString::fromLatin1(identity.application));
        const QStringList keys = priorSettings.allKeys();
        if (keys.isEmpty()) {
            continue;
        }

        for (const QString& key : keys) {
            if (!pipbongSettings.contains(key)) {
                pipbongSettings.setValue(key, priorSettings.value(key));
            }
        }

        pipbongSettings.setValue(QStringLiteral("migration/priorSettingsImported"), true);
        pipbongSettings.setValue(QStringLiteral("migration/priorSettingsSource"),
                                 QString::fromLatin1(identity.organization) + QLatin1Char('/')
                                     + QString::fromLatin1(identity.application));
        return;
    }
}

} // namespace

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
    migratePriorProjectFiles(dataDir);
    migratePriorSettings();

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
