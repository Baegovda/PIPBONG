#pragma once

#include "app/SessionRunPolicy.h"

#include <map>
#include <string>
#include <vector>

struct FeatureRunSession;

/// Run session lifecycle policy helpers (AGENTS.md §8.21 R5.2).
/// Maps `FeatureRunSession` → `SessionRunPolicyInput` and continuation rules without HWND/hotkey I/O.
class RunLifecycleCoordinator {
public:
    static SessionRunPolicyInput policyInputFrom(const FeatureRunSession& session);

    /// Build policy inputs for all entries in `m_runSessions` (optional `excludeFeatureId` skips one key).
    static std::vector<SessionRunPolicyInput> policyInputsFromSessions(
        const std::map<std::string, FeatureRunSession>& sessions,
        const std::string* excludeFeatureId = nullptr);

    /// `holdBindingStillActive` from `HotkeyManager::isHoldBindingStillActiveForRun` (Hold mode only).
    static bool shouldContinueSession(const FeatureRunSession& session, bool holdBindingStillActive);
};
