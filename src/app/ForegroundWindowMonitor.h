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

    const ForegroundWindowState& currentState() const { return m_state; }

#ifdef _WIN32
    HWND currentRootHwnd() const { return m_state.rootHwnd; }
    static ForegroundWindowState buildStateFromHwnd(HWND foregroundHwnd);
    static bool isShellTransientWindow(HWND hwnd);
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
#ifdef _WIN32
    void onWinEvent();
#endif

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
#endif
    bool m_started = false;
    ForegroundWindowState m_state;
    quint64 m_nextSeq = 1;
};
