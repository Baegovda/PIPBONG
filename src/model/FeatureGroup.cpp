#include "model/FeatureGroup.h"

#include <QUuid>

FeatureGroup::FeatureGroup() : m_id(generateId()), m_name("그룹") {}

FeatureGroup::FeatureGroup(std::string name) : m_id(generateId()), m_name(std::move(name)) {}

FeatureGroup FeatureGroup::clone() const {
    FeatureGroup copy;
    copy.m_id = m_id;
    copy.m_name = m_name;
    return copy;
}

std::string FeatureGroup::generateId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}
