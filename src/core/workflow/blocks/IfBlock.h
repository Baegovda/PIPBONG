#pragma once

#include "core/workflow/Block.h"
#include "core/workflow/Workflow.h"

#include <QString>

enum class IfCondition {
    LastMatchSuccess,
    LastMatchFailed,
    DetectionFailed
};

std::string ifConditionToString(IfCondition condition);
IfCondition ifConditionFromString(const std::string& value);
QString ifConditionDisplayLabel(IfCondition condition, bool negate);

class IfBlock : public Block {
public:
    IfCondition condition = IfCondition::LastMatchSuccess;
    bool negate = false;
    Workflow thenBranch;
    Workflow elseBranch;
    /// 1-based # column in the parent workflow; 0 = no jump after then branch.
    int thenGotoBlockNumber = 0;
    /// 1-based # column in the parent workflow; 0 = no jump after else branch.
    int elseGotoBlockNumber = 0;

    BlockType type() const override { return BlockType::If; }
    std::string displayName() const override { return "If"; }
    std::string summary() const override;
    BlockResult execute(ExecutionContext& ctx) override;
    nlohmann::json toJson() const override;
    std::unique_ptr<Block> clone() const override;

    static std::unique_ptr<IfBlock> fromJson(const nlohmann::json& json);

    static constexpr int kMaxNestingDepth = 8;

    bool evaluateCondition(const ExecutionContext& ctx) const;
};
