#include "core/workflow/blocks/IfBlock.h"

#include "core/workflow/ExecutionContext.h"
#include "core/workflow/WorkflowRunner.h"
#include "storage/JsonSerializer.h"

#include <QCoreApplication>
#include <QString>

#include <algorithm>

std::string ifConditionToString(IfCondition condition) {
    switch (condition) {
    case IfCondition::LastMatchSuccess:
        return "LastMatchSuccess";
    case IfCondition::LastMatchFailed:
        return "LastMatchFailed";
    case IfCondition::DetectionFailed:
        return "DetectionFailed";
    }
    return "LastMatchSuccess";
}

IfCondition ifConditionFromString(const std::string& value) {
    if (value == "LastMatchFailed") {
        return IfCondition::LastMatchFailed;
    }
    if (value == "DetectionFailed") {
        return IfCondition::DetectionFailed;
    }
    return IfCondition::LastMatchSuccess;
}

QString ifConditionDisplayLabel(IfCondition condition, bool negate) {
    QString label;
    switch (condition) {
    case IfCondition::LastMatchSuccess:
        label = QCoreApplication::translate("IfBlock", "직전 감지 성공");
        break;
    case IfCondition::LastMatchFailed:
        label = QCoreApplication::translate("IfBlock", "직전 감지 실패");
        break;
    case IfCondition::DetectionFailed:
        label = QCoreApplication::translate("IfBlock", "감지 실패");
        break;
    }
    if (negate) {
        label = QCoreApplication::translate("IfBlock", "반전: %1").arg(label);
    }
    return label;
}

bool IfBlock::evaluateCondition(const ExecutionContext& ctx) const {
    bool value = false;
    switch (condition) {
    case IfCondition::LastMatchSuccess:
        value = ctx.hasLastMatch();
        break;
    case IfCondition::LastMatchFailed:
        value = !ctx.hasLastMatch() || ctx.detectionFailedThisRun();
        break;
    case IfCondition::DetectionFailed:
        value = ctx.detectionFailedThisRun();
        break;
    }
    return negate ? !value : value;
}

std::string IfBlock::summary() const {
    std::string condLabel;
    switch (condition) {
    case IfCondition::LastMatchSuccess:
        condLabel = "직전 감지 성공";
        break;
    case IfCondition::LastMatchFailed:
        condLabel = "직전 감지 실패";
        break;
    case IfCondition::DetectionFailed:
        condLabel = "감지 실패";
        break;
    }
    if (negate) {
        condLabel = "NOT " + condLabel;
    }
    std::string summary = condLabel + " → " + std::to_string(thenBranch.blocks().size()) + "블록 / "
                          + std::to_string(elseBranch.blocks().size()) + "블록";
    if (thenGotoBlockNumber > 0) {
        summary += " · then→#" + std::to_string(thenGotoBlockNumber);
    }
    if (elseGotoBlockNumber > 0) {
        summary += " · else→#" + std::to_string(elseGotoBlockNumber);
    }
    return summary;
}

BlockResult IfBlock::execute(ExecutionContext& ctx) {
    if (!ctx.enterIfScope()) {
        BlockResult result;
        result.success = false;
        result.message = "만약 블록 중첩 깊이 초과";
        ctx.log(result.message);
        return result;
    }

    const bool takeThen = evaluateCondition(ctx);
    const Workflow& branch = takeThen ? thenBranch : elseBranch;
    const char* branchName = takeThen ? "then" : "else";

    ctx.log(std::string("만약: ") + (takeThen ? "참" : "거짓") + " → " + branchName + " ("
            + std::to_string(branch.blocks().size()) + "블록)");

    WorkflowRunResult runResult = WorkflowRunner::run(branch, ctx, nullptr);
    ctx.leaveIfScope();

    BlockResult result;
    result.success = runResult.success;
    result.message = runResult.message;
    if (!result.success && result.message.empty()) {
        result.message = std::string("만약 ") + branchName + " 분기 실패";
    }

    if (result.success) {
        const int gotoNumber = takeThen ? thenGotoBlockNumber : elseGotoBlockNumber;
        if (gotoNumber > 0) {
            result.workflowJumpIndex = gotoNumber - 1;
            ctx.log(std::string("만약: 분기 후 #") + std::to_string(gotoNumber) + " 블록으로 이동");
        }
    }

    return result;
}

nlohmann::json IfBlock::toJson() const {
    nlohmann::json json{{"type", "If"},
                        {"condition", ifConditionToString(condition)},
                        {"negate", negate},
                        {"then", JsonSerializer::workflowToJson(thenBranch)},
                        {"else", JsonSerializer::workflowToJson(elseBranch)}};
    if (thenGotoBlockNumber > 0) {
        json["thenGotoBlock"] = thenGotoBlockNumber;
    }
    if (elseGotoBlockNumber > 0) {
        json["elseGotoBlock"] = elseGotoBlockNumber;
    }
    return json;
}

std::unique_ptr<Block> IfBlock::clone() const {
    auto copy = std::make_unique<IfBlock>();
    copy->condition = condition;
    copy->negate = negate;
    copy->thenBranch.assignFrom(thenBranch);
    copy->elseBranch.assignFrom(elseBranch);
    copy->thenGotoBlockNumber = thenGotoBlockNumber;
    copy->elseGotoBlockNumber = elseGotoBlockNumber;
    return copy;
}

std::unique_ptr<IfBlock> IfBlock::fromJson(const nlohmann::json& json) {
    auto block = std::make_unique<IfBlock>();
    block->condition = ifConditionFromString(json.value("condition", "LastMatchSuccess"));
    block->negate = json.value("negate", false);
    if (json.contains("then")) {
        JsonSerializer::workflowFromJson(json["then"], block->thenBranch);
    }
    if (json.contains("else")) {
        JsonSerializer::workflowFromJson(json["else"], block->elseBranch);
    }
    block->thenGotoBlockNumber = std::max(0, json.value("thenGotoBlock", 0));
    block->elseGotoBlockNumber = std::max(0, json.value("elseGotoBlock", 0));
    return block;
}
