#pragma once

#include "core/workflow/Block.h"
#include "core/input/InputSimulator.h"

class ClickBlock : public Block {
public:
    enum class ClickTarget {
        Fixed,
        LastMatch,
        CurrentPosition
    };

    ClickTarget target = ClickTarget::LastMatch;
    int x = 0;
    int y = 0;
    bool lastMatchRelativeOffset = false;
    MouseButton button = MouseButton::Left;
    ClickAction action = ClickAction::Tap;
    int count = 1;
    bool useClientCoordinates = true;
    KeyModifiers modifiers;

    BlockType type() const override { return BlockType::Click; }
    std::string displayName() const override { return "Click"; }
    std::string summary() const override;
    BlockResult execute(ExecutionContext& ctx) override;
    nlohmann::json toJson() const override;
    std::unique_ptr<Block> clone() const override;

    static std::unique_ptr<ClickBlock> fromJson(const nlohmann::json& json);
};
