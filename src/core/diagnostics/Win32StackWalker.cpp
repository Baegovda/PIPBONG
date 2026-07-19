#include "core/diagnostics/Win32StackWalker.h"

#include <QStringList>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>

#include <algorithm>
#include <atomic>
#include <mutex>

namespace {

std::mutex g_symbolMutex;
std::atomic<bool> g_symbolsInitialized{false};

bool ensureSymbolsInitialized() {
    if (g_symbolsInitialized.load()) {
        return true;
    }
    std::lock_guard<std::mutex> lock(g_symbolMutex);
    if (g_symbolsInitialized.load()) {
        return true;
    }
    const BOOL ok = SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    if (ok) {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        g_symbolsInitialized.store(true);
    }
    return ok == TRUE;
}

void appendModuleForAddress(QStringList& lines, void* address) {
    MEMORY_BASIC_INFORMATION memoryInfo{};
    if (!VirtualQuery(address, &memoryInfo, sizeof(memoryInfo))) {
        lines.append(QStringLiteral("  %1").arg(reinterpret_cast<quintptr>(address), 16, 16, QLatin1Char('0')));
        return;
    }

    wchar_t modulePath[MAX_PATH]{};
    const DWORD length =
        GetModuleFileNameW(static_cast<HMODULE>(memoryInfo.AllocationBase), modulePath, MAX_PATH);
    const QString module = length > 0 ? QString::fromWCharArray(modulePath, static_cast<int>(length))
                                      : QStringLiteral("(unknown module)");
    const auto offset = reinterpret_cast<quintptr>(address)
                        - reinterpret_cast<quintptr>(memoryInfo.AllocationBase);
    lines.append(QStringLiteral("  0x%1  %2 + 0x%3")
                     .arg(reinterpret_cast<quintptr>(address), 16, 16, QLatin1Char('0'))
                     .arg(module)
                     .arg(offset, 0, 16));
}

void appendSymbolForAddress(QStringList& lines, void* address) {
    if (!ensureSymbolsInitialized()) {
        appendModuleForAddress(lines, address);
        return;
    }

    alignas(SYMBOL_INFO) unsigned char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    DWORD64 displacement = 0;
    const QString addressText =
        QStringLiteral("0x%1").arg(reinterpret_cast<quintptr>(address), 16, 16, QLatin1Char('0'));
    if (SymFromAddr(GetCurrentProcess(),
                    reinterpret_cast<DWORD64>(address),
                    &displacement,
                    symbol)) {
        const QString name = QString::fromLocal8Bit(symbol->Name);
        QString lineText = QStringLiteral("  %1  %2+0x%3")
                               .arg(addressText, name)
                               .arg(displacement, 0, 16);
        IMAGEHLP_LINE64 lineInfo{};
        lineInfo.SizeOfStruct = sizeof(lineInfo);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(GetCurrentProcess(),
                                 reinterpret_cast<DWORD64>(address),
                                 &lineDisplacement,
                                 &lineInfo)
            && lineInfo.FileName) {
            const QString fileName = QString::fromLocal8Bit(lineInfo.FileName);
            const int slash = qMax(fileName.lastIndexOf(QLatin1Char('/')),
                                   fileName.lastIndexOf(QLatin1Char('\\')));
            lineText += QStringLiteral("  (%1:%2)")
                            .arg(slash >= 0 ? fileName.mid(slash + 1) : fileName)
                            .arg(lineInfo.LineNumber);
        }
        lines.append(lineText);
        return;
    }

    appendModuleForAddress(lines, address);
}

void walkContextStack(QStringList& lines, CONTEXT* context, int maxFrames, HANDLE threadHandle = nullptr) {
    if (!context || maxFrames <= 0) {
        return;
    }

    if (!threadHandle || threadHandle == INVALID_HANDLE_VALUE) {
        threadHandle = GetCurrentThread();
    }

    if (!ensureSymbolsInitialized()) {
        void* frames[64]{};
        const USHORT captured = CaptureStackBackTrace(
            0, static_cast<DWORD>(std::min(maxFrames, 64)), frames, nullptr);
        for (USHORT index = 0; index < captured; ++index) {
            appendModuleForAddress(lines, frames[index]);
        }
        return;
    }

    STACKFRAME64 frame{};
    DWORD machineType = 0;
#ifdef _M_AMD64
    machineType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context->Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context->Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context->Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
#else
    machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context->Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context->Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context->Esp;
    frame.AddrStack.Mode = AddrModeFlat;
#endif

    CONTEXT walkContext = *context;
    for (int index = 0; index < maxFrames; ++index) {
        if (!StackWalk64(machineType,
                         GetCurrentProcess(),
                         threadHandle,
                         &frame,
                         &walkContext,
                         nullptr,
                         SymFunctionTableAccess64,
                         SymGetModuleBase64,
                         nullptr)) {
            break;
        }
        if (frame.AddrPC.Offset == 0) {
            break;
        }
        appendSymbolForAddress(lines, reinterpret_cast<void*>(frame.AddrPC.Offset));
    }
}

class SuspendedThreadGuard {
public:
    explicit SuspendedThreadGuard(HANDLE threadHandle)
        : m_threadHandle(threadHandle) {}

    ~SuspendedThreadGuard() {
        if (m_threadHandle && m_threadHandle != INVALID_HANDLE_VALUE) {
            ResumeThread(m_threadHandle);
            CloseHandle(m_threadHandle);
        }
    }

    SuspendedThreadGuard(const SuspendedThreadGuard&) = delete;
    SuspendedThreadGuard& operator=(const SuspendedThreadGuard&) = delete;

    HANDLE handle() const { return m_threadHandle; }

private:
    HANDLE m_threadHandle = nullptr;
};

} // namespace

namespace Win32StackWalker {

void initialize() {
    ensureSymbolsInitialized();
}

void shutdown() {
    std::lock_guard<std::mutex> lock(g_symbolMutex);
    if (g_symbolsInitialized.exchange(false)) {
        SymCleanup(GetCurrentProcess());
    }
}

void appendStackTrace(QStringList& lines, CONTEXT* context, int maxFrames) {
    walkContextStack(lines, context, maxFrames);
}

void appendCurrentThreadStackTrace(QStringList& lines, int maxFrames) {
    CONTEXT context{};
    RtlCaptureContext(&context);
    walkContextStack(lines, &context, maxFrames);
}

bool appendGuiThreadStackTrace(QStringList& lines, unsigned long mainThreadId, int maxFrames) {
    if (mainThreadId == 0) {
        return false;
    }

    HANDLE threadHandle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, mainThreadId);
    if (!threadHandle || threadHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    SuspendedThreadGuard guard(threadHandle);
    if (SuspendThread(guard.handle()) == static_cast<DWORD>(-1)) {
        return false;
    }

    CONTEXT context{};
    context.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(guard.handle(), &context)) {
        return false;
    }

    walkContextStack(lines, &context, maxFrames, guard.handle());
    return true;
}

} // namespace Win32StackWalker

#endif // _WIN32
