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
    if(!m_host.mayPersist || m_host.mayPersist())healLinkedTargetProcessPathFromForeground(hwnd, foregroundTitle);
    if(m_host.mayApplyCapture && !m_host.mayApplyCapture()) {
        rememberProfileLinkedForeground(hwnd,foregroundTitle);return;
    }
    ScreenCapture::setForegroundHintWindow(hwnd);
    ScreenCapture::invalidateTargetWindowCache();
    if (m_host.syncEffectiveTargetWindowTitleToCapture) {
        m_host.syncEffectiveTargetWindowTitleToCapture();
    }
    rememberProfileLinkedForeground(hwnd, foregroundTitle);
}

void TargetWindowController::healLinkedTargetProcessPathFromForeground(HWND hwnd,const QString& title) {
    if(!m_profileManager || (m_host.isActiveDefaultProfile && m_host.isActiveDefaultProfile())
        || (m_host.mayPersist && !m_host.mayPersist()) || !hwnd || !IsWindow(hwnd))return;
    ScreenCapture::TargetWindowInfo info;
    if(!ScreenCapture::queryWindowInfo(hwnd,info) || info.processPath.empty())return;
    const QString path=QString::fromStdWString(info.processPath),id=m_profileManager->activeProfileId();
    for(const auto& profile:m_profileManager->profiles())if(profile.id!=id) {
        for(const auto& owned:{m_profileManager->linkedTargetProcessPath(profile.id),m_profileManager->subLinkedTargetProcessPath(profile.id)})
            if(!owned.isEmpty() && owned.compare(path,Qt::CaseInsensitive)==0)return;
    }
    const QString main=m_host.currentTargetWindowTitleW?QString::fromStdWString(m_host.currentTargetWindowTitleW()).trimmed():QString();
    const QString sub=m_profileManager->subTargetWindowTitle(id).trimmed();
    const bool isSub=ForegroundRunGate::preferSubTitle(title,main,sub);
    const auto& binding=isSub?sub:main;
    if(binding.isEmpty() || !title.contains(binding,Qt::CaseInsensitive))return;
    const auto stored=isSub?m_profileManager->subLinkedTargetProcessPath(id):m_profileManager->linkedTargetProcessPath(id);
    if(!stored.isEmpty())return;
    const bool saved=isSub?m_profileManager->updateProfileSubTargetBinding(id,binding,path)
        :m_profileManager->updateProfileTargetBinding(id,binding,path);
    if(saved && m_host.scheduleAutoSave)m_host.scheduleAutoSave();
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
    const bool sub=ForegroundRunGate::foregroundMatchesScopedSubTarget(foregroundTitle,hwnd,mainBinding,subBinding,mainProcessPath,subProcessPath);
    const bool main=ForegroundRunGate::foregroundMatchesScopedMainTarget(foregroundTitle,hwnd,mainBinding,subBinding,mainProcessPath,subProcessPath);
    if(!main && !sub)return;
    m_lastProfileLinkedForegroundHwnd = hwnd;
    m_lastProfileLinkedForegroundIsSub = sub && (!main || ForegroundRunGate::preferSubTitle(foregroundTitle,mainBinding,subBinding));
}
#endif
