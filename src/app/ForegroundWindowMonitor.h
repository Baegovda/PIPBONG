#pragma once

#include "app/ForegroundWindowState.h"

#include <QObject>

#ifdef _WIN32
#    include <windows.h>
#endif

class QTimer;

/// Single real-time source for the desktop foreground window (Win32 EVENT_SYSTEM_FOREGROUND).
class ForegroundWindowMonitor : public QObject {
    Q_OBJECT
public:
    explicit ForegroundWindowMonitor(QObject* parent = nullptr);
    ~ForegroundWindowMonitor() override;

    void start();
    void stop();

    /// Refresh cached state from GetForegroundWindow (required when PIPBONG is focused — WinEvent skips own process).
    /// Returns true when cached state changed (may have emitted foregroundChanged).
    bool syncFromDesktopForeground();

    const ForegroundWindowState& currentState() const { return m_state; }

#ifdef _WIN32
    HWND currentRootHwnd() const { return m_state.rootHwnd; }
    static ForegroundWindowState buildStateFromHwnd(HWND foregroundHwnd);
    static bool isShellTransientWindow(HWND hwnd);
    /// Desktop wallpaper host (Progman / WorkerW) — unmatched foreground → default profile.
    static bool isDesktopShellHostHwnd(HWND hwnd);
    static bool isAltTabModifierHeld();
    static bool isPipbongProcessWindow(HWND hwnd);
#endif
    QString currentTitle() const { return m_state.title; }
    QString currentProcessPath() const { return m_state.processPath; }
    bool isPipbongForeground() const { return m_state.pipbong; }
    bool isShellTransientForeground() const { return m_state.shellTransient; }

signals:
    void foregroundChanged(const ForegroundWindowState& state);
    void altModifierReleased();

private slots:
    void onWinEvent();

private:
#ifdef _WIN32
    void refreshFromForegroundHwnd(HWND foregroundHwnd, bool forceEmit);
    static void CALLBACK winEventProc(HWINEVENTHOOK,
                                      DWORD event,
                                      HWND hwnd,
                                      LONG,
                                      LONG,
                                      DWORD,
                                      DWORD);

    HWINEVENTHOOK m_hook = nullptr;
    bool m_altTabModifierWasHeld = false;
    bool m_pendingEmit = false;
    QTimer* m_coalesceTimer = nullptr;
    QTimer* m_pollTimer = nullptr;
#endif
    bool m_started = false;
    ForegroundWindowState m_state;
    quint64 m_nextSeq = 1;
};
