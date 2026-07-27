#include "ProfileForegroundResolver.h"

#include "core/capture/ScreenCapture.h"
#include "ProfileManager.h"

#ifdef _WIN32
#    include <windows.h>
#endif

namespace ProfileForegroundResolver {

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

namespace {

#ifdef _WIN32
HWND rootHwndOf(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return nullptr;
    }
    HWND root = GetAncestor(hwnd, GA_ROOT);
    return root && IsWindow(root) ? root : hwnd;
}

QString profileIdForForegroundHwnd(const ProfileManager& profileManager, HWND foregroundRootHwnd) {
    if (!foregroundRootHwnd || !IsWindow(foregroundRootHwnd)) {
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
        const auto scoreIfMatches = [&](const QString& binding, const QString& proc) -> int {
            if (binding.isEmpty()) {
                return 0;
            }
            const HWND hwnd =
                ScreenCapture::findVisibleWindowMatchingTitle(binding.toStdWString(),
                                                              proc.toStdWString());
            if (!hwnd) {
                return 0;
            }
            if (rootHwndOf(hwnd) != foregroundRootHwnd) {
                return 0;
            }
            return static_cast<int>(binding.trimmed().length());
        };
        const int score =
            qMax(scoreIfMatches(profile.targetWindowTitle, mainProc),
                 scoreIfMatches(profile.subTargetWindowTitle, subProc));
        if (score > bestScore) {
            bestScore = score;
            bestId = profile.id;
        }
    }
    return bestId;
}
#endif

bool linkedProcessPathOwnedByOtherProfile(const ProfileManager& profileManager,
                                          const QString& processPath,
                                          const QString& skipProfileId) {
    if (processPath.isEmpty()) {
        return false;
    }
    for (const ProfileManager::Profile& profile : profileManager.profiles()) {
        if (profileManager.isDefaultProfile(profile.id) || profile.id == skipProfileId) {
            continue;
        }
        const QString mainProc = profileManager.linkedTargetProcessPath(profile.id);
        const QString subProc = profileManager.subLinkedTargetProcessPath(profile.id);
        if (!mainProc.isEmpty() && processPath.compare(mainProc, Qt::CaseInsensitive) == 0) {
            return true;
        }
        if (!subProc.isEmpty() && processPath.compare(subProc, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
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
            if (!processPath.isEmpty()) {
                if (!linkedProc.isEmpty()) {
                    if (processPath.compare(linkedProc, Qt::CaseInsensitive) != 0) {
                        return;
                    }
                } else if (linkedProcessPathOwnedByOtherProfile(profileManager, processPath, profile.id)) {
                    return;
                }
            }
            bestLength = binding.length();
            bestId = profile.id;
        };
        considerBinding(profile.targetWindowTitle, mainProc);
        considerBinding(profile.subTargetWindowTitle, subProc);
    }
    return bestId;
}

} // namespace

void healProfileBindingsFromForeground(ProfileManager& profileManager,
                                       const ForegroundWindowState& state) {
#ifdef _WIN32
    if (!state.rootHwnd || state.pipbong || state.shellTransient || state.processPath.isEmpty()) {
        return;
    }
    const QString fgPath = state.processPath;
    const QString fgTitle = state.title;
    for (const ProfileManager::Profile& profile : profileManager.profiles()) {
        if (profileManager.isDefaultProfile(profile.id)) {
            continue;
        }
        const QString mainTitle = profile.targetWindowTitle.trimmed();
        const QString subTitle = profile.subTargetWindowTitle.trimmed();
        if (!mainTitle.isEmpty() && fgTitle.contains(mainTitle, Qt::CaseInsensitive)) {
            const QString stored = profileManager.linkedTargetProcessPath(profile.id);
            if (stored.compare(fgPath, Qt::CaseInsensitive) != 0) {
                profileManager.updateProfileTargetBinding(profile.id, mainTitle, fgPath);
            }
            continue;
        }
        if (!subTitle.isEmpty() && fgTitle.contains(subTitle, Qt::CaseInsensitive)) {
            const QString stored = profileManager.subLinkedTargetProcessPath(profile.id);
            if (stored.compare(fgPath, Qt::CaseInsensitive) != 0) {
                profileManager.updateProfileSubTargetBinding(profile.id, subTitle, fgPath);
            }
        }
    }
#else
    Q_UNUSED(profileManager);
    Q_UNUSED(state);
#endif
}

ResolveResult resolve(const ProfileManager& profileManager, const ForegroundWindowState& state) {
    ResolveResult result;
    result.profileId = profileManager.defaultProfileId();
    result.matchKind = MatchKind::DefaultFallback;

#ifdef _WIN32
    if (!state.rootHwnd || state.pipbong || state.shellTransient) {
        return result;
    }

    const QString byHwnd = profileIdForForegroundHwnd(profileManager, state.rootHwnd);
    if (!byHwnd.isEmpty() && !profileManager.isDefaultProfile(byHwnd)) {
        result.profileId = byHwnd;
        result.matchKind = MatchKind::ForegroundHwnd;
        return result;
    }

    const QString byProcess = profileIdForProcessPath(profileManager, state.processPath);
    if (!byProcess.isEmpty() && !profileManager.isDefaultProfile(byProcess)) {
        result.profileId = byProcess;
        result.matchKind = MatchKind::ProcessPath;
        return result;
    }

    const QString byTitle = profileIdForTitleBinding(profileManager, state.title, state.processPath);
    if (!byTitle.isEmpty() && !profileManager.isDefaultProfile(byTitle)) {
        result.profileId = byTitle;
        result.matchKind = MatchKind::TitleBinding;
        return result;
    }
#else
    Q_UNUSED(state);
#endif

    if (result.profileId.isEmpty()) {
        result.profileId = profileManager.defaultProfileId();
    }
    return result;
}

} // namespace ProfileForegroundResolver
