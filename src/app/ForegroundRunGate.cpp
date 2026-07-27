#include "ForegroundRunGate.h"

#include <QtGlobal>

namespace ForegroundRunGate {

bool activeProfileForegroundBindingMatches(const GateInput& input) {
#ifdef _WIN32
    if (input.bindings.activeIsDefault) {
        return false;
    }

    const HWND foregroundHwnd = input.foreground.rootHwnd;
    if (!foregroundHwnd) {
        return false;
    }
    const QString foregroundTitle = input.foreground.title;

    const QString& mainBinding = input.bindings.mainBinding;
    const QString& subBinding = input.bindings.subBinding;
    const QString& mainProcessPath = input.bindings.mainProcessPath;
    const QString& subProcessPath = input.bindings.subProcessPath;
    if (foregroundHwndMatchesLinkedProcess(foregroundHwnd, mainProcessPath, subProcessPath)) {
        return true;
    }
    if (foregroundTitle.isEmpty()) {
        return false;
    }

    const auto titleMatchesBinding = [&](const QString& binding) {
        return !binding.isEmpty()
               && foregroundTitle.contains(binding, Qt::CaseInsensitive);
    };

    const bool mainHit = titleMatchesBinding(mainBinding);
    const bool subHit = titleMatchesBinding(subBinding);
    if (!mainHit && !subHit) {
        return false;
    }
    if (mainHit && subHit) {
        return foregroundMatchesScopedMainTarget(foregroundTitle,
                                               foregroundHwnd,
                                               mainBinding,
                                               subBinding,
                                               mainProcessPath,
                                               subProcessPath)
               || foregroundMatchesScopedSubTarget(foregroundTitle,
                                                   foregroundHwnd,
                                                   mainBinding,
                                                   subBinding,
                                                   mainProcessPath,
                                                   subProcessPath);
    }
    if (subHit) {
        return foregroundMatchesScopedSubTarget(foregroundTitle,
                                                foregroundHwnd,
                                                mainBinding,
                                                subBinding,
                                                mainProcessPath,
                                                subProcessPath);
    }
    return true;
#else
    Q_UNUSED(input);
    return false;
#endif
}

bool profileMainOrSubForegroundActive(const GateInput& input) {
#ifdef _WIN32
    const HWND foregroundHwnd = input.foreground.rootHwnd;
    if (!foregroundHwnd) {
        return false;
    }
    const QString foregroundTitle = input.foreground.title;

    const QString& mainBinding = input.bindings.mainBinding;
    const QString& subBinding = input.bindings.subBinding;
    const QString& mainProcessPath = input.bindings.mainProcessPath;
    const QString& subProcessPath = input.bindings.subProcessPath;
    if (foregroundHwndMatchesLinkedProcess(foregroundHwnd, mainProcessPath, subProcessPath)) {
        return true;
    }
    if (foregroundTitle.isEmpty()) {
        return false;
    }
    if (mainBinding.isEmpty() && subBinding.isEmpty()) {
        return true;
    }

    const auto titleMatchesBinding = [&](const QString& binding) {
        return !binding.isEmpty()
               && foregroundTitle.contains(binding, Qt::CaseInsensitive);
    };

    const bool mainHit = titleMatchesBinding(mainBinding);
    const bool subHit = titleMatchesBinding(subBinding);
    if (!mainHit && !subHit) {
        return false;
    }
    if (mainHit && subHit) {
        return foregroundMatchesScopedMainTarget(foregroundTitle,
                                               foregroundHwnd,
                                               mainBinding,
                                               subBinding,
                                               mainProcessPath,
                                               subProcessPath)
               || foregroundMatchesScopedSubTarget(foregroundTitle,
                                                   foregroundHwnd,
                                                   mainBinding,
                                                   subBinding,
                                                   mainProcessPath,
                                                   subProcessPath);
    }
    if (subHit) {
        return foregroundMatchesScopedSubTarget(foregroundTitle,
                                                foregroundHwnd,
                                                mainBinding,
                                                subBinding,
                                                mainProcessPath,
                                                subProcessPath);
    }
    return true;
#else
    Q_UNUSED(input);
    return true;
#endif
}

bool foregroundProfileMatchesActive(const GateInput& input) {
#ifdef _WIN32
    if (!input.bindings.activeIsDefault && activeProfileForegroundBindingMatches(input)) {
        return true;
    }
    if (profileMainOrSubForegroundActive(input)) {
        return true;
    }

    const QString foregroundProfileId = input.foregroundResolvedProfileId;
    if (foregroundProfileId.isEmpty()) {
        return false;
    }
    if (input.foregroundResolvedIsDefault) {
        return input.bindings.activeIsDefault;
    }
    return foregroundProfileId == input.bindings.activeProfileId;
#else
    Q_UNUSED(input);
    return true;
#endif
}

bool scopedTargetForegroundActive(const GateInput& input) {
    if (!input.feature || !input.feature->requireScopedTargetForeground) {
        return true;
    }
    const FeatureCaptureTargetScope scope = input.feature->captureTargetScope;
    if (scope == FeatureCaptureTargetScope::Auto) {
        return true;
    }

#ifdef _WIN32
    HWND hwnd = input.foreground.rootHwnd;
    if (!hwnd || input.foreground.pipbong) {
        return false;
    }
    const QString foregroundTitle = input.foreground.title;

    const QString& mainBinding = input.bindings.mainBinding;
    const QString& subBinding = input.bindings.subBinding;
    const QString& mainProcessPath = input.bindings.mainProcessPath;
    const QString& subProcessPath = input.bindings.subProcessPath;

    if (scope == FeatureCaptureTargetScope::SubOnly) {
        return foregroundMatchesScopedSubTarget(foregroundTitle,
                                                hwnd,
                                                mainBinding,
                                                subBinding,
                                                mainProcessPath,
                                                subProcessPath);
    }
    if (scope == FeatureCaptureTargetScope::MainOnly) {
        return foregroundMatchesScopedMainTarget(foregroundTitle,
                                               hwnd,
                                               mainBinding,
                                               subBinding,
                                               mainProcessPath,
                                               subProcessPath);
    }
#else
    Q_UNUSED(input);
#endif
    return true;
}

bool runForegroundGateActive(const GateInput& input) {
    if (input.feature && input.feature->runMode == FeatureRunMode::Trigger
        && input.feature->triggerRunWithoutTargetForeground) {
        if (input.runWithoutTargetWindow) {
            return true;
        }
        if (input.bindings.activeIsDefault) {
            return false;
        }
        if (input.triggerBackgroundVisible) {
            return true;
        }
    }

    if (input.runWithoutTargetWindow) {
        return true;
    }

    if (!foregroundProfileMatchesActive(input)) {
        return false;
    }

    if (input.bindings.activeIsDefault) {
        return true;
    }

    if (input.bindings.mainBinding.isEmpty() && input.bindings.subBinding.isEmpty()) {
        return true;
    }

    if (input.feature && input.feature->requireScopedTargetForeground
        && input.feature->captureTargetScope != FeatureCaptureTargetScope::Auto) {
        return scopedTargetForegroundActive(input);
    }

    return true;
}

} // namespace ForegroundRunGate
