#pragma once

#include <QString>

#ifdef _WIN32
#    include <windows.h>
#endif

class ProfileManager;

namespace ProfileForegroundSync {

enum class MatchKind {
    ProcessPath,
    TitleBinding,
    DefaultFallback,
};

struct Snapshot {
#ifdef _WIN32
    HWND rootHwnd = nullptr;
#endif
    QString title;
    QString processPath;
    bool pipbong = false;
    bool shellTransient = false;
};

struct ResolveResult {
    QString profileId;
    MatchKind matchKind = MatchKind::DefaultFallback;
};

#ifdef _WIN32
Snapshot snapshotFrom(HWND foregroundHwnd);
bool isShellTransientWindow(HWND hwnd);
bool isAltTabModifierHeld();
bool isPipbongProcessWindow(HWND hwnd);
#endif

ResolveResult resolve(const ProfileManager& profileManager, const Snapshot& snapshot);

QString profileIdForProcessPath(const ProfileManager& profileManager, const QString& processPath);

void autoSwitchTiming(MatchKind matchKind, int* stabilityMsOut, int* minIntervalMsOut);

} // namespace ProfileForegroundSync
