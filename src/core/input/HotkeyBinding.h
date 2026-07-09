#pragma once

#include <nlohmann/json.hpp>

#include <QString>
#include <Qt>

struct HotkeyBinding {
    int virtualKey = 0;
    bool ctrl = false;
    bool alt = false;
    bool shift = false;

    bool isEmpty() const { return virtualKey == 0; }

    bool operator==(const HotkeyBinding& other) const {
        return virtualKey == other.virtualKey && ctrl == other.ctrl && alt == other.alt
               && shift == other.shift;
    }

    bool operator!=(const HotkeyBinding& other) const { return !(*this == other); }

    nlohmann::json toJson() const;
    static HotkeyBinding fromJson(const nlohmann::json& json);

    QString displayString() const;
    static QString keyDisplayName(int virtualKey);

#ifdef _WIN32
    bool isMouseButton() const;
    bool modifiersMatch(bool allowExtraModifiers = false) const;
    unsigned int winModifiers() const;
    bool isPressed() const;
    bool isPhysicallyDown(bool allowExtraModifiers = false) const;
    bool matchesVirtualKey(int vkCode) const;
    static bool isMouseVirtualKey(int vk);
    static bool isModifierOnlyVirtualKey(int vk);
    static int qtKeyToVirtualKey(int qtKey);
    static int qtMouseButtonToVirtualKey(Qt::MouseButton button);
    static int virtualKeyToQtKey(int vk);
#endif
};
