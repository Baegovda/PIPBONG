#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <crtdbg.h>
#include <windows.h>
#endif

#include "core/diagnostics/CrashReporter.h"

#include "PipbongVersion.h"
#include "ui/diagnostics/CrashReportDialog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QUrl>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <string>

#include <QStringConverter>

namespace {

constexpr int kMaxRecentLogLines = 800;
constexpr int kMaxRetainedCrashFolders = 10;
constexpr wchar_t kPendingFileName[] = L"pending.txt";
constexpr wchar_t kCrashReportViewerArg[] = L"--crash-report";

struct CrashArtifacts {
    QString folderPath;
    QString reportText;
};

std::mutex g_logMutex;
std::deque<QString> g_recentLogLines;
QtMessageHandler g_previousQtHandler = nullptr;
std::atomic<bool> g_writingCrashReport{false};

#ifdef _WIN32
CrashArtifacts writeCrashArtifacts(const QString& reason,
                                   EXCEPTION_POINTERS* exceptionInfo,
                                   CONTEXT* fallbackContext);
void presentCrashReport(const CrashArtifacts& artifacts, bool allowInProcessUi);
#endif

#ifdef _WIN32
struct MinidumpExceptionInformation {
    DWORD threadId = 0;
    EXCEPTION_POINTERS* exceptionPointers = nullptr;
    BOOL clientPointers = FALSE;
};

using MiniDumpWriteDumpFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, DWORD, MinidumpExceptionInformation*, void*, void*);
#endif

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
                writeCrashArtifacts(reason, nullptr, &contextRecord);
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

void launchDetachedCrashReportViewer(const QString& folderPath) {
    const QString executablePath = currentExecutablePath();
    if (executablePath.isEmpty() || folderPath.isEmpty()) {
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
        return;
    }
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
}

void showCrashReportDialogOnGuiThread(const QString& reportText, const QString& folderPath) {
    CrashReportDialog dialog(reportText, folderPath, nullptr, true);
    dialog.exec();
    CrashReporter::dismissPendingReport();
}

void presentCrashReport(const CrashArtifacts& artifacts, bool allowInProcessUi) {
    if (artifacts.reportText.trimmed().isEmpty()) {
        return;
    }

    QCoreApplication* app = QCoreApplication::instance();
    if (allowInProcessUi && app) {
        const auto showOnGuiThread = [reportText = artifacts.reportText, folderPath = artifacts.folderPath]() {
            showCrashReportDialogOnGuiThread(reportText, folderPath);
        };
        if (QThread::currentThread() == app->thread()) {
            showOnGuiThread();
        } else {
            QMetaObject::invokeMethod(app, showOnGuiThread, Qt::BlockingQueuedConnection);
        }
        return;
    }

    launchDetachedCrashReportViewer(artifacts.folderPath);
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

void appendModuleForAddress(QStringList& lines, void* address) {
    MEMORY_BASIC_INFORMATION memoryInfo{};
    if (!VirtualQuery(address, &memoryInfo, sizeof(memoryInfo))) {
        lines.append(QStringLiteral("  %1").arg(reinterpret_cast<quintptr>(address), 16, 16, QLatin1Char('0')));
        return;
    }

    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(static_cast<HMODULE>(memoryInfo.AllocationBase), modulePath, MAX_PATH);
    const QString module = length > 0 ? QString::fromWCharArray(modulePath, static_cast<int>(length))
                                      : QStringLiteral("(unknown module)");
    const auto offset = reinterpret_cast<quintptr>(address)
                        - reinterpret_cast<quintptr>(memoryInfo.AllocationBase);
    lines.append(QStringLiteral("  0x%1  %2 + 0x%3")
                     .arg(reinterpret_cast<quintptr>(address), 16, 16, QLatin1Char('0'))
                     .arg(module)
                     .arg(offset, 0, 16));
}

void appendStackTrace(QStringList& lines, CONTEXT* context, int maxFrames = 32) {
    Q_UNUSED(context);
    void* frames[64]{};
    const USHORT captured =
        CaptureStackBackTrace(0, static_cast<DWORD>(std::min(maxFrames, 64)), frames, nullptr);
    for (USHORT index = 0; index < captured; ++index) {
        appendModuleForAddress(lines, frames[index]);
    }
}

QString buildCrashReportText(const QString& reason,
                             EXCEPTION_POINTERS* exceptionInfo,
                             CONTEXT* fallbackContext) {
    QStringList lines;
    lines.append(QStringLiteral("PIPBONG crash report"));
    lines.append(QStringLiteral("version: %1").arg(QStringLiteral(PIPBONG_VERSION)));
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
    lines.append(QStringLiteral("--- stack trace ---"));
    if (exceptionInfo && exceptionInfo->ContextRecord) {
        appendStackTrace(lines, exceptionInfo->ContextRecord);
    } else if (fallbackContext) {
        appendStackTrace(lines, fallbackContext);
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

CrashArtifacts writeCrashArtifacts(const QString& reason,
                                   EXCEPTION_POINTERS* exceptionInfo,
                                   CONTEXT* fallbackContext) {
    CrashArtifacts artifacts;
    const QString root = crashRootDirectoryPath();
    artifacts.folderPath = QDir(root).filePath(makeCrashFolderName());
    QDir().mkpath(artifacts.folderPath);

    const QString reportPath = QDir(artifacts.folderPath).filePath(QStringLiteral("report.txt"));
    const QString dumpPath = QDir(artifacts.folderPath).filePath(QStringLiteral("crash.dmp"));
    const QString logPath = QDir(artifacts.folderPath).filePath(QStringLiteral("recent_log.txt"));

    artifacts.reportText = buildCrashReportText(reason, exceptionInfo, fallbackContext);
    writeTextFile(reportPath, artifacts.reportText);

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
        writeCrashArtifacts(QStringLiteral("unhandled SEH exception"), exceptionInfo, nullptr);
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
        writeCrashArtifacts(QStringLiteral("std::terminate"), nullptr, &context);
    presentCrashReport(artifacts, QCoreApplication::instance() != nullptr);
    std::abort();
}

void purecallHandler() {
    if (!g_writingCrashReport.exchange(true)) {
        const CrashArtifacts artifacts = writeCrashArtifacts(QStringLiteral("_purecall"), nullptr, nullptr);
        presentCrashReport(artifacts, false);
    }
    std::abort();
}

void invalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
    if (!g_writingCrashReport.exchange(true)) {
        const CrashArtifacts artifacts =
            writeCrashArtifacts(QStringLiteral("invalid parameter handler"), nullptr, nullptr);
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

    QFile reportFile(summary.reportFilePath);
    if (reportFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        summary.reportText = QString::fromUtf8(reportFile.readAll());
    }

    if (!summary.reportText.trimmed().isEmpty()) {
        showCrashReportDialogOnGuiThread(summary.reportText, summary.folderPath);
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
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
    std::set_terminate(terminateHandler);
    _set_purecall_handler(purecallHandler);
    _set_invalid_parameter_handler(invalidParameterHandler);
#endif
    if (!g_previousQtHandler) {
        g_previousQtHandler = qInstallMessageHandler(qtMessageHandler);
    }
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

    QFile reportFile(summary.reportFilePath);
    if (reportFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        summary.reportText = QString::fromUtf8(reportFile.readAll());
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
