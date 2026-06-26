#pragma once

#include "core/workflow/Block.h"

#include <string>

class CommentBlock : public Block {
public:
    std::string text;

    BlockType type() const override { return BlockType::Comment; }
    std::string displayName() const override { return "Comment"; }
    std::string summary() const override;
    BlockResult execute(ExecutionContext& ctx) override;
    nlohmann::json toJson() const override;
    std::unique_ptr<Block> clone() const override;

    static std::unique_ptr<CommentBlock> fromJson(const nlohmann::json& json);
};
