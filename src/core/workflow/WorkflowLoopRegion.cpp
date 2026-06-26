#include "core/workflow/WorkflowLoopRegion.h"

#include <QUuid>

#include <algorithm>

nlohmann::json workflowLoopRegionToJson(const WorkflowLoopRegion& region) {
    nlohmann::json json{{"id", region.id},
                        {"startBlock", region.startIndex + 1},
                        {"endBlock", region.endIndex + 1},
                        {"exitCondition", loopExitConditionToString(region.exitCondition)}};
    if (region.exitCondition == LoopExitCondition::DetectionFailed && region.detectionMissLimit != 1) {
        json["detectionMissLimit"] = region.detectionMissLimit;
    }
    return json;
}

WorkflowLoopRegion workflowLoopRegionFromJson(const nlohmann::json& json) {
    WorkflowLoopRegion region;
    region.id = json.value("id", "");
    const int startBlock = json.value("startBlock", 1);
    const int endBlock = json.value("endBlock", startBlock);
    region.startIndex = std::max(0, startBlock - 1);
    region.endIndex = std::max(region.startIndex, endBlock - 1);
    region.exitCondition =
        loopExitConditionFromString(json.value("exitCondition", "DetectionFailed"));
    region.detectionMissLimit = std::max(1, json.value("detectionMissLimit", 1));
    if (region.id.empty()) {
        region.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    }
    return region;
}

nlohmann::json workflowLoopRegionsToJson(const std::vector<WorkflowLoopRegion>& regions) {
    nlohmann::json array = nlohmann::json::array();
    for (const WorkflowLoopRegion& region : regions) {
        array.push_back(workflowLoopRegionToJson(region));
    }
    return array;
}

std::vector<WorkflowLoopRegion> workflowLoopRegionsFromJson(const nlohmann::json& json) {
    std::vector<WorkflowLoopRegion> regions;
    if (!json.is_array()) {
        return regions;
    }
    for (const auto& entry : json) {
        regions.push_back(workflowLoopRegionFromJson(entry));
    }
    return regions;
}
