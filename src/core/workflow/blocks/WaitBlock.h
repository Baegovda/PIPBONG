#pragma once

#include "core/workflow/Block.h"

constexpr int kWaitDelayStepMs = 5;

int snapWaitDelayMs(int ms);

class WaitBlock : public Block {
public:
    int ms = 500;
    int minMs = 0;
    int maxMs = 0;
    bool randomRange = false;

    BlockType type() const override { return BlockType::Wait; }
    std::string displayName() const override { return "Wait"; }
    std::string summary() const override;
    BlockResult execute(ExecutionContext& ctx) override;
    nlohmann::json toJson() const override;
    std::unique_ptr<Block> clone() const override;

    static std::unique_ptr<WaitBlock> fromJson(const nlohmann::json& json);
};
