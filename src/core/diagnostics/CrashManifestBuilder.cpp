#include "core/diagnostics/CrashManifestBuilder.h"

#include "PipbongVersion.h"
#include "core/diagnostics/CrashReporter.h"
#include "core/diagnostics/DiagnosticHub.h"
#include "core/diagnostics/Win32StackWalker.h"

#include <nlohmann/json.hpp>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <opencv2/core/version.hpp>

#include <algorithm>

#include <QStringConverter>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

constexpr int kMaxIncidents = 50;
constexpr int kWorkerStaleThresholdMs = 15000;

nlohmann::json stackFramesToJson(const std::vector<Win32StackWalker::StackFrame>& frames) {
    nlohmann::json array = nlohmann::json::array();
    for (const Win32StackWalker::StackFrame& frame : frames) {
        nlohmann::json entry;
        entry["address"] = frame.address.toStdString();
        if (!frame.symbol.isEmpty()) {
            entry["symbol"] = frame.symbol.toStdString();
        }
        if (!frame.module.isEmpty()) {
            entry["module"] = frame.module.toStdString();
        }
        if (frame.moduleOffset != 0) {
            entry["moduleOffset"] = QStringLiteral("0x%1").arg(frame.moduleOffset, 0, 16).toStdString();
        }
        if (!frame.file.isEmpty()) {
            entry["file"] = frame.file.toStdString();
        }
        if (frame.line > 0) {
            entry["line"] = frame.line;
        }
        array.push_back(std::move(entry));
    }
    return array;
}

nlohmann::json fingerprintJson() {
    nlohmann::json fingerprint;
    fingerprint["version"] = PIPBONG_VERSION;
    fingerprint["gitSha"] = PIPBONG_GIT_SHA;
    fingerprint["buildDate"] = __DATE__;
    fingerprint["buildTime"] = __TIME__;
    if (QCoreApplication::instance()) {
        fingerprint["qtVersion"] = qVersion();
        fingerprint["executable"] = QCoreApplication::applicationFilePath().toStdString();
    }
    fingerprint["opencvVersion"] = CV_VERSION;
#ifdef _WIN32
    fingerprint["processId"] = static_cast<int>(GetCurrentProcessId());
    fingerprint["threadId"] = QStringLiteral("0x%1")
                                  .arg(GetCurrentThreadId(), 0, 16)
                                  .toStdString();
#endif
    return fingerprint;
}

nlohmann::json breadcrumbsJson() {
    nlohmann::json array = nlohmann::json::array();
    for (const DiagnosticHub::BreadcrumbEntry& entry : DiagnosticHub::snapshotBreadcrumbs(400)) {
        nlohmann::json item;
        item["epochMs"] = entry.epochMs;
        item["iso"] =
            QDateTime::fromMSecsSinceEpoch(entry.epochMs).toString(Qt::ISODateWithMs).toStdString();
        item["category"] = entry.category.toStdString();
        item["message"] = entry.message.toStdString();
        array.push_back(std::move(item));
    }
    return array;
}

nlohmann::json workersJson() {
    nlohmann::json array = nlohmann::json::array();
    for (const DiagnosticHub::WorkerHeartbeatSnapshot& worker : DiagnosticHub::snapshotWorkers()) {
        nlohmann::json item;
        item["threadId"] = QStringLiteral("0x%1").arg(worker.threadId, 0, 16).toStdString();
        item["label"] = worker.label.toStdString();
        item["lastBeatAgeMs"] = worker.lastBeatAgeMs;
        array.push_back(std::move(item));
    }
    return array;
}

QString kindToString(CrashReportKind kind) {
    switch (kind) {
    case CrashReportKind::Hang:
        return QStringLiteral("hang");
    case CrashReportKind::QtFatal:
        return QStringLiteral("qt_fatal");
    case CrashReportKind::Crash:
    default:
        return QStringLiteral("crash");
    }
}

bool recentBreadcrumbContains(const std::vector<DiagnosticHub::BreadcrumbEntry>& breadcrumbs,
                              const QString& category,
                              const QString& needle,
                              int lastN = 24) {
    const int count = static_cast<int>(breadcrumbs.size());
    const int start = std::max(0, count - lastN);
    for (int index = start; index < count; ++index) {
        const DiagnosticHub::BreadcrumbEntry& entry = breadcrumbs.at(static_cast<size_t>(index));
        if (entry.category == category && entry.message.contains(needle)) {
            return true;
        }
    }
    return false;
}

bool contextContainsEngineRunning(const QString& contextSnapshot) {
    return contextSnapshot.contains(QStringLiteral("engine=running"))
           || contextSnapshot.contains(QStringLiteral("engine=worker"));
}

} // namespace

namespace CrashManifestBuilder {

QStringList buildSuspectedCauses(CrashReportKind kind,
                                 const QString& reason,
                                 const QString& contextSnapshot,
#ifdef _WIN32
                                 EXCEPTION_POINTERS* exceptionInfo,
#endif
                                 bool hangReport) {
    Q_UNUSED(reason);
    QStringList causes;
    const std::vector<DiagnosticHub::BreadcrumbEntry> breadcrumbs = DiagnosticHub::snapshotBreadcrumbs(120);
    const bool recentStop =
        recentBreadcrumbContains(breadcrumbs, QStringLiteral("run"), QStringLiteral("stop feature"));
    const bool recentEngineAbandon =
        recentBreadcrumbContains(breadcrumbs, QStringLiteral("engine"), QStringLiteral("abandon"));
    const bool recentBlockStart =
        recentBreadcrumbContains(breadcrumbs, QStringLiteral("workflow"), QStringLiteral("block start"));
    const bool recentImageFind =
        recentBreadcrumbContains(breadcrumbs, QStringLiteral("workflow"), QStringLiteral("ImageFind"));

#ifdef _WIN32
    if (exceptionInfo && exceptionInfo->ExceptionRecord) {
        const auto* record = exceptionInfo->ExceptionRecord;
        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
            const auto accessKind = record->ExceptionInformation[0];
            const auto address = record->ExceptionInformation[1];
            if (accessKind == 1 && address == 0) {
                causes.append(QStringLiteral(
                    "null_pointer_write: EXCEPTION_ACCESS_VIOLATION write at address 0x0"));
            } else if (accessKind == 0) {
                causes.append(QStringLiteral(
                    "access_violation_read: EXCEPTION_ACCESS_VIOLATION read at 0x%1")
                                  .arg(address, 0, 16));
            } else if (accessKind == 1) {
                causes.append(QStringLiteral(
                    "access_violation_write: EXCEPTION_ACCESS_VIOLATION write at 0x%1")
                                  .arg(address, 0, 16));
            }
        } else if (record->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
            causes.append(QStringLiteral("stack_overflow: EXCEPTION_STACK_OVERFLOW"));
        }
    }
#endif

    if (hangReport || kind == CrashReportKind::Hang) {
        causes.append(QStringLiteral(
            "gui_thread_hang: main-thread heartbeat silent beyond hang threshold"));
    }

    if (recentStop && contextContainsEngineRunning(contextSnapshot)) {
        causes.append(QStringLiteral(
            "worker_stop_race: user stop recorded while a workflow engine was still running"));
    }

    if (recentEngineAbandon && recentStop) {
        causes.append(QStringLiteral(
            "abandoned_engine_teardown: engine abandoned during user stop — verify deferred session finish"));
    }

    if (contextSnapshot.contains(QStringLiteral("abandonedEngines:"))
        && !contextSnapshot.contains(QStringLiteral("abandonedEngines: 0"))) {
        causes.append(QStringLiteral(
            "abandoned_engine_active: abandoned workflow engines still present in session map"));
    }

    if (recentImageFind && recentStop) {
        causes.append(QStringLiteral(
            "imagefind_during_stop: ImageFind block activity near user stop request"));
    }

    if (recentBlockStart && recentStop) {
        causes.append(QStringLiteral(
            "block_execution_during_stop: workflow block started shortly before stop"));
    }

    const QStringList staleWorkers = DiagnosticHub::staleWorkerLabels(kWorkerStaleThresholdMs);
    for (const QString& stale : staleWorkers) {
        causes.append(QStringLiteral("stale_worker_heartbeat: %1").arg(stale));
    }

    if (contextSnapshot.contains(QStringLiteral("mouseCenterLock: active"))) {
        causes.append(QStringLiteral(
            "mouse_lock_active: MouseCenterLock engaged during incident"));
    }

    if (contextSnapshot.contains(QStringLiteral("calculator=open"))) {
        causes.append(QStringLiteral(
            "modeless_calculator_open: calculator dialog open during incident"));
    }

    return causes;
}

QString buildManifestJson(CrashReportKind kind,
                          const QString& reason,
                          const QString& contextSnapshot,
                          const QString& lastUserAction,
#ifdef _WIN32
                          EXCEPTION_POINTERS* exceptionInfo,
                          CONTEXT* fallbackContext,
#endif
                          bool hangReport) {
    nlohmann::json root;
    root["manifestVersion"] = 1;
    root["kind"] = kindToString(kind).toStdString();
    root["reason"] = reason.toStdString();
    root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs).toStdString();
    root["fingerprint"] = fingerprintJson();

    if (!lastUserAction.isEmpty()) {
        root["lastUserAction"] = lastUserAction.toStdString();
    }

    if (!contextSnapshot.trimmed().isEmpty()) {
        nlohmann::json contextLines = nlohmann::json::array();
        for (const QString& line : contextSnapshot.split(QLatin1Char('\n'))) {
            const QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                contextLines.push_back(trimmed.toStdString());
            }
        }
        root["context"] = std::move(contextLines);
    }

#ifdef _WIN32
    if (exceptionInfo && exceptionInfo->ExceptionRecord) {
        const auto* record = exceptionInfo->ExceptionRecord;
        nlohmann::json exception;
        exception["code"] = QStringLiteral("0x%1")
                                .arg(record->ExceptionCode, 8, 16, QLatin1Char('0'))
                                .toStdString();
        exception["address"] = QStringLiteral("0x%1")
                                   .arg(reinterpret_cast<quintptr>(record->ExceptionAddress),
                                        16,
                                        16,
                                        QLatin1Char('0'))
                                   .toStdString();
        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
            const char* accessKind = record->ExceptionInformation[0] == 0   ? "read"
                                     : record->ExceptionInformation[0] == 1 ? "write"
                                                                            : "execute";
            exception["accessKind"] = accessKind;
            exception["accessAddress"] = QStringLiteral("0x%1")
                                           .arg(record->ExceptionInformation[1], 16, 16, QLatin1Char('0'))
                                           .toStdString();
        }
        root["exception"] = std::move(exception);
    }
#endif

    nlohmann::json stacks;
#ifdef _WIN32
    CONTEXT* stackContext = nullptr;
    if (exceptionInfo && exceptionInfo->ContextRecord) {
        stackContext = exceptionInfo->ContextRecord;
    } else if (fallbackContext) {
        stackContext = fallbackContext;
    }
    if (stackContext) {
        stacks["faulting"] = stackFramesToJson(Win32StackWalker::collectStackFrames(stackContext, 48));
    }
    if (hangReport) {
        CONTEXT watchdogContext{};
        RtlCaptureContext(&watchdogContext);
        stacks["watchdog"] =
            stackFramesToJson(Win32StackWalker::collectStackFrames(&watchdogContext, 24));
    }
#endif
    if (!stacks.empty()) {
        root["stacks"] = std::move(stacks);
    }

    root["breadcrumbs"] = breadcrumbsJson();
    root["workers"] = workersJson();

    const QStringList staleWorkers = DiagnosticHub::staleWorkerLabels(kWorkerStaleThresholdMs);
    if (!staleWorkers.isEmpty()) {
        nlohmann::json stale = nlohmann::json::array();
        for (const QString& label : staleWorkers) {
            stale.push_back(label.toStdString());
        }
        root["staleWorkers"] = std::move(stale);
    }

    const QStringList suspectedCauses =
        buildSuspectedCauses(kind, reason, contextSnapshot,
#ifdef _WIN32
                             exceptionInfo,
#endif
                             hangReport);
    if (!suspectedCauses.isEmpty()) {
        nlohmann::json causes = nlohmann::json::array();
        for (const QString& cause : suspectedCauses) {
            causes.push_back(cause.toStdString());
        }
        root["suspectedCauses"] = std::move(causes);
    }

    if (hangReport) {
        nlohmann::json hang;
        hang["reported"] = true;
        root["hang"] = std::move(hang);
    }

    return QString::fromStdString(root.dump(2));
}

bool writeManifestFile(const QString& folderPath, const QString& manifestJson) {
    if (folderPath.isEmpty() || manifestJson.isEmpty()) {
        return false;
    }
    QFile file(QDir(folderPath).filePath(QStringLiteral("manifest.json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << manifestJson;
    return true;
}

bool updateIncidentsIndex(const QString& crashRootPath,
                          const QString& incidentFolderName,
                          CrashReportKind kind,
                          const QString& reason,
                          const QStringList& suspectedCauses) {
    if (crashRootPath.isEmpty() || incidentFolderName.isEmpty()) {
        return false;
    }

    const QString indexPath = QDir(crashRootPath).filePath(QStringLiteral("incidents.json"));
    nlohmann::json root;
    QFile existing(indexPath);
    if (existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
        try {
            root = nlohmann::json::parse(existing.readAll().constData());
        } catch (...) {
            root = nlohmann::json::object();
        }
    }

    if (!root.is_object()) {
        root = nlohmann::json::object();
    }
    if (!root.contains("version")) {
        root["version"] = 1;
    }
    if (!root.contains("incidents") || !root["incidents"].is_array()) {
        root["incidents"] = nlohmann::json::array();
    }

    nlohmann::json incident;
    incident["folder"] = incidentFolderName.toStdString();
    incident["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs).toStdString();
    incident["kind"] = kindToString(kind).toStdString();
    incident["reason"] = reason.toStdString();
    incident["version"] = PIPBONG_VERSION;
    incident["gitSha"] = PIPBONG_GIT_SHA;
    if (!suspectedCauses.isEmpty()) {
        nlohmann::json causes = nlohmann::json::array();
        for (const QString& cause : suspectedCauses) {
            causes.push_back(cause.toStdString());
        }
        incident["suspectedCauses"] = std::move(causes);
    }

    nlohmann::json incidents = nlohmann::json::array();
    incidents.push_back(std::move(incident));
    for (const auto& prior : root["incidents"]) {
        if (!prior.is_object()) {
            continue;
        }
        if (prior.contains("folder")
            && prior["folder"].get<std::string>() == incidentFolderName.toStdString()) {
            continue;
        }
        incidents.push_back(prior);
        if (static_cast<int>(incidents.size()) >= kMaxIncidents) {
            break;
        }
    }
    root["incidents"] = std::move(incidents);

    QFile out(indexPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    const std::string serialized = root.dump(2);
    out.write(serialized.data(), static_cast<qint64>(serialized.size()));
    return true;
}

} // namespace CrashManifestBuilder
