#pragma once

#include "core/input/HotkeyBinding.h"
#include "core/workflow/Workflow.h"
#include "model/FeatureRunMode.h"
#include "model/UserInputInterruptMode.h"

#include <memory>
#include <string>

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

    bool roiCorrection() const { return m_roiCorrection; }
    void setRoiCorrection(bool enabled) { m_roiCorrection = enabled; }

    /// True when run mode supports ROI correction (infinite repeat or N≥2).
    bool roiCorrectionSessionEligible() const;

    Workflow& workflow() { return m_workflow; }
    const Workflow& workflow() const { return m_workflow; }

    const HotkeyBinding& hotkey() const { return m_hotkey; }
    void setHotkey(const HotkeyBinding& hotkey) { m_hotkey = hotkey; }

    std::unique_ptr<Feature> clone() const;
    std::unique_ptr<Feature> duplicateAsNewInstance() const;

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
    bool m_roiCorrection = false;
    HotkeyBinding m_hotkey;
    Workflow m_workflow;
};
