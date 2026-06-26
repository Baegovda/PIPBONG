#pragma once

#include "model/Project.h"

#include <nlohmann/json.hpp>
#include <QString>
#include <memory>

class JsonSerializer {
public:
    static bool saveToFile(const Project& project, const QString& filePath, const QString& projectDirectory);
    static std::unique_ptr<Project> loadFromFile(const QString& filePath, QString* projectDirectoryOut = nullptr);

    static nlohmann::json workflowToJson(const Workflow& workflow);
    static void workflowFromJson(const nlohmann::json& json, Workflow& workflow);
};
