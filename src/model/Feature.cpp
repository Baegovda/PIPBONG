#include "model/Feature.h"

#include <QUuid>

bool Feature::roiCorrectionSessionEligible() const {
    if (m_runMode == FeatureRunMode::RepeatInfinite) {
        return true;
    }
    return m_runMode == FeatureRunMode::RepeatCount && m_repeatCount >= 2;
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
    copy->m_roiCorrection = m_roiCorrection;
    copy->m_hotkey = m_hotkey;
    copy->m_workflow.assignFrom(m_workflow);
    return copy;
}

std::unique_ptr<Feature> Feature::duplicateAsNewInstance() const {
    auto copy = std::make_unique<Feature>();
    copy->m_id = generateId();
    copy->m_name = m_name;
    copy->m_enabled = m_enabled;
    copy->m_runMode = m_runMode;
    copy->m_repeatCount = m_repeatCount;
    copy->m_infiniteExitAfterConsecutiveMisses = m_infiniteExitAfterConsecutiveMisses;
    copy->m_userInputInterruptMode = m_userInputInterruptMode;
    copy->m_pointerVisualFeedback = m_pointerVisualFeedback;
    copy->m_roiCorrection = m_roiCorrection;
    copy->m_hotkey = {};
    copy->m_workflow.assignFrom(m_workflow);
    return copy;
}

std::string Feature::generateId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}
