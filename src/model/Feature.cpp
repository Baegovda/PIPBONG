#include "model/Feature.h"

#include <QUuid>

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
    copy->m_hotkey = m_hotkey;
    copy->m_workflow.assignFrom(m_workflow);
    return copy;
}

std::unique_ptr<Feature> Feature::duplicateForPaste() const {
    auto copy = clone();
    copy->m_id = generateId();
    copy->m_hotkey = {};
    return copy;
}

std::string Feature::generateId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}
