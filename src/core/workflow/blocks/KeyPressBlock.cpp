#include "core/workflow/blocks/KeyPressBlock.h"

#include "core/input/HotkeyBinding.h"
#include "core/workflow/ExecutionContext.h"

#include <QStringList>

namespace {

std::string keyActionToString(KeyAction action) {
    switch (action) {
    case KeyAction::Down:
        return "Down";
    case KeyAction::Up:
        return "Up";
    case KeyAction::Tap:
    default:
        return "Tap";
    }
}

std::string keyActionDisplayName(KeyAction action) {
    switch (action) {
    case KeyAction::Down:
        return "누름";
    case KeyAction::Up:
        return "뗌";
    case KeyAction::Tap:
    default:
        return "탭";
    }
}

KeyAction keyActionFromString(const std::string& value) {
    if (value == "Down") {
        return KeyAction::Down;
    }
    if (value == "Up") {
        return KeyAction::Up;
    }
    return KeyAction::Tap;
}

std::string modifierKeyActionToString(ModifierKeyAction action) {
    switch (action) {
    case ModifierKeyAction::Tap:
        return "Tap";
    case ModifierKeyAction::Down:
        return "Down";
    case ModifierKeyAction::Up:
        return "Up";
    case ModifierKeyAction::None:
    default:
        return "None";
    }
}

ModifierKeyAction modifierKeyActionFromString(const std::string& value) {
    if (value == "Tap") {
        return ModifierKeyAction::Tap;
    }
    if (value == "Down") {
        return ModifierKeyAction::Down;
    }
    if (value == "Up") {
        return ModifierKeyAction::Up;
    }
    return ModifierKeyAction::None;
}

std::string modifierKeyActionDisplaySuffix(ModifierKeyAction action) {
    switch (action) {
    case ModifierKeyAction::Down:
        return "↓";
    case ModifierKeyAction::Up:
        return "↑";
    case ModifierKeyAction::Tap:
        return "↔";
    case ModifierKeyAction::None:
    default:
        return "";
    }
}

void appendModifierSummary(QStringList& parts, const QString& name, ModifierKeyAction action) {
    if (action == ModifierKeyAction::None) {
        return;
    }
    parts.append(name + QString::fromStdString(modifierKeyActionDisplaySuffix(action)));
}

QString keyPressBindingLabel(int virtualKey,
                             const KeyPressModifierActions& modifierActions,
                             bool useMainKey) {
    QStringList parts;
    appendModifierSummary(parts, QStringLiteral("Ctrl"), modifierActions.ctrl);
    appendModifierSummary(parts, QStringLiteral("Alt"), modifierActions.alt);
    appendModifierSummary(parts, QStringLiteral("Shift"), modifierActions.shift);
    if (useMainKey) {
        parts.append(HotkeyBinding::keyDisplayName(virtualKey));
    }
    return parts.join(QStringLiteral("+"));
}

ModifierKeyAction loadModifierAction(const nlohmann::json& json, const char* actionField) {
    if (json.contains(actionField)) {
        return modifierKeyActionFromString(json.value(actionField, "None"));
    }
    return ModifierKeyAction::None;
}

} // namespace

std::string KeyPressBlock::summary() const {
    const QString label = keyPressBindingLabel(virtualKey, modifierActions, useMainKey);
    if (!useMainKey) {
        return label.isEmpty() ? std::string("수정키") : label.toStdString();
    }
    return label.toStdString() + " " + keyActionDisplayName(action);
}

BlockResult KeyPressBlock::execute(ExecutionContext& ctx) {
    if (!useMainKey && !modifierActions.hasAny()) {
        BlockResult result;
        result.success = false;
        result.message = "수정키 동작이 지정되지 않음";
        ctx.log(result.message);
        return result;
    }

    InputSimulator::sendKey(virtualKey, action, modifierActions, useMainKey);
    BlockResult result;
    result.success = true;
    result.message = summary();
    ctx.log(result.message);
    return result;
}

nlohmann::json KeyPressBlock::toJson() const {
    nlohmann::json json{{"type", "KeyPress"}};
    if (!useMainKey) {
        json["useMainKey"] = false;
    } else {
        json["virtualKey"] = virtualKey;
        json["action"] = keyActionToString(action);
    }
    if (modifierActions.ctrl != ModifierKeyAction::None) {
        json["ctrlAction"] = modifierKeyActionToString(modifierActions.ctrl);
    }
    if (modifierActions.alt != ModifierKeyAction::None) {
        json["altAction"] = modifierKeyActionToString(modifierActions.alt);
    }
    if (modifierActions.shift != ModifierKeyAction::None) {
        json["shiftAction"] = modifierKeyActionToString(modifierActions.shift);
    }
    return json;
}

std::unique_ptr<Block> KeyPressBlock::clone() const {
    return std::make_unique<KeyPressBlock>(*this);
}

std::unique_ptr<KeyPressBlock> KeyPressBlock::fromJson(const nlohmann::json& json) {
    auto block = std::make_unique<KeyPressBlock>();
    block->useMainKey = json.value("useMainKey", true);
    block->virtualKey = json.value("virtualKey", 0x20);
    block->action = keyActionFromString(json.value("action", "Tap"));
    block->modifierActions.ctrl = loadModifierAction(json, "ctrlAction");
    block->modifierActions.alt = loadModifierAction(json, "altAction");
    block->modifierActions.shift = loadModifierAction(json, "shiftAction");
    return block;
}