#include "core/workflow/blocks/ClickBlock.h"

#include "core/input/InputSimulator.h"
#include "core/workflow/ExecutionContext.h"

namespace {

std::string mouseButtonDisplayName(MouseButton button) {
    switch (button) {
    case MouseButton::Right:
        return "오른쪽";
    case MouseButton::Middle:
        return "휠 클릭";
    case MouseButton::Back:
        return "뒤로 가기";
    case MouseButton::Forward:
        return "앞으로 가기";
    case MouseButton::WheelUp:
        return "휠 올림";
    case MouseButton::WheelDown:
        return "휠 내림";
    case MouseButton::Left:
    default:
        return "왼쪽";
    }
}

std::string mouseButtonToString(MouseButton button) {
    switch (button) {
    case MouseButton::Left:
        return "Left";
    case MouseButton::Right:
        return "Right";
    case MouseButton::Middle:
        return "Middle";
    case MouseButton::Back:
        return "Back";
    case MouseButton::Forward:
        return "Forward";
    case MouseButton::WheelUp:
        return "WheelUp";
    case MouseButton::WheelDown:
        return "WheelDown";
    }
    return "Left";
}

MouseButton mouseButtonFromString(const std::string& value) {
    if (value == "Right") {
        return MouseButton::Right;
    }
    if (value == "Middle") {
        return MouseButton::Middle;
    }
    if (value == "Back" || value == "XButton1") {
        return MouseButton::Back;
    }
    if (value == "Forward" || value == "XButton2") {
        return MouseButton::Forward;
    }
    if (value == "WheelUp") {
        return MouseButton::WheelUp;
    }
    if (value == "WheelDown") {
        return MouseButton::WheelDown;
    }
    return MouseButton::Left;
}

std::string clickActionDisplayName(ClickAction action) {
    switch (action) {
    case ClickAction::MoveOnly:
        return "이동만";
    case ClickAction::Down:
        return "누름";
    case ClickAction::Up:
        return "뗌";
    case ClickAction::Tap:
    default:
        return "클릭";
    }
}

std::string clickButtonActionSummary(MouseButton button, ClickAction action) {
    if (action == ClickAction::MoveOnly) {
        return "이동만";
    }
    if (button == MouseButton::WheelUp) {
        return "휠 올림";
    }
    if (button == MouseButton::WheelDown) {
        return "휠 내림";
    }
    if (button == MouseButton::Middle) {
        switch (action) {
        case ClickAction::Down:
            return "휠 누름";
        case ClickAction::Up:
            return "휠 뗌";
        case ClickAction::Tap:
        default:
            return "휠 클릭";
        }
    }
    if (button == MouseButton::Back) {
        switch (action) {
        case ClickAction::Down:
            return "뒤로 가기 누름";
        case ClickAction::Up:
            return "뒤로 가기 뗌";
        case ClickAction::Tap:
        default:
            return "뒤로 가기 클릭";
        }
    }
    if (button == MouseButton::Forward) {
        switch (action) {
        case ClickAction::Down:
            return "앞으로 가기 누름";
        case ClickAction::Up:
            return "앞으로 가기 뗌";
        case ClickAction::Tap:
        default:
            return "앞으로 가기 클릭";
        }
    }
    return mouseButtonDisplayName(button) + " " + clickActionDisplayName(action);
}

std::string clickActionToString(ClickAction action) {
    switch (action) {
    case ClickAction::MoveOnly:
        return "MoveOnly";
    case ClickAction::Down:
        return "Down";
    case ClickAction::Up:
        return "Up";
    case ClickAction::Tap:
    default:
        return "Tap";
    }
}

ClickAction clickActionFromString(const std::string& value) {
    if (value == "MoveOnly") {
        return ClickAction::MoveOnly;
    }
    if (value == "Down") return ClickAction::Down;
    if (value == "Up") return ClickAction::Up;
    return ClickAction::Tap;
}

std::string clickTargetToString(ClickBlock::ClickTarget target) {
    switch (target) {
    case ClickBlock::ClickTarget::Fixed:
        return "Fixed";
    case ClickBlock::ClickTarget::CurrentPosition:
        return "CurrentPosition";
    case ClickBlock::ClickTarget::LastMatch:
    default:
        return "LastMatch";
    }
}

ClickBlock::ClickTarget clickTargetFromString(const std::string& value) {
    if (value == "Fixed") {
        return ClickBlock::ClickTarget::Fixed;
    }
    if (value == "CurrentPosition") {
        return ClickBlock::ClickTarget::CurrentPosition;
    }
    return ClickBlock::ClickTarget::LastMatch;
}

std::string modifierPrefix(const KeyModifiers& mods) {
    std::string prefix;
    if (mods.ctrl) {
        prefix += "Ctrl+";
    }
    if (mods.alt) {
        prefix += "Alt+";
    }
    if (mods.shift) {
        prefix += "Shift+";
    }
    return prefix;
}

#ifdef _WIN32
bool clientPointForPointerFeedback(ExecutionContext& ctx,
                                   int pointX,
                                   int pointY,
                                   bool useClientCoordinates,
                                   int& clientX,
                                   int& clientY) {
    HWND hwnd = ctx.targetWindow();
    if (!hwnd) {
        return false;
    }
    if (useClientCoordinates) {
        clientX = pointX;
        clientY = pointY;
        return true;
    }
    int screenX = pointX;
    int screenY = pointY;
    return InputSimulator::screenToClient(hwnd, screenX, screenY, clientX, clientY);
}
#endif

} // namespace

std::string ClickBlock::summary() const {
    const std::string actionText = clickButtonActionSummary(button, action);
    const std::string modPrefix = modifierPrefix(modifiers);
    if (target == ClickTarget::LastMatch) {
        std::string text = "직전 매칭";
        if (lastMatchRelativeOffset && (x != 0 || y != 0)) {
            text += " +" + std::to_string(x) + "," + std::to_string(y);
        }
        return modPrefix + text + " " + actionText;
    }
    if (target == ClickTarget::CurrentPosition) {
        return modPrefix + std::string("현재 위치 ") + actionText;
    }
    return modPrefix + std::to_string(x) + "," + std::to_string(y) + " " + actionText;
}

BlockResult ClickBlock::execute(ExecutionContext& ctx) {
    int clickX = x;
    int clickY = y;

    if (target == ClickTarget::LastMatch) {
        if (!ctx.hasLastMatch()) {
            BlockResult result;
            result.success = false;
            result.message = "먼저 화면 찾기 단계가 성공해야 합니다";
            return result;
        }
        clickX = ctx.lastMatchPoint().x;
        clickY = ctx.lastMatchPoint().y;
        if (lastMatchRelativeOffset) {
            clickX += x;
            clickY += y;
        }
    } else if (target == ClickTarget::CurrentPosition) {
        // Coordinates resolved after click only when pointer feedback is enabled.
    }

#ifdef _WIN32
    if (action == ClickAction::MoveOnly) {
        HWND hwnd = ctx.targetWindow();
        if (useClientCoordinates && hwnd) {
            InputSimulator::moveAtClient(hwnd, clickX, clickY);
        } else {
            InputSimulator::moveAt(clickX, clickY);
        }
    } else if (target == ClickTarget::CurrentPosition) {
#ifndef _WIN32
        BlockResult result;
        result.success = false;
        result.message = "현재 위치 클릭은 Windows에서만 지원됩니다";
        return result;
#else
        InputSimulator::clickAtCursor(button, action, count, modifiers);
#endif
    } else if (target == ClickTarget::LastMatch) {
        int screenX = 0;
        int screenY = 0;
        bool haveScreen = false;
        if (useClientCoordinates && ctx.hasLastMatchScreenPoint()) {
            const cv::Point screen = ctx.lastMatchScreenPoint();
            screenX = screen.x;
            screenY = screen.y;
            haveScreen = true;
        } else if (useClientCoordinates) {
            HWND hwnd = ctx.targetWindow();
            if (hwnd && InputSimulator::clientToScreen(hwnd, clickX, clickY, screenX, screenY)) {
                haveScreen = true;
            }
        } else {
            screenX = clickX;
            screenY = clickY;
            haveScreen = true;
        }
        if (haveScreen) {
            // Prefer client path so out-of-bounds screen points from bad mapping are clamped.
            HWND hwnd = ctx.targetWindow();
            int clientX = 0;
            int clientY = 0;
            if (hwnd && InputSimulator::screenToClient(hwnd, screenX, screenY, clientX, clientY)) {
                InputSimulator::clickAtClient(hwnd, clientX, clientY, button, action, count, modifiers);
            } else {
                InputSimulator::clickAtMatchScreen(screenX, screenY, button, action, count, modifiers);
            }
        } else {
            InputSimulator::clickAt(clickX, clickY, button, action, count, modifiers);
        }
    } else {
        HWND hwnd = ctx.targetWindow();
        if (useClientCoordinates && hwnd) {
            InputSimulator::clickAtClient(hwnd, clickX, clickY, button, action, count, modifiers);
        } else {
            InputSimulator::clickAt(clickX, clickY, button, action, count, modifiers);
        }
    }
#else
    if (action == ClickAction::MoveOnly) {
        InputSimulator::moveAt(clickX, clickY);
    } else {
        InputSimulator::clickAt(clickX, clickY, button, action, count, modifiers);
    }
#endif

#ifdef _WIN32
    if (ctx.pointerVisualFeedback()) {
        int feedbackClientX = 0;
        int feedbackClientY = 0;
        if (target == ClickTarget::CurrentPosition) {
            int screenX = 0;
            int screenY = 0;
            if (InputSimulator::getCursorScreenPosition(screenX, screenY)
                && clientPointForPointerFeedback(
                    ctx, screenX, screenY, false, feedbackClientX, feedbackClientY)) {
                ctx.reportPointerFeedback(feedbackClientX, feedbackClientY);
            }
        } else if (clientPointForPointerFeedback(
                       ctx, clickX, clickY, useClientCoordinates, feedbackClientX, feedbackClientY)) {
            ctx.reportPointerFeedback(feedbackClientX, feedbackClientY);
        }
    }
#endif

    BlockResult result;
    result.success = true;
    if (action == ClickAction::MoveOnly) {
        result.message = std::string("커서만 이동 · (") + std::to_string(clickX) + ","
                         + std::to_string(clickY) + ")";
    } else if (target == ClickTarget::CurrentPosition) {
        result.message = clickActionDisplayName(action) + " · 현재 위치";
    } else {
        result.message = clickActionDisplayName(action) + " · (" + std::to_string(clickX) + ","
                         + std::to_string(clickY) + ")";
    }
    ctx.log(result.message);
    return result;
}

nlohmann::json ClickBlock::toJson() const {
    nlohmann::json json{
        {"type", "Click"},
        {"target", clickTargetToString(target)},
        {"x", x},
        {"y", y},
        {"button", mouseButtonToString(button)},
        {"count", count},
        {"useClientCoordinates", useClientCoordinates}};
    if (action != ClickAction::Tap) {
        json["action"] = clickActionToString(action);
    }
    if (lastMatchRelativeOffset) {
        json["lastMatchRelativeOffset"] = true;
    }
    if (modifiers.ctrl) {
        json["ctrl"] = true;
    }
    if (modifiers.alt) {
        json["alt"] = true;
    }
    if (modifiers.shift) {
        json["shift"] = true;
    }
    return json;
}

std::unique_ptr<Block> ClickBlock::clone() const {
    return std::make_unique<ClickBlock>(*this);
}

std::unique_ptr<ClickBlock> ClickBlock::fromJson(const nlohmann::json& json) {
    auto block = std::make_unique<ClickBlock>();
    block->target = clickTargetFromString(json.value("target", "LastMatch"));
    block->x = json.value("x", 0);
    block->y = json.value("y", 0);
    block->lastMatchRelativeOffset = json.value("lastMatchRelativeOffset", false);
    block->button = mouseButtonFromString(json.value("button", "Left"));
    block->action = clickActionFromString(json.value("action", "Tap"));
    block->count = json.value("count", 1);
    block->useClientCoordinates = json.value("useClientCoordinates", true);
    block->modifiers.ctrl = json.value("ctrl", false);
    block->modifiers.alt = json.value("alt", false);
    block->modifiers.shift = json.value("shift", false);
    return block;
}
