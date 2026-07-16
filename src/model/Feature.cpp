#include "model/Feature.h"

#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/blocks/WaitBlock.h"

#include <algorithm>
#include <cmath>
#include <random>

#include <QUuid>

int snapTriggerCooldownMs(int ms) {
    if (ms < 0) {
        return 0;
    }
    const int snapped = (ms / kTriggerCooldownStepMs) * kTriggerCooldownStepMs;
    return std::min(snapped, kTriggerCooldownMaxSeconds * 1000);
}

int triggerCooldownSecondsFromMs(int ms) {
    const int snappedMs = snapTriggerCooldownMs(ms);
    return std::clamp(static_cast<int>(std::lround(snappedMs / 1000.0)),
                      0,
                      kTriggerCooldownMaxSeconds);
}

int triggerCooldownMsFromSeconds(int seconds) {
    return snapTriggerCooldownMs(seconds * 1000);
}

void Feature::setLoopIntervalMs(int ms) {
    m_loopIntervalMs = snapWaitDelayMs(ms);
}

void Feature::setLoopIntervalMinMs(int ms) {
    m_loopIntervalMinMs = snapWaitDelayMs(ms);
}

void Feature::setLoopIntervalMaxMs(int ms) {
    m_loopIntervalMaxMs = snapWaitDelayMs(ms);
}

int Feature::resolvedLoopIntervalMs() const {
    if (m_loopIntervalRandomRange) {
        int minMs = m_loopIntervalMinMs;
        int maxMs = m_loopIntervalMaxMs;
        if (maxMs < minMs) {
            std::swap(minMs, maxMs);
        }
        // Random mode with unset 0~0 bounds used to always yield 0 ms (looked "broken").
        // Fall back to the fixed interval when random bounds were never configured.
        if (minMs <= 0 && maxMs <= 0) {
            return m_loopIntervalMs;
        }
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(minMs, maxMs);
        return dist(gen);
    }
    return m_loopIntervalMs;
}

bool Feature::supportsLoopInterval() const {
    if (m_runMode == FeatureRunMode::Hold || m_runMode == FeatureRunMode::RepeatInfinite) {
        return true;
    }
    return m_runMode == FeatureRunMode::RepeatCount && m_repeatCount >= 2;
}

bool Feature::roiCorrectionSessionEligible() const {
    if (m_runMode == FeatureRunMode::RepeatInfinite || m_runMode == FeatureRunMode::Trigger) {
        return true;
    }
    return m_runMode == FeatureRunMode::RepeatCount && m_repeatCount >= 2;
}

void Feature::setRoiCorrectionExpandPercent(int percent) {
    m_roiCorrectionExpandPercent = snapRoiCorrectionExpandPercent(percent);
}

Feature::Feature()
    : m_id(generateId())
    , m_name("새 기능") {}

Feature::Feature(std::string name)
    : m_id(generateId())
    , m_name(std::move(name)) {}

std::unique_ptr<Feature> Feature::clone() const {
    auto copy = std::make_unique<Feature>();
    copy->m_id = m_id;
    copy->m_name = m_name;
    copy->m_enabled = m_enabled;
    copy->m_runMode = m_runMode;
    copy->m_repeatCount = m_repeatCount;
    copy->m_infiniteExitAfterConsecutiveMisses = m_infiniteExitAfterConsecutiveMisses;
    copy->m_userInputInterruptMode = m_userInputInterruptMode;
    copy->m_pointerVisualFeedback = m_pointerVisualFeedback;
    copy->m_restoreMousePositionOnEnd = m_restoreMousePositionOnEnd;
    copy->m_lockMouseToScreenCenterDuringRun = m_lockMouseToScreenCenterDuringRun;
    copy->m_lockMouseToCurrentPositionDuringRun = m_lockMouseToCurrentPositionDuringRun;
    copy->m_lockMouseDuringFirstLoopCount = m_lockMouseDuringFirstLoopCount;
    copy->m_unlockMouseOnBlockFailureCount = m_unlockMouseOnBlockFailureCount;
    copy->m_roiCorrection = m_roiCorrection;
    copy->m_roiCorrectionExpandPercent = m_roiCorrectionExpandPercent;
    copy->m_editFirstTemplateRoiOnStart = m_editFirstTemplateRoiOnStart;
    copy->m_triggerCooldownMs = m_triggerCooldownMs;
    copy->m_loopIntervalMs = m_loopIntervalMs;
    copy->m_loopIntervalMinMs = m_loopIntervalMinMs;
    copy->m_loopIntervalMaxMs = m_loopIntervalMaxMs;
    copy->m_loopIntervalRandomRange = m_loopIntervalRandomRange;
    copy->m_hotkeyAllowExtraModifiers = m_hotkeyAllowExtraModifiers;
    copy->m_captureTargetScope = m_captureTargetScope;
    copy->m_hotkey = m_hotkey;
    copy->m_workflow.assignFrom(m_workflow);
    return copy;
}

std::unique_ptr<Feature> Feature::duplicateAsNewInstance(bool preserveHotkey) const {
    auto copy = std::make_unique<Feature>();
    copy->m_id = generateId();
    copy->m_name = m_name;
    copy->m_enabled = m_enabled;
    copy->m_runMode = m_runMode;
    copy->m_repeatCount = m_repeatCount;
    copy->m_infiniteExitAfterConsecutiveMisses = m_infiniteExitAfterConsecutiveMisses;
    copy->m_userInputInterruptMode = m_userInputInterruptMode;
    copy->m_pointerVisualFeedback = m_pointerVisualFeedback;
    copy->m_restoreMousePositionOnEnd = m_restoreMousePositionOnEnd;
    copy->m_lockMouseToScreenCenterDuringRun = m_lockMouseToScreenCenterDuringRun;
    copy->m_lockMouseToCurrentPositionDuringRun = m_lockMouseToCurrentPositionDuringRun;
    copy->m_lockMouseDuringFirstLoopCount = m_lockMouseDuringFirstLoopCount;
    copy->m_unlockMouseOnBlockFailureCount = m_unlockMouseOnBlockFailureCount;
    copy->m_roiCorrection = m_roiCorrection;
    copy->m_roiCorrectionExpandPercent = m_roiCorrectionExpandPercent;
    copy->m_editFirstTemplateRoiOnStart = m_editFirstTemplateRoiOnStart;
    copy->m_triggerCooldownMs = m_triggerCooldownMs;
    copy->m_loopIntervalMs = m_loopIntervalMs;
    copy->m_loopIntervalMinMs = m_loopIntervalMinMs;
    copy->m_loopIntervalMaxMs = m_loopIntervalMaxMs;
    copy->m_loopIntervalRandomRange = m_loopIntervalRandomRange;
    copy->m_hotkeyAllowExtraModifiers = m_hotkeyAllowExtraModifiers;
    copy->m_captureTargetScope = m_captureTargetScope;
    copy->m_hotkey = preserveHotkey ? m_hotkey : HotkeyBinding{};
    copy->m_workflow.assignFrom(m_workflow);
    return copy;
}

std::string Feature::generateId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}
