#pragma once

#include "core/workflow/Block.h"
#include "core/workflow/LoopExitCondition.h"
#include "core/workflow/Workflow.h"

#include <nlohmann/json.hpp>
#include <memory>
#include <string>

class LoopBlock : public Block {
public:
    LoopExitCondition exitCondition = LoopExitCondition::DetectionFailed;
    /// Poll misses before ImageFind fails inside the loop body (DetectionFailed exit only).
    int detectionMissLimit = 1;
    Workflow body;

    BlockType type() const override { return BlockType::Loop; }
    std::string displayName() const override { return "Loop"; }
    std::string summary() const override;
    BlockResult execute(ExecutionContext& ctx) override;
    nlohmann::json toJson() const override;
    std::unique_ptr<Block> clone() const override;

    static std::unique_ptr<LoopBlock> fromJson(const nlohmann::json& json);

    static constexpr int kMaxNestingDepth = 8;

    bool shouldExitAfterIteration(const ExecutionContext& ctx, bool bodySucceeded) const;
};
