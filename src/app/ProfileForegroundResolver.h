#pragma once

#include "app/ForegroundWindowState.h"

#include <QString>

class ProfileManager;

namespace ProfileForegroundResolver {

enum class MatchKind {
    ProcessPath,
    TitleBinding,
    DefaultFallback,
};

struct ResolveResult {
    QString profileId;
    MatchKind matchKind = MatchKind::DefaultFallback;
};

ResolveResult resolve(const ProfileManager& profileManager, const ForegroundWindowState& state);

/// Updates linked exe paths on non-default profiles when foreground title/path matches their binding.
void healProfileBindingsFromForeground(ProfileManager& profileManager,
                                       const ForegroundWindowState& state);

QString profileIdForProcessPath(const ProfileManager& profileManager, const QString& processPath);

} // namespace ProfileForegroundResolver
