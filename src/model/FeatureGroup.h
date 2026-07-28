#pragma once

#include <string>

class FeatureGroup {
public:
    FeatureGroup();
    explicit FeatureGroup(std::string name);

    const std::string& id() const { return m_id; }
    void setId(const std::string& id) { m_id = id; }

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    FeatureGroup clone() const;

private:
    static std::string generateId();

    std::string m_id;
    std::string m_name;
};
