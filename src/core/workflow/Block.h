#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>
#include <memory>
#include <string>

enum class BlockType {
    ImageFind,
    Click,
    KeyPress,
    Wait,
    Text
};

struct BlockResult {
    bool success = true;
    std::string message;
    /// Active capture + template-match work in ImageFind blocks (excludes poll sleep). -1 if N/A.
    int64_t imageFindMatchDurationMs = -1;
    /// ImageFind poll iterations in the last execute(); -1 if N/A.
    int imageFindPollAttempts = -1;
};

std::string blockTypeToString(BlockType type);
BlockType blockTypeFromString(const std::string& value);

class ExecutionContext;

class Block {
public:
    virtual ~Block() = default;

    virtual BlockType type() const = 0;
    virtual std::string displayName() const = 0;
    virtual std::string summary() const = 0;
    virtual BlockResult execute(ExecutionContext& ctx) = 0;
    virtual nlohmann::json toJson() const = 0;
    virtual std::unique_ptr<Block> clone() const = 0;
};
