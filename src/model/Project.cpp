#include "model/Project.h"

#include <algorithm>

Project::Project() = default;

Feature* Project::featureAt(int index) {
    if (index < 0 || index >= static_cast<int>(m_features.size())) {
        return nullptr;
    }
    return m_features[index].get();
}

const Feature* Project::featureAt(int index) const {
    if (index < 0 || index >= static_cast<int>(m_features.size())) {
        return nullptr;
    }
    return m_features[index].get();
}

Feature* Project::featureById(const std::string& id) {
    for (auto& feature : m_features) {
        if (feature->id() == id) {
            return feature.get();
        }
    }
    return nullptr;
}

const Feature* Project::featureById(const std::string& id) const {
    for (const auto& feature : m_features) {
        if (feature->id() == id) {
            return feature.get();
        }
    }
    return nullptr;
}

Feature* Project::addFeature(const std::string& name) {
    auto feature = std::make_unique<Feature>(name);
    Feature* ptr = feature.get();
    m_features.push_back(std::move(feature));
    return ptr;
}

Feature* Project::insertFeature(int index, std::unique_ptr<Feature> feature) {
    if (!feature) {
        return nullptr;
    }
    const int size = static_cast<int>(m_features.size());
    index = std::clamp(index, 0, size);
    Feature* ptr = feature.get();
    m_features.insert(m_features.begin() + index, std::move(feature));
    return ptr;
}

void Project::removeFeature(int index) {
    if (index < 0 || index >= static_cast<int>(m_features.size())) {
        return;
    }
    m_features.erase(m_features.begin() + index);
}

void Project::moveFeature(int fromIndex, int toIndex) {
    const int size = static_cast<int>(m_features.size());
    if (fromIndex < 0 || fromIndex >= size || toIndex < 0 || toIndex >= size || fromIndex == toIndex) {
        return;
    }
    if (fromIndex < toIndex) {
        std::rotate(m_features.begin() + fromIndex,
                    m_features.begin() + fromIndex + 1,
                    m_features.begin() + toIndex + 1);
    } else {
        std::rotate(m_features.begin() + toIndex,
                    m_features.begin() + fromIndex,
                    m_features.begin() + fromIndex + 1);
    }
}

void Project::clear() {
    m_features.clear();
}

std::unique_ptr<Project> Project::clone() const {
    auto copy = std::make_unique<Project>();
    copy->m_version = m_version;
    copy->m_targetWindowTitle = m_targetWindowTitle;
    for (const auto& feature : m_features) {
        auto cloned = feature->clone();
        copy->m_features.push_back(std::move(cloned));
    }
    return copy;
}
