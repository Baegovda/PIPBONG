#include "TargetWindowController.h"

#include "ProfileManager.h"
#include "app/ForegroundRunGate.h"
#include "core/capture/ScreenCapture.h"

#include <QFileInfo>

#ifdef _WIN32
#include <windows.h>
#endif

void TargetWindowController::setProfileManager(ProfileManager* profileManager) {
    m_profileManager = profileManager;
}

void TargetWindowController::setHostCallbacks(const HostCallbacks& callbacks) {
    m_host = callbacks;
}

#ifdef _WIN32
void TargetWindowController::applyForegroundCaptureHints(HWND hwnd, const QString& foregroundTitle) {
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }
    ScreenCapture::setForegroundHintWindow(hwnd);
    healLinkedTargetProcessPathFromForeground(hwnd, foregroundTitle);
    ScreenCapture::invalidateTargetWindowCache();
    if (m_host.syncEffectiveTargetWindowTitleToCapture) {
        m_host.syncEffectiveTargetWindowTitleToCapture();
    }
    rememberProfileLinkedForeground(hwnd, foregroundTitle);
}

void TargetWindowController::healLinkedTargetProcessPathFromForeground(HWND hwnd,
                                                                     const QString& foregroundTitle) {
    if (!m_profileManager || (m_host.isActiveDefaultProfile && m_host.isActiveDefaultProfile())
        || !hwnd || !IsWindow(hwnd)) {
        return;
    }

    ScreenCapture::TargetWindowInfo info;
    if (!ScreenCapture::queryWindowInfo(hwnd, info) || info.processPath.empty()) {
        return;
    }
    const QString processPath = QString::fromStdWString(info.processPath);
    const QString profileId = m_profileManager->activeProfileId();
    const QString mainBinding =
        QString::fromStdWString(m_host.currentTargetWindowTitleW ? m_host.currentTargetWindowTitleW()
                                                                : std::wstring{}).trimmed();
    const QString subBinding = m_profileManager->subTargetWindowTitle(profileId).trimmed();

    if (!mainBinding.isEmpty() && foregroundTitle.contains(mainBinding, Qt::CaseInsensitive)) {
        const QString storedPath = m_profileManager->linkedTargetProcessPath(profileId);
        if (storedPath.compare(processPath, Qt::CaseInsensitive) != 0) {
            m_profileManager->updateProfileTargetBinding(profileId, mainBinding, processPath);
            if (m_host.scheduleAutoSave) {
                m_host.scheduleAutoSave();
            }
        }
        return;
    }
    if (!subBinding.isEmpty() && foregroundTitle.contains(subBinding, Qt::CaseInsensitive)) {
        const QString storedPath = m_profileManager->subLinkedTargetProcessPath(profileId);
        if (storedPath.compare(processPath, Qt::CaseInsensitive) != 0) {
            m_profileManager->updateProfileSubTargetBinding(profileId, subBinding, processPath);
            if (m_host.scheduleAutoSave) {
                m_host.scheduleAutoSave();
            }
        }
        return;
    }

    const QString mainProc = m_profileManager->linkedTargetProcessPath(profileId);
    const QString subProc = m_profileManager->subLinkedTargetProcessPath(profileId);
    if (!mainBinding.isEmpty() && !mainProc.isEmpty()
        && processPath.compare(mainProc, Qt::CaseInsensitive) == 0) {
        const QString storedPath = m_profileManager->linkedTargetProcessPath(profileId);
        if (storedPath.compare(processPath, Qt::CaseInsensitive) != 0) {
            m_profileManager->updateProfileTargetBinding(profileId, mainBinding, processPath);
            if (m_host.scheduleAutoSave) {
                m_host.scheduleAutoSave();
            }
        }
        return;
    }
    if (!subBinding.isEmpty() && !subProc.isEmpty()
        && processPath.compare(subProc, Qt::CaseInsensitive) == 0) {
        const QString storedPath = m_profileManager->subLinkedTargetProcessPath(profileId);
        if (storedPath.compare(processPath, Qt::CaseInsensitive) != 0) {
            m_profileManager->updateProfileSubTargetBinding(profileId, subBinding, processPath);
            if (m_host.scheduleAutoSave) {
                m_host.scheduleAutoSave();
            }
        }
    }
}

void TargetWindowController::rememberProfileLinkedForeground(HWND hwnd,
                                                           const QString& foregroundTitle) {
    if (!hwnd || !IsWindow(hwnd) || !m_profileManager
        || (m_host.isActiveDefaultProfile && m_host.isActiveDefaultProfile())) {
        return;
    }
    const QString profileId = m_profileManager->activeProfileId();
    const QString mainBinding =
        QString::fromStdWString(m_host.currentTargetWindowTitleW ? m_host.currentTargetWindowTitleW()
                                                                : std::wstring{}).trimmed();
    const QString subBinding = m_profileManager->subTargetWindowTitle(profileId).trimmed();
    const QString mainProcessPath = m_profileManager->linkedTargetProcessPath(profileId);
    const QString subProcessPath = m_profileManager->subLinkedTargetProcessPath(profileId);
    m_lastProfileLinkedForegroundHwnd = hwnd;
    m_lastProfileLinkedForegroundIsSub =
        ForegroundRunGate::foregroundMatchesScopedSubTarget(foregroundTitle,
                                                            hwnd,
                                                            mainBinding,
                                                            subBinding,
                                                            mainProcessPath,
                                                            subProcessPath);
}
#endif
