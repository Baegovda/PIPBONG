#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <crtdbg.h>
#include <windows.h>
#endif

#include "core/diagnostics/CrashReporter.h"

#include "PipbongVersion.h"
#include "core/diagnostics/CrashReportSystemInfo.h"
#include "core/diagnostics/Win32StackWalker.h"
#include "ui/diagnostics/CrashReportDialog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include <QStringConverter>

namespace {

constexpr int kMaxRecentLogLines = 800;
constexpr int kMaxCriticalLogLines = 20;
constexpr int kMaxRetainedCrashFolders = 10;
constexpr int kGuiHeartbeatIntervalMs = 400;
constexpr int kGuiHangWatchdogPollMs = 1000;
constexpr int kGuiHangThresholdMs = 6000;
constexpr wchar_t kPendingFileName[] = L"pending.txt";
constexpr wchar_t kCrashReportViewerArg[] = L"--crash-report";

struct CrashArtifacts {
    QString folderPath;
    QString reportText;
    CrashReportKind kind = CrashReportKind::Crash;
};

std::mutex g_logMutex;
std::deque<QString> g_recentLogLines;
QtMessageHandler g_previousQtHandler = nullptr;
std::atomic<bool> g_writingCrashReport{false};
std::atomic<qint64> g_lastGuiHeartbeatMs{0};
std::atomic<bool> g_guiHangReported{false};
std::atomic<bool> g_guiHangWatchdogStop{false};
std::atomic<unsigned long> g_mainThreadId{0};

std::mutex g_contextMutex;
CrashContextProvider g_contextProvider;
QString g_cachedContextSnapshot;

#ifdef _WIN32
struct MinidumpExceptionInformation {
    DWORD threadId = 0;
    EXCEPTION_POINTERS* exceptionPointers = nullptr;
    BOOL clientPointers = FALSE;
};

using MiniDumpWriteDumpFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, DWORD, MinidumpExceptionInformation*, void*, void*);
#endif

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

CrashReportKind kindFromString(const QString& value) {
    if (value == QStringLiteral("hang")) {
        return CrashReportKind::Hang;
    }
    if (value == QStringLiteral("qt_fatal")) {
        return CrashReportKind::QtFatal;
    }
    return CrashReportKind::Crash;
}

QString qtMessageTypeLabel(QtMsgType type) {
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("CRIT");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }
    return QStringLiteral("LOG");
}

void appendRecentLogLine(const QString& line) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_recentLogLines.push_back(line);
    while (static_cast<int>(g_recentLogLines.size()) > kMaxRecentLogLines) {
        g_recentLogLines.pop_front();
    }
}

void refreshContextCacheFromProvider() {
    CrashContextProvider provider;
    {
        std::lock_guard<std::mutex> lock(g_contextMutex);
        provider = g_contextProvider;
    }
    if (!provider) {
        return;
    }
    const QString snapshot = provider();
    std::lock_guard<std::mutex> lock(g_contextMutex);
    g_cachedContextSnapshot = snapshot;
}

QString cachedContextSnapshot() {
    std::lock_guard<std::mutex> lock(g_contextMutex);
    return g_cachedContextSnapshot;
}

#ifdef _WIN32
CrashArtifacts writeCrashArtifacts(CrashReportKind kind,
                                   const QString& reason,
                                   EXCEPTION_POINTERS* exceptionInfo,
                                   CONTEXT* fallbackContext,
                                   bool hangReport);
void presentCrashReport(const CrashArtifacts& artifacts, bool allowInProcessUi);
#endif

void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
    QString fileName;
    if (context.file) {
        fileName = QString::fromUtf8(context.file);
        const int slash = qMax(fileName.lastIndexOf(QLatin1Char('/')), fileName.lastIndexOf(QLatin1Char('\\')));
        if (slash >= 0) {
            fileName = fileName.mid(slash + 1);
        }
    }
    const QString line =
        QStringLiteral("[%1] %2 %3:%4 %5")
            .arg(timestamp, qtMessageTypeLabel(type), fileName)
            .arg(context.line)
            .arg(message);
    appendRecentLogLine(line);

    if (type == QtFatalMsg) {
#ifdef _WIN32
        if (!g_writingCrashReport.exchange(true)) {
            CONTEXT contextRecord{};
            RtlCaptureContext(&contextRecord);
            const QString reason =
                QStringLiteral("Qt fatal message: %1").arg(message);
            const CrashArtifacts artifacts =
                writeCrashArtifacts(CrashReportKind::QtFatal, reason, nullptr, &contextRecord, false);
            presentCrashReport(artifacts, true);
        }
#endif
    }

    if (g_previousQtHandler) {
        g_previousQtHandler(type, context, message);
    }
}

QString crashRootDirectoryPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString path = QDir(base).filePath(QStringLiteral("crash"));
    QDir().mkpath(path);
    return path;
}

QString pendingMarkerPath() {
    return QDir(crashRootDirectoryPath()).filePath(QString::fromWCharArray(kPendingFileName));
}

QString makeCrashFolderName() {
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
}

#ifdef _WIN32
QString currentExecutablePath() {
    if (QCoreApplication::instance()) {
        return QCoreApplication::applicationFilePath();
    }
    wchar_t pathBuffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, pathBuffer, MAX_PATH);
    if (length == 0) {
        return QString();
    }
    return QString::fromWCharArray(pathBuffer, static_cast<int>(length));
}

void notifyCrashReportSaved(const QString& folderPath, bool immediateHang) {
    const QString message =
        folderPath.isEmpty()
            ? QStringLiteral("PIPBONG: An error report was saved under %LOCALAPPDATA%\\PIPBONG\\PIPBONG\\crash\\")
            : QStringLiteral("PIPBONG: An error report was saved.\n\n%1").arg(folderPath);
    const QString title = immediateHang ? QStringLiteral("PIPBONG - Not Responding")
                                        : QStringLiteral("PIPBONG - Error Report");
    MessageBoxW(nullptr,
                reinterpret_cast<LPCWSTR>(message.utf16()),
                reinterpret_cast<LPCWSTR>(title.utf16()),
                MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND);
}

void launchDetachedCrashReportViewer(const QString& folderPath) {
    const QString executablePath = currentExecutablePath();
    if (executablePath.isEmpty() || folderPath.isEmpty()) {
        notifyCrashReportSaved(folderPath, false);
        return;
    }

    QString commandLine = QStringLiteral("\"%1\" %2 \"%3\"")
                              .arg(executablePath, QString::fromWCharArray(kCrashReportViewerArg), folderPath);
    std::wstring nativeCommand = commandLine.toStdWString();

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    const BOOL created = CreateProcessW(nullptr,
                                        nativeCommand.data(),
                                        nullptr,
                                        nullptr,
                                        FALSE,
                                        DETACHED_PROCESS,
                                        nullptr,
                                        nullptr,
                                        &startupInfo,
                                        &processInfo);
    if (!created) {
        notifyCrashReportSaved(folderPath, true);
        return;
    }
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
}

void showCrashReportDialogOnGuiThread(const QString& reportText,
                                       const QString& folderPath,
                                       CrashReportKind kind) {
    CrashReportDialog dialog(reportText, folderPath, nullptr, true, kind);
    dialog.exec();
    CrashReporter::dismissPendingReport();
}

void presentCrashReport(const CrashArtifacts& artifacts, bool allowInProcessUi) {
    if (artifacts.reportText.trimmed().isEmpty()) {
        return;
    }

    QCoreApplication* app = QCoreApplication::instance();
    if (allowInProcessUi && app) {
        const auto showOnGuiThread = [reportText = artifacts.reportText,
                                    folderPath = artifacts.folderPath,
                                    kind = artifacts.kind]() {
            showCrashReportDialogOnGuiThread(reportText, folderPath, kind);
        };
        if (QThread::currentThread() == app->thread()) {
            showOnGuiThread();
        } else {
            const bool invoked =
                QMetaObject::invokeMethod(app, showOnGuiThread, Qt::BlockingQueuedConnection);
            if (!invoked) {
                launchDetachedCrashReportViewer(artifacts.folderPath);
            }
        }
        return;
    }

    launchDetachedCrashReportViewer(artifacts.folderPath);
}

void touchGuiHeartbeat() {
    g_lastGuiHeartbeatMs.store(QDateTime::currentMSecsSinceEpoch());
    refreshContextCacheFromProvider();
}

void reportGuiThreadHang(qint64 silentMs) {
    if (g_guiHangReported.exchange(true) || g_writingCrashReport.exchange(true)) {
        return;
    }

    const QString reason =
        QStringLiteral("GUI thread not responding (hang) - no heartbeat for %1 ms").arg(silentMs);
    const CrashArtifacts artifacts =
        writeCrashArtifacts(CrashReportKind::Hang, reason, nullptr, nullptr, true);
    presentCrashReport(artifacts, false);
}

void guiHangWatchdogThreadMain() {
    while (!g_guiHangWatchdogStop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kGuiHangWatchdogPollMs));
        if (g_guiHangWatchdogStop.load() || g_guiHangReported.load()) {
            continue;
        }

        const qint64 lastBeat = g_lastGuiHeartbeatMs.load();
        if (lastBeat <= 0) {
            continue;
        }

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 silentMs = now - lastBeat;
        if (silentMs >= kGuiHangThresholdMs) {
            reportGuiThreadHang(silentMs);
        }
    }
}

QString exceptionCodeName(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        return QStringLiteral("EXCEPTION_ACCESS_VIOLATION");
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return QStringLiteral("EXCEPTION_ARRAY_BOUNDS_EXCEEDED");
    case EXCEPTION_BREAKPOINT:
        return QStringLiteral("EXCEPTION_BREAKPOINT");
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return QStringLiteral("EXCEPTION_DATATYPE_MISALIGNMENT");
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        return QStringLiteral("EXCEPTION_FLT_DENORMAL_OPERAND");
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return QStringLiteral("EXCEPTION_ILLEGAL_INSTRUCTION");
    case EXCEPTION_IN_PAGE_ERROR:
        return QStringLiteral("EXCEPTION_IN_PAGE_ERROR");
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return QStringLiteral("EXCEPTION_INT_DIVIDE_BY_ZERO");
    case EXCEPTION_STACK_OVERFLOW:
        return QStringLiteral("EXCEPTION_STACK_OVERFLOW");
    default:
        return QStringLiteral("0x%1").arg(code, 8, 16, QLatin1Char('0'));
    }
}

bool writeMiniDump(const QString& dumpPath, EXCEPTION_POINTERS* exceptionInfo) {
    HMODULE dbgHelp = LoadLibraryW(L"Dbghelp.dll");
    if (!dbgHelp) {
        return false;
    }
    auto writeDump = reinterpret_cast<MiniDumpWriteDumpFn>(GetProcAddress(dbgHelp, "MiniDumpWriteDump"));
    if (!writeDump) {
        FreeLibrary(dbgHelp);
        return false;
    }

    HANDLE file =
        CreateFileW(reinterpret_cast<LPCWSTR>(dumpPath.utf16()), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        FreeLibrary(dbgHelp);
        return false;
    }

    MinidumpExceptionInformation info{};
    MinidumpExceptionInformation* infoPtr = nullptr;
    if (exceptionInfo) {
        info.threadId = GetCurrentThreadId();
        info.exceptionPointers = exceptionInfo;
        info.clientPointers = FALSE;
        infoPtr = &info;
    }

    constexpr DWORD kMiniDumpWithThreadInfo = 0x00001000;
    constexpr DWORD kMiniDumpScanMemory = 0x00000010;
    const BOOL ok = writeDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                              kMiniDumpWithThreadInfo | kMiniDumpScanMemory, infoPtr, nullptr, nullptr);
    CloseHandle(file);
    FreeLibrary(dbgHelp);
    return ok == TRUE;
}

QString buildCrashReportText(CrashReportKind kind,
                             const QString& reason,
                             EXCEPTION_POINTERS* exceptionInfo,
                             CONTEXT* fallbackContext,
                             bool hangReport) {
    QStringList lines;
    lines.append(QStringLiteral("PIPBONG crash report"));
    lines.append(QStringLiteral("version: %1").arg(QStringLiteral(PIPBONG_VERSION)));
    lines.append(QStringLiteral("kind: %1").arg(kindToString(kind)));
    lines.append(QStringLiteral("reason: %1").arg(reason));
    lines.append(QStringLiteral("timestamp: %1")
                     .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
    if (QCoreApplication::instance()) {
        lines.append(QStringLiteral("app: %1").arg(QCoreApplication::applicationFilePath()));
    }

    if (exceptionInfo && exceptionInfo->ExceptionRecord) {
        const auto* record = exceptionInfo->ExceptionRecord;
        lines.append(QStringLiteral("exception: %1 at 0x%2")
                         .arg(exceptionCodeName(record->ExceptionCode))
                         .arg(reinterpret_cast<quintptr>(record->ExceptionAddress), 16, 16, QLatin1Char('0')));
        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
            const char* accessKind = record->ExceptionInformation[0] == 0   ? "read"
                                     : record->ExceptionInformation[0] == 1 ? "write"
                                                                            : "execute";
            lines.append(QStringLiteral("access: %1 at 0x%2")
                             .arg(QString::fromLatin1(accessKind))
                             .arg(record->ExceptionInformation[1], 16, 16, QLatin1Char('0')));
        }
    }

    lines.append(QString());
    lines.append(CrashReportSystemInfo::buildSystemSection());

    const QString contextSnapshot = cachedContextSnapshot();
    lines.append(QString());
    lines.append(QStringLiteral("--- application context ---"));
    if (contextSnapshot.trimmed().isEmpty()) {
        lines.append(QStringLiteral("  (not available)"));
    } else {
        lines.append(contextSnapshot);
    }

    lines.append(QString());
    lines.append(QStringLiteral("--- recent critical messages ---"));
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        int criticalCount = 0;
        for (auto it = g_recentLogLines.rbegin();
             it != g_recentLogLines.rend() && criticalCount < kMaxCriticalLogLines;
             ++it) {
            if (it->contains(QStringLiteral("] CRIT "))) {
                lines.append(*it);
                ++criticalCount;
            }
        }
        if (criticalCount == 0) {
            lines.append(QStringLiteral("  (none)"));
        }
    }

    if (hangReport) {
        lines.append(QString());
        lines.append(QStringLiteral("--- GUI thread stack ---"));
        const unsigned long mainThreadId = g_mainThreadId.load();
        if (!Win32StackWalker::appendGuiThreadStackTrace(lines, mainThreadId)) {
            lines.append(QStringLiteral("  (failed to capture GUI thread stack)"));
        }

        lines.append(QString());
        lines.append(QStringLiteral("--- watchdog thread stack ---"));
        Win32StackWalker::appendCurrentThreadStackTrace(lines);
    } else {
        lines.append(QString());
        lines.append(QStringLiteral("--- stack trace ---"));
        if (exceptionInfo && exceptionInfo->ContextRecord) {
            Win32StackWalker::appendStackTrace(lines, exceptionInfo->ContextRecord);
        } else if (fallbackContext) {
            Win32StackWalker::appendStackTrace(lines, fallbackContext);
        }
    }

    lines.append(QString());
    lines.append(QStringLiteral("--- recent application log ---"));
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        for (const QString& entry : g_recentLogLines) {
            lines.append(entry);
        }
    }

    return lines.join(QLatin1Char('\n'));
}

bool writeTextFile(const QString& path, const QString& text) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << text;
    return true;
}

void pruneOldCrashFolders(const QString& rootPath) {
    QDir root(rootPath);
    const QFileInfoList entries =
        root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time | QDir::Reversed);
    for (int index = 0; index < entries.size() - kMaxRetainedCrashFolders; ++index) {
        QDir(entries.at(index).absoluteFilePath()).removeRecursively();
    }
}

CrashArtifacts writeCrashArtifacts(CrashReportKind kind,
                                   const QString& reason,
                                   EXCEPTION_POINTERS* exceptionInfo,
                                   CONTEXT* fallbackContext,
                                   bool hangReport) {
    CrashArtifacts artifacts;
    artifacts.kind = kind;
    const QString root = crashRootDirectoryPath();
    artifacts.folderPath = QDir(root).filePath(makeCrashFolderName());
    QDir().mkpath(artifacts.folderPath);

    const QString reportPath = QDir(artifacts.folderPath).filePath(QStringLiteral("report.txt"));
    const QString dumpPath = QDir(artifacts.folderPath).filePath(QStringLiteral("crash.dmp"));
    const QString logPath = QDir(artifacts.folderPath).filePath(QStringLiteral("recent_log.txt"));
    const QString kindPath = QDir(artifacts.folderPath).filePath(QStringLiteral("kind.txt"));

    artifacts.reportText =
        buildCrashReportText(kind, reason, exceptionInfo, fallbackContext, hangReport);
    writeTextFile(reportPath, artifacts.reportText);
    writeTextFile(kindPath, kindToString(kind) + QLatin1Char('\n'));

    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        QStringList logLines;
        logLines.reserve(static_cast<int>(g_recentLogLines.size()));
        for (const QString& entry : g_recentLogLines) {
            logLines.append(entry);
        }
        writeTextFile(logPath, logLines.join(QLatin1Char('\n')));
    }

    if (!writeMiniDump(dumpPath, exceptionInfo)) {
        QFile(dumpPath).remove();
    }

    writeTextFile(pendingMarkerPath(), artifacts.folderPath + QLatin1Char('\n'));
    pruneOldCrashFolders(root);
    return artifacts;
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
    if (g_writingCrashReport.exchange(true)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const CrashArtifacts artifacts =
        writeCrashArtifacts(CrashReportKind::Crash,
                            QStringLiteral("unhandled SEH exception"),
                            exceptionInfo,
                            nullptr,
                            false);
    presentCrashReport(artifacts, false);
    return EXCEPTION_CONTINUE_SEARCH;
}

void terminateHandler() {
    if (g_writingCrashReport.exchange(true)) {
        std::abort();
    }

    CONTEXT context{};
    RtlCaptureContext(&context);
    const CrashArtifacts artifacts =
        writeCrashArtifacts(CrashReportKind::Crash, QStringLiteral("std::terminate"), nullptr, &context, false);
    presentCrashReport(artifacts, QCoreApplication::instance() != nullptr);
    std::abort();
}

void purecallHandler() {
    if (!g_writingCrashReport.exchange(true)) {
        const CrashArtifacts artifacts =
            writeCrashArtifacts(CrashReportKind::Crash, QStringLiteral("_purecall"), nullptr, nullptr, false);
        presentCrashReport(artifacts, false);
    }
    std::abort();
}

void invalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
    if (!g_writingCrashReport.exchange(true)) {
        const CrashArtifacts artifacts = writeCrashArtifacts(CrashReportKind::Crash,
                                                             QStringLiteral("invalid parameter handler"),
                                                             nullptr,
                                                             nullptr,
                                                             false);
        presentCrashReport(artifacts, false);
    }
    std::abort();
}
#endif // _WIN32

} // namespace

bool CrashReporter::runViewerModeIfRequested() {
#ifdef _WIN32
    const QStringList args = QCoreApplication::arguments();
    const int flagIndex = args.indexOf(QString::fromWCharArray(kCrashReportViewerArg));
    if (flagIndex < 0 || flagIndex + 1 >= args.size()) {
        return false;
    }

    CrashReportSummary summary;
    summary.folderPath = args.at(flagIndex + 1);
    summary.reportFilePath = QDir(summary.folderPath).filePath(QStringLiteral("report.txt"));
    summary.dumpFilePath = QDir(summary.folderPath).filePath(QStringLiteral("crash.dmp"));

    bool kindRead = false;
    QFile kindFile(QDir(summary.folderPath).filePath(QStringLiteral("kind.txt")));
    if (kindFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        summary.kind = kindFromString(QString::fromUtf8(kindFile.readAll()).trimmed());
        kindRead = true;
    }

    QFile reportFile(summary.reportFilePath);
    if (reportFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        summary.reportText = QString::fromUtf8(reportFile.readAll());
        if (!kindRead) {
            summary.kind = kindFromReportText(summary.reportText);
        }
    }

    if (!summary.reportText.trimmed().isEmpty()) {
        showCrashReportDialogOnGuiThread(summary.reportText, summary.folderPath, summary.kind);
    } else {
        dismissPendingReport();
    }
    return true;
#else
    return false;
#endif
}

void CrashReporter::install() {
#ifdef _WIN32
    Win32StackWalker::initialize();
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
    std::set_terminate(terminateHandler);
    _set_purecall_handler(purecallHandler);
    _set_invalid_parameter_handler(invalidParameterHandler);
#endif
    if (!g_previousQtHandler) {
        g_previousQtHandler = qInstallMessageHandler(qtMessageHandler);
    }
}

void CrashReporter::installGuiHangWatchdog() {
#ifdef _WIN32
    QCoreApplication* app = QCoreApplication::instance();
    if (!app) {
        return;
    }

    g_mainThreadId.store(GetCurrentThreadId());
    touchGuiHeartbeat();

    auto* heartbeatTimer = new QTimer(app);
    heartbeatTimer->setInterval(kGuiHeartbeatIntervalMs);
    QObject::connect(heartbeatTimer, &QTimer::timeout, app, []() { touchGuiHeartbeat(); });
    heartbeatTimer->start();

    QObject::connect(app, &QCoreApplication::aboutToQuit, app, []() {
        g_guiHangWatchdogStop.store(true);
        Win32StackWalker::shutdown();
    });

    std::thread(guiHangWatchdogThreadMain).detach();
#else
    Q_UNUSED(kGuiHeartbeatIntervalMs);
#endif
}

void CrashReporter::setContextProvider(CrashContextProvider provider) {
    {
        std::lock_guard<std::mutex> lock(g_contextMutex);
        g_contextProvider = std::move(provider);
    }
    refreshContextCacheFromProvider();
}

bool CrashReporter::hasPendingReport() {
    return QFile::exists(pendingMarkerPath());
}

CrashReportSummary CrashReporter::pendingReport() {
    CrashReportSummary summary;
    QFile pendingFile(pendingMarkerPath());
    if (!pendingFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return summary;
    }
    const QString folderPath = QString::fromUtf8(pendingFile.readAll()).trimmed();
    if (folderPath.isEmpty() || !QDir(folderPath).exists()) {
        return summary;
    }

    summary.folderPath = folderPath;
    summary.reportFilePath = QDir(folderPath).filePath(QStringLiteral("report.txt"));
    summary.dumpFilePath = QDir(folderPath).filePath(QStringLiteral("crash.dmp"));

    bool kindRead = false;
    QFile kindFile(QDir(folderPath).filePath(QStringLiteral("kind.txt")));
    if (kindFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        summary.kind = kindFromString(QString::fromUtf8(kindFile.readAll()).trimmed());
        kindRead = true;
    }

    QFile reportFile(summary.reportFilePath);
    if (reportFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        summary.reportText = QString::fromUtf8(reportFile.readAll());
        if (!kindRead) {
            summary.kind = kindFromReportText(summary.reportText);
        }
    }
    return summary;
}

void CrashReporter::dismissPendingReport() {
    QFile::remove(pendingMarkerPath());
}

QString CrashReporter::crashRootDirectory() {
    return crashRootDirectoryPath();
}

bool CrashReporter::openPathInExplorer(const QString& path) {
    if (path.isEmpty()) {
        return false;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QString CrashReporter::latestReportDirectory() {
    const CrashReportSummary summary = pendingReport();
    if (!summary.folderPath.isEmpty()) {
        return summary.folderPath;
    }

    QDir root(crashRootDirectoryPath());
    const QFileInfoList entries =
        root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    return entries.isEmpty() ? QString() : entries.first().absoluteFilePath();
}

CrashReportKind CrashReporter::kindFromReportText(const QString& reportText) {
    const QStringList lines = reportText.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (line.startsWith(QStringLiteral("kind:"))) {
            return kindFromString(line.mid(5).trimmed());
        }
    }
    if (reportText.contains(QStringLiteral("not responding"), Qt::CaseInsensitive)
        || reportText.contains(QStringLiteral("(hang)"), Qt::CaseInsensitive)) {
        return CrashReportKind::Hang;
    }
    if (reportText.contains(QStringLiteral("Qt fatal"), Qt::CaseInsensitive)) {
        return CrashReportKind::QtFatal;
    }
    return CrashReportKind::Crash;
}

bool CrashReporter::writeUserNote(const QString& folderPath, const QString& note) {
    if (folderPath.isEmpty() || note.trimmed().isEmpty()) {
        return false;
    }
    return writeTextFile(QDir(folderPath).filePath(QStringLiteral("user_note.txt")), note.trimmed());
}

bool CrashReporter::exportReportFolderAsZip(const QString& folderPath, const QString& zipPath) {
    if (folderPath.isEmpty() || zipPath.isEmpty()) {
        return false;
    }

    QDir().mkpath(QFileInfo(zipPath).absolutePath());
    QFile::remove(zipPath);

    QStringList filesToPack;
    const QStringList candidates = {
        QStringLiteral("report.txt"),
        QStringLiteral("recent_log.txt"),
        QStringLiteral("crash.dmp"),
        QStringLiteral("user_note.txt"),
        QStringLiteral("kind.txt"),
    };
    for (const QString& name : candidates) {
        const QString absolutePath = QDir(folderPath).filePath(name);
        if (QFile::exists(absolutePath)) {
            filesToPack.append(name);
        }
    }
    if (filesToPack.isEmpty()) {
        return false;
    }

    QProcess process;
    process.setProgram(QStringLiteral("tar"));
    QStringList arguments;
    arguments << QStringLiteral("-a") << QStringLiteral("-cf") << QDir::toNativeSeparators(zipPath);
    for (const QString& name : filesToPack) {
        arguments << name;
    }
    process.setArguments(arguments);
    process.setWorkingDirectory(folderPath);
#ifdef _WIN32
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif
    process.start();
    if (!process.waitForStarted(5000)) {
        return false;
    }
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished(3000);
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}
