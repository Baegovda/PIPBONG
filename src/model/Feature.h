#pragma once

#include "core/input/HotkeyBinding.h"
#include "core/workflow/Workflow.h"
#include "model/FeatureCaptureTargetScope.h"
#include "model/FeatureRunMode.h"
#include "model/UserInputInterruptMode.h"

#include <memory>
#include <string>

constexpr int kTriggerCooldownStepMs = 1000;
constexpr int kDefaultTriggerCooldownMs = 1000;
constexpr int kTriggerCooldownMaxSeconds = 600;
int snapTriggerCooldownMs(int ms);
int triggerCooldownSecondsFromMs(int ms);
int triggerCooldownMsFromSeconds(int seconds);

class Feature {
public:
    Feature();
    explicit Feature(std::string name);

    const std::string& id() const { return m_id; }
    void setId(const std::string& id) { m_id = id; }
    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    FeatureRunMode runMode() const { return m_runMode; }
    void setRunMode(FeatureRunMode mode) { m_runMode = mode; }

    int repeatCount() const { return m_repeatCount; }
    void setRepeatCount(int count) { m_repeatCount = count < 1 ? 1 : count; }

    int infiniteExitAfterConsecutiveMisses() const { return m_infiniteExitAfterConsecutiveMisses; }
    void setInfiniteExitAfterConsecutiveMisses(int count) {
        m_infiniteExitAfterConsecutiveMisses = count < 0 ? 0 : count;
    }

    UserInputInterruptMode userInputInterruptMode() const { return m_userInputInterruptMode; }
    void setUserInputInterruptMode(UserInputInterruptMode mode) { m_userInputInterruptMode = mode; }

    bool pointerVisualFeedback() const { return m_pointerVisualFeedback; }
    void setPointerVisualFeedback(bool enabled) { m_pointerVisualFeedback = enabled; }

    bool restoreMousePositionOnEnd() const { return m_restoreMousePositionOnEnd; }
    void setRestoreMousePositionOnEnd(bool enabled) { m_restoreMousePositionOnEnd = enabled; }

    bool lockMouseToScreenCenterDuringRun() const { return m_lockMouseToScreenCenterDuringRun; }
    void setLockMouseToScreenCenterDuringRun(bool enabled) { m_lockMouseToScreenCenterDuringRun = enabled; }

    bool lockMouseToCurrentPositionDuringRun() const { return m_lockMouseToCurrentPositionDuringRun; }
    void setLockMouseToCurrentPositionDuringRun(bool enabled) { m_lockMouseToCurrentPositionDuringRun = enabled; }

    int lockMouseDuringFirstLoopCount() const { return m_lockMouseDuringFirstLoopCount; }
    void setLockMouseDuringFirstLoopCount(int count) { m_lockMouseDuringFirstLoopCount = count < 0 ? 0 : count; }

    int unlockMouseOnBlockFailureCount() const { return m_unlockMouseOnBlockFailureCount; }
    void setUnlockMouseOnBlockFailureCount(int count) {
        m_unlockMouseOnBlockFailureCount = count < 1 ? 1 : count;
    }

    bool roiCorrection() const { return m_roiCorrection; }
    void setRoiCorrection(bool enabled) { m_roiCorrection = enabled; }

    int roiCorrectionExpandPercent() const { return m_roiCorrectionExpandPercent; }
    void setRoiCorrectionExpandPercent(int percent);

    bool editFirstTemplateRoiOnStart() const { return m_editFirstTemplateRoiOnStart; }
    void setEditFirstTemplateRoiOnStart(bool enabled) { m_editFirstTemplateRoiOnStart = enabled; }

    int triggerCooldownMs() const { return m_triggerCooldownMs; }
    void setTriggerCooldownMs(int ms) { m_triggerCooldownMs = snapTriggerCooldownMs(ms); }

    int loopIntervalMs() const { return m_loopIntervalMs; }
    void setLoopIntervalMs(int ms);

    bool loopIntervalRandomRange() const { return m_loopIntervalRandomRange; }
    void setLoopIntervalRandomRange(bool enabled) { m_loopIntervalRandomRange = enabled; }

    int loopIntervalMinMs() const { return m_loopIntervalMinMs; }
    void setLoopIntervalMinMs(int ms);

    int loopIntervalMaxMs() const { return m_loopIntervalMaxMs; }
    void setLoopIntervalMaxMs(int ms);

    /// Delay before the next loop iteration for Hold / infinite / N-repeat (0 when fixed ms is 0 and not random).
    int resolvedLoopIntervalMs() const;

    /// True when run mode supports configuring a gap between loop iterations.
    bool supportsLoopInterval() const;

    /// True when run mode supports ROI correction (infinite repeat or N≥2).
    bool roiCorrectionSessionEligible() const;

    Workflow& workflow() { return m_workflow; }
    const Workflow& workflow() const { return m_workflow; }

    const HotkeyBinding& hotkey() const { return m_hotkey; }
    void setHotkey(const HotkeyBinding& hotkey) { m_hotkey = hotkey; }

    bool hotkeyAllowExtraModifiers() const { return m_hotkeyAllowExtraModifiers; }
    void setHotkeyAllowExtraModifiers(bool allow) { m_hotkeyAllowExtraModifiers = allow; }

    FeatureCaptureTargetScope captureTargetScope() const { return m_captureTargetScope; }
    void setCaptureTargetScope(FeatureCaptureTargetScope scope) { m_captureTargetScope = scope; }

    std::unique_ptr<Feature> clone() const;
    /// New feature id; clears hotkey by default (paste / profile copy). Library import passes preserveHotkey.
    std::unique_ptr<Feature> duplicateAsNewInstance(bool preserveHotkey = false) const;

private:
    static std::string generateId();

    std::string m_id;
    std::string m_name;
    bool m_enabled = true;
    FeatureRunMode m_runMode = FeatureRunMode::RepeatCount;
    int m_repeatCount = 1;
    int m_infiniteExitAfterConsecutiveMisses = 0;
    UserInputInterruptMode m_userInputInterruptMode = UserInputInterruptMode::Stop;
    bool m_pointerVisualFeedback = true;
    bool m_restoreMousePositionOnEnd = false;
    bool m_lockMouseToScreenCenterDuringRun = false;
    bool m_lockMouseToCurrentPositionDuringRun = false;
    int m_lockMouseDuringFirstLoopCount = 0;
    int m_unlockMouseOnBlockFailureCount = 1;
    bool m_roiCorrection = false;
    int m_roiCorrectionExpandPercent = 110;
    bool m_editFirstTemplateRoiOnStart = false;
    int m_triggerCooldownMs = kDefaultTriggerCooldownMs;
    int m_loopIntervalMs = 0;
    int m_loopIntervalMinMs = 0;
    int m_loopIntervalMaxMs = 0;
    bool m_loopIntervalRandomRange = false;
    bool m_hotkeyAllowExtraModifiers = false;
    FeatureCaptureTargetScope m_captureTargetScope = FeatureCaptureTargetScope::Auto;
    HotkeyBinding m_hotkey;
    Workflow m_workflow;
};
