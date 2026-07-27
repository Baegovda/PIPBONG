#pragma once

#include <functional>

#include <QString>

#ifdef _WIN32
#include <windows.h>
#endif

class ProfileManager;

class TargetWindowController {
public:
    struct HostCallbacks {
        std::function<bool()> isActiveDefaultProfile;
        std::function<std::wstring()> currentTargetWindowTitleW;
        std::function<void()> scheduleAutoSave;
        std::function<void()> syncEffectiveTargetWindowTitleToCapture;
    };

    void setProfileManager(ProfileManager* profileManager);
    void setHostCallbacks(const HostCallbacks& callbacks);

#ifdef _WIN32
    void applyForegroundCaptureHints(HWND hwnd, const QString& foregroundTitle);
    void healLinkedTargetProcessPathFromForeground(HWND hwnd, const QString& foregroundTitle);
    void rememberProfileLinkedForeground(HWND hwnd, const QString& foregroundTitle);

    HWND lastProfileLinkedForegroundHwnd() const { return m_lastProfileLinkedForegroundHwnd; }
    bool lastProfileLinkedForegroundIsSub() const { return m_lastProfileLinkedForegroundIsSub; }
    void clearLastProfileLinkedForeground() {
        m_lastProfileLinkedForegroundHwnd = nullptr;
        m_lastProfileLinkedForegroundIsSub = false;
    }
#endif

private:
    ProfileManager* m_profileManager = nullptr;
    HostCallbacks m_host;
#ifdef _WIN32
    HWND m_lastProfileLinkedForegroundHwnd = nullptr;
    bool m_lastProfileLinkedForegroundIsSub = false;
#endif
};
