#pragma once

#include "app/ForegroundWindowState.h"
#include "model/FeatureCaptureTargetScope.h"
#include "model/FeatureRunMode.h"

#include <QString>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ForegroundRunGate {

struct ProfileBindings {
    QString activeProfileId;
    bool activeIsDefault = true;
    QString mainBinding;
    QString subBinding;
    QString mainProcessPath;
    QString subProcessPath;
};

struct FeatureGateOptions {
    FeatureRunMode runMode = FeatureRunMode::RepeatCount;
    bool triggerRunWithoutTargetForeground = false;
    bool requireScopedTargetForeground = false;
    FeatureCaptureTargetScope captureTargetScope = FeatureCaptureTargetScope::Auto;
};

struct GateInput {
    ForegroundWindowState foreground;
    ProfileBindings bindings;
    QString foregroundResolvedProfileId;
    bool foregroundResolvedIsDefault = false;
    bool runWithoutTargetWindow = false;
    bool triggerBackgroundVisible = false;
    const FeatureGateOptions* feature = nullptr;
};

bool activeProfileForegroundBindingMatches(const GateInput& input);
bool foregroundProfileMatchesActive(const GateInput& input);
bool profileMainOrSubForegroundActive(const GateInput& input);
bool scopedTargetForegroundActive(const GateInput& input);
bool runForegroundGateActive(const GateInput& input);

#ifdef _WIN32
bool foregroundHwndMatchesLinkedProcess(HWND hwnd,
                                        const QString& mainProcessPath,
                                        const QString& subProcessPath);
bool foregroundMatchesScopedSubTarget(const QString& foregroundTitle,
                                      HWND foregroundHwnd,
                                      const QString& mainBinding,
                                      const QString& subBinding,
                                      const QString& mainProcessPath,
                                      const QString& subProcessPath);
bool foregroundMatchesScopedMainTarget(const QString& foregroundTitle,
                                       HWND foregroundHwnd,
                                       const QString& mainBinding,
                                       const QString& subBinding,
                                       const QString& mainProcessPath,
                                       const QString& subProcessPath);
HWND findVisibleTopLevelWindowHwnd(const QString& binding, const QString& processPath = {});
#endif

} // namespace ForegroundRunGate
