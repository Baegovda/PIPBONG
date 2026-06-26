#pragma once

#include "core/workflow/Block.h"

#include <memory>
#include <nlohmann/json.hpp>

class BlockFactory {
public:
    static std::unique_ptr<Block> create(BlockType type);
    static std::unique_ptr<Block> fromJson(const nlohmann::json& json);
};
