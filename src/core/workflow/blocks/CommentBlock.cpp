#include "core/workflow/blocks/CommentBlock.h"

#include "core/workflow/ExecutionContext.h"

std::string CommentBlock::summary() const {
    return text.empty() ? "주석" : text;
}

BlockResult CommentBlock::execute(ExecutionContext& ctx) {
    BlockResult result;
    result.success = true;
    result.message = summary();
    ctx.log("[주석] " + result.message);
    return result;
}

nlohmann::json CommentBlock::toJson() const {
    return nlohmann::json{
        {"type", "Comment"},
        {"text", text}};
}

std::unique_ptr<Block> CommentBlock::clone() const {
    return std::make_unique<CommentBlock>(*this);
}

std::unique_ptr<CommentBlock> CommentBlock::fromJson(const nlohmann::json& json) {
    auto block = std::make_unique<CommentBlock>();
    block->text = json.value("text", "");
    return block;
}
