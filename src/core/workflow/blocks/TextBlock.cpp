#include "core/workflow/blocks/TextBlock.h"

#include "core/input/InputSimulator.h"
#include "core/workflow/ExecutionContext.h"

#include <QString>

namespace {

std::string collapseTextForSummary(const std::string& value) {
    std::string collapsed;
    collapsed.reserve(value.size());
    bool pendingSpace = false;
    for (unsigned char ch : value) {
        if (ch == '\r' || ch == '\n' || ch == '\t') {
            pendingSpace = !collapsed.empty();
            continue;
        }
        if (ch == ' ') {
            pendingSpace = !collapsed.empty();
            continue;
        }
        if (pendingSpace) {
            collapsed.push_back(' ');
            pendingSpace = false;
        }
        collapsed.push_back(static_cast<char>(ch));
    }
    constexpr std::size_t kMaxSummaryChars = 120;
    if (collapsed.size() > kMaxSummaryChars) {
        collapsed.resize(kMaxSummaryChars - 3);
        collapsed.append("...");
    }
    return collapsed;
}

#ifdef _WIN32
bool typeConfiguredText(const QString& text, ExecutionContext& ctx) {
    for (int i = 0; i < text.size(); ++i) {
        if (ctx.shouldStop() || !ctx.waitWhilePaused()) {
            return false;
        }

        const QChar ch = text.at(i);
        if (ch == QLatin1Char('\r')) {
            if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('\n')) {
                continue;
            }
            InputSimulator::sendKey(VK_RETURN, KeyAction::Tap);
            continue;
        }
        if (ch == QLatin1Char('\n')) {
            InputSimulator::sendKey(VK_RETURN, KeyAction::Tap);
            continue;
        }
        if (ch == QLatin1Char('\t')) {
            InputSimulator::sendKey(VK_TAB, KeyAction::Tap);
            continue;
        }

        const wchar_t wchar = ch.unicode();
        InputSimulator::sendText(std::wstring(1, wchar));
    }
    return true;
}
#endif

} // namespace

std::string TextBlock::summary() const {
    const std::string collapsed = collapseTextForSummary(text);
    return collapsed.empty() ? std::string{} : collapsed;
}

BlockResult TextBlock::execute(ExecutionContext& ctx) {
    BlockResult result;
    if (text.empty()) {
        result.success = true;
        result.message = "입력할 텍스트가 없습니다";
        ctx.log(result.message);
        return result;
    }

    if (ctx.shouldStop()) {
        result.success = false;
        result.message = "텍스트 입력 중 중지됨";
        return result;
    }
    if (!ctx.waitWhilePaused()) {
        result.success = false;
        result.message = "텍스트 입력 중 중지됨";
        return result;
    }

#ifdef _WIN32
    const QString configuredText = QString::fromUtf8(text.c_str(), static_cast<int>(text.size()));
    if (!typeConfiguredText(configuredText, ctx)) {
        result.success = false;
        result.message = "텍스트 입력 중 중지됨";
        return result;
    }
#else
    (void)ctx;
#endif

    const std::string summaryText = collapseTextForSummary(text);
    result.success = true;
    result.message = "텍스트 입력 · " + summaryText;
    ctx.log(result.message);
    return result;
}

nlohmann::json TextBlock::toJson() const {
    nlohmann::json json = {{"type", "Text"}};
    if (!text.empty()) {
        json["text"] = text;
    }
    return json;
}

std::unique_ptr<Block> TextBlock::clone() const {
    return std::make_unique<TextBlock>(*this);
}

std::unique_ptr<TextBlock> TextBlock::fromJson(const nlohmann::json& json) {
    auto block = std::make_unique<TextBlock>();
    if (json.contains("text")) {
        block->text = json.value("text", std::string{});
    } else {
        block->text = json.value("message", std::string{});
    }
    return block;
}
