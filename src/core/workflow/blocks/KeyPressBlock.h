#pragma once

#include "core/workflow/Block.h"
#include "core/input/InputSimulator.h"

#include <string>

class KeyPressBlock : public Block {
public:
    int virtualKey = 0x20; // VK_SPACE
    KeyAction action = KeyAction::Tap;
    bool useMainKey = true;
    KeyPressModifierActions modifierActions;

    BlockType type() const override { return BlockType::KeyPress; }
    std::string displayName() const override { return "KeyPress"; }
    std::string summary() const override;
    std::string listDetailSummary() const override;
    BlockResult execute(ExecutionContext& ctx) override;
    nlohmann::json toJson() const override;
    std::unique_ptr<Block> clone() const override;

    static std::unique_ptr<KeyPressBlock> fromJson(const nlohmann::json& json);
};
