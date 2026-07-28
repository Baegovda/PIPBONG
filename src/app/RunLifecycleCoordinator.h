#pragma once

#include "app/SessionRunPolicy.h"

#include <string>

struct FeatureRunSession;

/// Run session lifecycle policy helpers (AGENTS.md §8.21 R5.2).
/// Maps `FeatureRunSession` → `SessionRunPolicyInput` and continuation rules without HWND/hotkey I/O.
class RunLifecycleCoordinator {
public:
    static SessionRunPolicyInput policyInputFrom(const FeatureRunSession& session);

    /// `holdBindingStillActive` from `HotkeyManager::isHoldBindingStillActiveForRun` (Hold mode only).
    static bool shouldContinueSession(const FeatureRunSession& session, bool holdBindingStillActive);
};
