#pragma once

#include "model/Feature.h"

#include <memory>
#include <string>
#include <vector>

class Project {
public:
    Project();

    int version() const { return m_version; }

    const std::string& targetWindowTitle() const { return m_targetWindowTitle; }
    void setTargetWindowTitle(const std::string& title) { m_targetWindowTitle = title; }

    const std::vector<std::unique_ptr<Feature>>& features() const { return m_features; }
    std::vector<std::unique_ptr<Feature>>& features() { return m_features; }

    Feature* featureAt(int index);
    const Feature* featureAt(int index) const;
    Feature* featureById(const std::string& id);
    const Feature* featureById(const std::string& id) const;

    Feature* addFeature(const std::string& name);
    Feature* insertFeature(int index, std::unique_ptr<Feature> feature);
    void removeFeature(int index);
    void moveFeature(int fromIndex, int toIndex);
    void clear();

    std::unique_ptr<Project> clone() const;

private:
    int m_version = 1;
    std::string m_targetWindowTitle;
    std::vector<std::unique_ptr<Feature>> m_features;
};
