#include "core/workflow/BlockFactory.h"

#include "core/workflow/blocks/ClickBlock.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/blocks/KeyPressBlock.h"
#include "core/workflow/blocks/WaitBlock.h"

std::string blockTypeToString(BlockType type) {
    switch (type) {
    case BlockType::ImageFind:
        return "ImageFind";
    case BlockType::Click:
        return "Click";
    case BlockType::KeyPress:
        return "KeyPress";
    case BlockType::Wait:
        return "Wait";
    }
    return "Unknown";
}

BlockType blockTypeFromString(const std::string& value) {
    if (value == "ImageFind") {
        return BlockType::ImageFind;
    }
    if (value == "Click") {
        return BlockType::Click;
    }
    if (value == "KeyPress") {
        return BlockType::KeyPress;
    }
    if (value == "Wait") {
        return BlockType::Wait;
    }
    return BlockType::Wait;
}

std::unique_ptr<Block> BlockFactory::create(BlockType type) {
    switch (type) {
    case BlockType::ImageFind:
        return std::make_unique<ImageFindBlock>();
    case BlockType::Click:
        return std::make_unique<ClickBlock>();
    case BlockType::KeyPress:
        return std::make_unique<KeyPressBlock>();
    case BlockType::Wait:
        return std::make_unique<WaitBlock>();
    }
    return std::make_unique<WaitBlock>();
}

std::unique_ptr<Block> BlockFactory::fromJson(const nlohmann::json& json) {
    const std::string type = json.value("type", "");
    if (type != "ImageFind" && type != "Click" && type != "KeyPress" && type != "Wait") {
        return nullptr;
    }
    switch (blockTypeFromString(type)) {
    case BlockType::ImageFind:
        return ImageFindBlock::fromJson(json);
    case BlockType::Click:
        return ClickBlock::fromJson(json);
    case BlockType::KeyPress:
        return KeyPressBlock::fromJson(json);
    case BlockType::Wait:
        return WaitBlock::fromJson(json);
    }
    return nullptr;
}
