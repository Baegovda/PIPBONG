#include "ProfileForegroundResolver.h"

#include "ProfileManager.h"

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

ResolveResult resolve(const ProfileManager& profileManager, const ForegroundWindowState& state) {
    ResolveResult result;
    result.profileId = profileManager.defaultProfileId();
    result.matchKind = MatchKind::DefaultFallback;

#ifdef _WIN32
    if (!state.rootHwnd || state.pipbong || state.shellTransient) {
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
