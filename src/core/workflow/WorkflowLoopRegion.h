#pragma once

#include "core/workflow/LoopExitCondition.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct WorkflowLoopRegion {
    std::string id;
    /// Inclusive 0-based block indices in the parent workflow.
    int startIndex = 0;
    int endIndex = 0;
    LoopExitCondition exitCondition = LoopExitCondition::DetectionFailed;
    int detectionMissLimit = 1;
};

nlohmann::json workflowLoopRegionToJson(const WorkflowLoopRegion& region);
WorkflowLoopRegion workflowLoopRegionFromJson(const nlohmann::json& json);
nlohmann::json workflowLoopRegionsToJson(const std::vector<WorkflowLoopRegion>& regions);
std::vector<WorkflowLoopRegion> workflowLoopRegionsFromJson(const nlohmann::json& json);
