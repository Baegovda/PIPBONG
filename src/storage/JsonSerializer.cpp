#include "storage/JsonSerializer.h"

#include "core/workflow/BlockFactory.h"
#include "core/workflow/WorkflowLoopRegion.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "model/FeatureRunMode.h"
#include "model/UserInputInterruptMode.h"
#include "model/Feature.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

#include <fstream>

namespace {

nlohmann::json featureToJsonImpl(const Feature& feature) {
    nlohmann::json blocks = nlohmann::json::array();
    for (const auto& block : feature.workflow().blocks()) {
        blocks.push_back(block->toJson());
    }

    nlohmann::json workflowJson = blocks;
    if (!feature.workflow().loopRegions().empty()) {
        workflowJson = nlohmann::json{{"blocks", blocks},
                                      {"loopRegions",
                                       workflowLoopRegionsToJson(feature.workflow().loopRegions())}};
    }

    nlohmann::json json{
        {"id", feature.id()},
        {"name", feature.name()},
        {"enabled", feature.enabled()},
        {"runMode", featureRunModeToString(feature.runMode())},
        {"repeatCount", feature.repeatCount()},
        {"workflow", workflowJson}};
    if (feature.infiniteExitAfterConsecutiveMisses() > 0) {
        json["infiniteExitAfterConsecutiveMisses"] = feature.infiniteExitAfterConsecutiveMisses();
    }
    if (feature.userInputInterruptMode() != UserInputInterruptMode::Stop) {
        json["userInputInterrupt"] = userInputInterruptModeToString(feature.userInputInterruptMode());
    }
    if (!feature.pointerVisualFeedback()) {
        json["pointerVisualFeedback"] = false;
    }
    if (feature.restoreMousePositionOnEnd()) {
        json["restoreMousePositionOnEnd"] = true;
    }
    if (feature.lockMouseToScreenCenterDuringRun()) {
        json["lockMouseToScreenCenterDuringRun"] = true;
    }
    if (feature.lockMouseToCurrentPositionDuringRun()) {
        json["lockMouseToCurrentPositionDuringRun"] = true;
    }
    if (feature.roiCorrection()) {
        json["roiCorrection"] = true;
    }
    if (feature.roiCorrectionExpandPercent() != kDefaultRoiCorrectionExpandPercent) {
        json["roiCorrectionExpandPercent"] = feature.roiCorrectionExpandPercent();
    }
    if (feature.editFirstTemplateRoiOnStart()) {
        json["editFirstTemplateRoiOnStart"] = true;
    }
    if (feature.triggerCooldownMs() != kDefaultTriggerCooldownMs) {
        json["triggerCooldownMs"] = feature.triggerCooldownMs();
    }
    if (feature.loopIntervalRandomRange()) {
        json["loopIntervalRandomRange"] = true;
        json["loopIntervalMinMs"] = feature.loopIntervalMinMs();
        json["loopIntervalMaxMs"] = feature.loopIntervalMaxMs();
    } else if (feature.loopIntervalMs() > 0) {
        json["loopIntervalMs"] = feature.loopIntervalMs();
    }
    if (!feature.hotkey().isEmpty()) {
        json["hotkey"] = feature.hotkey().toJson();
    }
    if (feature.hotkeyAllowExtraModifiers()) {
        json["hotkeyAllowExtraModifiers"] = true;
    }
    return json;
}

void featureFromJsonImpl(const nlohmann::json& json, Feature& feature) {
    if (json.contains("id")) {
        feature.setId(json.value("id", feature.id()));
    }
    feature.setName(json.value("name", "새 기능"));
    feature.setEnabled(json.value("enabled", true));
    feature.setRunMode(featureRunModeFromString(json.value("runMode", "RepeatCount")));
    feature.setRepeatCount(json.value("repeatCount", 1));
    feature.setInfiniteExitAfterConsecutiveMisses(json.value("infiniteExitAfterConsecutiveMisses", 0));
    feature.setUserInputInterruptMode(
        userInputInterruptModeFromString(json.value("userInputInterrupt", "Stop")));
    feature.setPointerVisualFeedback(json.value("pointerVisualFeedback", true));
    feature.setRestoreMousePositionOnEnd(json.value("restoreMousePositionOnEnd", false));
    feature.setLockMouseToScreenCenterDuringRun(json.value("lockMouseToScreenCenterDuringRun", false));
    feature.setLockMouseToCurrentPositionDuringRun(
        json.value("lockMouseToCurrentPositionDuringRun", false));
    feature.setRoiCorrection(json.value("roiCorrection", false));
    feature.setRoiCorrectionExpandPercent(
        json.value("roiCorrectionExpandPercent", kDefaultRoiCorrectionExpandPercent));
    feature.setEditFirstTemplateRoiOnStart(json.value("editFirstTemplateRoiOnStart", false));
    feature.setTriggerCooldownMs(
        snapTriggerCooldownMs(json.value("triggerCooldownMs", kDefaultTriggerCooldownMs)));
    feature.setLoopIntervalRandomRange(json.value("loopIntervalRandomRange", false));
    feature.setLoopIntervalMs(json.value("loopIntervalMs", 0));
    feature.setLoopIntervalMinMs(json.value("loopIntervalMinMs", 0));
    feature.setLoopIntervalMaxMs(json.value("loopIntervalMaxMs", 0));
    if (json.contains("hotkey")) {
        feature.setHotkey(HotkeyBinding::fromJson(json["hotkey"]));
    } else {
        feature.setHotkey({});
    }
    feature.setHotkeyAllowExtraModifiers(json.value("hotkeyAllowExtraModifiers", false));
    feature.workflow().clear();
    if (json.contains("workflow")) {
        JsonSerializer::workflowFromJson(json["workflow"], feature.workflow());
    }
}

} // namespace

nlohmann::json JsonSerializer::featureToJson(const Feature& feature) {
    return featureToJsonImpl(feature);
}

void JsonSerializer::featureFromJson(const nlohmann::json& json, Feature& feature) {
    featureFromJsonImpl(json, feature);
}

bool JsonSerializer::saveToFile(const Project& project,
                               const QString& filePath,
                               const QString& projectDirectory) {
    nlohmann::json features = nlohmann::json::array();
    for (const auto& feature : project.features()) {
        features.push_back(featureToJsonImpl(*feature));
    }

    nlohmann::json root{
        {"version", project.version()},
        {"targetWindowTitle", project.targetWindowTitle()},
        {"projectDirectory", projectDirectory.toStdString()},
        {"features", features}};

    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QDir().mkpath(projectDirectory);

    std::ofstream out(filePath.toStdString(), std::ios::binary);
    if (!out.is_open()) {
        return false;
    }
    out << root.dump(2);
    return true;
}

std::unique_ptr<Project> JsonSerializer::loadFromFile(const QString& filePath,
                                                      QString* projectDirectoryOut) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return nullptr;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(file.readAll().constData());
    } catch (...) {
        return nullptr;
    }

    auto project = std::make_unique<Project>();
    project->setTargetWindowTitle(root.value("targetWindowTitle", ""));

    QString projectDirectory;
    if (root.contains("projectDirectory")) {
        projectDirectory = QString::fromStdString(root.value("projectDirectory", ""));
    }
    if (projectDirectory.isEmpty()) {
        projectDirectory = QFileInfo(filePath).absolutePath();
    }
    if (projectDirectoryOut) {
        *projectDirectoryOut = projectDirectory;
    }

    if (root.contains("features") && root["features"].is_array()) {
        for (const auto& featureJson : root["features"]) {
            auto feature = std::make_unique<Feature>();
            featureFromJsonImpl(featureJson, *feature);
            project->features().push_back(std::move(feature));
        }
    }

    return project;
}

nlohmann::json JsonSerializer::workflowToJson(const Workflow& workflow) {
    nlohmann::json blocks = nlohmann::json::array();
    for (const auto& block : workflow.blocks()) {
        blocks.push_back(block->toJson());
    }
    return blocks;
}

void JsonSerializer::workflowFromJson(const nlohmann::json& json, Workflow& workflow) {
    workflow.clear();
    if (json.is_array()) {
        for (const auto& blockJson : json) {
            auto block = BlockFactory::fromJson(blockJson);
            if (block) {
                workflow.addBlock(std::move(block));
            }
        }
        return;
    }
    if (!json.is_object()) {
        return;
    }
    if (json.contains("blocks") && json["blocks"].is_array()) {
        for (const auto& blockJson : json["blocks"]) {
            auto block = BlockFactory::fromJson(blockJson);
            if (block) {
                workflow.addBlock(std::move(block));
            }
        }
    }
    if (json.contains("loopRegions")) {
        workflow.setLoopRegions(workflowLoopRegionsFromJson(json["loopRegions"]));
    }
}
