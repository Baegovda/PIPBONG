#include "ProfileForegroundSync.h"

#include "ProfileManager.h"
#include "core/capture/ScreenCapture.h"

#ifdef _WIN32
#    include <windows.h>
#endif

namespace ProfileForegroundSync {

#ifdef _WIN32
bool isShellTransientWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return true;
    }
    wchar_t className[256]{};
    if (GetClassNameW(hwnd, className, 256) <= 0) {
        return false;
    }
    static const wchar_t* kIgnoredClasses[] = {
        L"#32771",
        L"ForegroundStaging",
        L"MultitaskingViewFrame",
        L"XamlExplorerHostIslandWindow",
        L"TaskSwitcherWnd",
        L"Xaml_Window",
        L"Windows.UI.Core.CoreWindow",
        L"Shell_TrayWnd",
        L"Shell_SecondaryTrayWnd",
        L"NotifyIconOverflowWindow",
        L"WorkerW",
        L"Progman",
    };
    for (const wchar_t* ignored : kIgnoredClasses) {
        if (_wcsicmp(className, ignored) == 0) {
            return true;
        }
    }
    return false;
}

bool isAltTabModifierHeld() {
    return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0 || (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0
           || (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;
}

bool isPipbongProcessWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid == GetCurrentProcessId();
}

Snapshot snapshotFrom(HWND foregroundHwnd) {
    Snapshot snap;
    if (!foregroundHwnd || !IsWindow(foregroundHwnd)) {
        return snap;
    }
    snap.pipbong = isPipbongProcessWindow(foregroundHwnd);
    snap.rootHwnd = GetAncestor(foregroundHwnd, GA_ROOT);
    if (!snap.rootHwnd || !IsWindow(snap.rootHwnd)) {
        snap.rootHwnd = nullptr;
        return snap;
    }
    snap.shellTransient = isShellTransientWindow(snap.rootHwnd);
    wchar_t titleBuffer[512]{};
    GetWindowTextW(snap.rootHwnd, titleBuffer, 512);
    snap.title = QString::fromWCharArray(titleBuffer).trimmed();

    ScreenCapture::TargetWindowInfo info;
    if (ScreenCapture::queryWindowInfo(snap.rootHwnd, info) && !info.processPath.empty()) {
        snap.processPath = QString::fromStdWString(info.processPath);
    }
    return snap;
}
#endif

QString profileIdForProcessPath(const ProfileManager& profileManager, const QString& processPath) {
    if (processPath.isEmpty()) {
        return {};
    }
    QString bestId;
    int bestScore = 0;
    for (const ProfileManager::Profile& profile : profileManager.profiles()) {
        if (profileManager.isDefaultProfile(profile.id)) {
            continue;
        }
        const QString mainProc = profileManager.linkedTargetProcessPath(profile.id);
        const QString subProc = profileManager.subLinkedTargetProcessPath(profile.id);
        const auto scoreFor = [&](const QString& proc, const QString& binding) -> int {
            if (proc.isEmpty() || processPath.compare(proc, Qt::CaseInsensitive) != 0) {
                return 0;
            }
            return binding.isEmpty() ? 1 : static_cast<int>(binding.length());
        };
        const int score = qMax(scoreFor(mainProc, profile.targetWindowTitle),
                               scoreFor(subProc, profile.subTargetWindowTitle));
        if (score > bestScore) {
            bestScore = score;
            bestId = profile.id;
        }
    }
    return bestId;
}

QString profileIdForTitleBinding(const ProfileManager& profileManager,
                                 const QString& foregroundTitle,
                                 const QString& processPath) {
    const QString trimmed = foregroundTitle.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    QString bestId;
    int bestLength = 0;
    for (const ProfileManager::Profile& profile : profileManager.profiles()) {
        if (profileManager.isDefaultProfile(profile.id)) {
            continue;
        }
        const QString mainProc = profileManager.linkedTargetProcessPath(profile.id);
        const QString subProc = profileManager.subLinkedTargetProcessPath(profile.id);

        const auto considerBinding = [&](const QString& binding, const QString& linkedProc) {
            if (binding.isEmpty()) {
                return;
            }
            if (!trimmed.contains(binding, Qt::CaseInsensitive) || binding.length() <= bestLength) {
                return;
            }
            if (!linkedProc.isEmpty() && !processPath.isEmpty()
                && processPath.compare(linkedProc, Qt::CaseInsensitive) != 0) {
                return;
            }
            bestLength = binding.length();
            bestId = profile.id;
        };
        considerBinding(profile.targetWindowTitle, mainProc);
        considerBinding(profile.subTargetWindowTitle, subProc);
    }
    return bestId;
}

ResolveResult resolve(const ProfileManager& profileManager, const Snapshot& snapshot) {
    ResolveResult result;
    result.profileId = profileManager.defaultProfileId();
    result.matchKind = MatchKind::DefaultFallback;

#ifdef _WIN32
    if (!snapshot.rootHwnd || snapshot.pipbong || snapshot.shellTransient) {
        return result;
    }

    const QString byProcess = profileIdForProcessPath(profileManager, snapshot.processPath);
    if (!byProcess.isEmpty() && !profileManager.isDefaultProfile(byProcess)) {
        result.profileId = byProcess;
        result.matchKind = MatchKind::ProcessPath;
        return result;
    }

    const QString byTitle =
        profileIdForTitleBinding(profileManager, snapshot.title, snapshot.processPath);
    if (!byTitle.isEmpty() && !profileManager.isDefaultProfile(byTitle)) {
        result.profileId = byTitle;
        result.matchKind = MatchKind::TitleBinding;
        return result;
    }
#else
    Q_UNUSED(snapshot);
#endif

    if (result.profileId.isEmpty()) {
        result.profileId = profileManager.defaultProfileId();
    }
    return result;
}

} // namespace ProfileForegroundSync
