#include "core/workflow/blocks/WaitBlock.h"

#include "core/workflow/ExecutionContext.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <thread>

int snapWaitDelayMs(int ms) {
    ms = std::max(0, ms);
    return ((ms + kWaitDelayStepMs / 2) / kWaitDelayStepMs) * kWaitDelayStepMs;
}

std::string WaitBlock::summary() const {
    if (randomRange) {
        return std::to_string(minMs) + "-" + std::to_string(maxMs) + "ms";
    }
    return std::to_string(ms) + "ms";
}

BlockResult WaitBlock::execute(ExecutionContext& ctx) {
    int delay = ms;
    if (randomRange && maxMs >= minMs) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(minMs, maxMs);
        delay = dist(gen);
    }

    const auto step = std::chrono::milliseconds(50);
    auto remaining = std::chrono::milliseconds(delay);
    while (remaining.count() > 0) {
        if (ctx.shouldStop()) {
            BlockResult result;
            result.success = false;
            result.message = "대기 중 중지됨";
            return result;
        }
        if (!ctx.waitWhilePaused()) {
            BlockResult result;
            result.success = false;
            result.message = "대기 중 중지됨";
            return result;
        }
        const auto sleepFor = remaining > step ? step : remaining;
        std::this_thread::sleep_for(sleepFor);
        remaining -= sleepFor;
    }

    BlockResult result;
    result.success = true;
    result.message = std::to_string(delay) + "ms 대기";
    return result;
}

nlohmann::json WaitBlock::toJson() const {
    return nlohmann::json{
        {"type", "Wait"},
        {"ms", ms},
        {"minMs", minMs},
        {"maxMs", maxMs},
        {"randomRange", randomRange}};
}

std::unique_ptr<Block> WaitBlock::clone() const {
    return std::make_unique<WaitBlock>(*this);
}

std::unique_ptr<WaitBlock> WaitBlock::fromJson(const nlohmann::json& json) {
    auto block = std::make_unique<WaitBlock>();
    block->ms = snapWaitDelayMs(json.value("ms", 500));
    block->minMs = snapWaitDelayMs(json.value("minMs", 0));
    block->maxMs = snapWaitDelayMs(json.value("maxMs", 0));
    block->randomRange = json.value("randomRange", false);
    return block;
}
