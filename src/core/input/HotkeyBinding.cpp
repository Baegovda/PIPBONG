#include "core/input/HotkeyBinding.h"

#include <QKeySequence>
#include <QStringList>
#include <Qt>

#ifdef _WIN32
#include <windows.h>

namespace {

QString normalizeKeyDisplayName(QString name) {
    return name.replace(QStringLiteral("Return"), QStringLiteral("Enter"));
}

QString mouseButtonDisplayName(int vk) {
    switch (vk) {
    case VK_LBUTTON:
        return QStringLiteral("왼쪽 버튼");
    case VK_RBUTTON:
        return QStringLiteral("오른쪽 버튼");
    case VK_MBUTTON:
        return QStringLiteral("휠 클릭");
    case VK_XBUTTON1:
        return QStringLiteral("뒤로 가기");
    case VK_XBUTTON2:
        return QStringLiteral("앞으로 가기");
    default:
        return QStringLiteral("VK %1").arg(vk);
    }
}

} // namespace
#endif

nlohmann::json HotkeyBinding::toJson() const {
    return nlohmann::json{
        {"virtualKey", virtualKey},
        {"ctrl", ctrl},
        {"alt", alt},
        {"shift", shift},
    };
}

HotkeyBinding HotkeyBinding::fromJson(const nlohmann::json& json) {
    HotkeyBinding binding;
    if (!json.is_object()) {
        return binding;
    }
    binding.virtualKey = json.value("virtualKey", 0);
    binding.ctrl = json.value("ctrl", false);
    binding.alt = json.value("alt", false);
    binding.shift = json.value("shift", false);
    return binding;
}

QString HotkeyBinding::keyDisplayName(int virtualKey) {
#ifdef _WIN32
    if (virtualKey >= 'A' && virtualKey <= 'Z') {
        return QString(QChar(virtualKey));
    }
    if (virtualKey >= '0' && virtualKey <= '9') {
        return QString(QChar(virtualKey));
    }
    if (isMouseVirtualKey(virtualKey)) {
        return mouseButtonDisplayName(virtualKey);
    }
    if (virtualKey == VK_RETURN) {
        return QStringLiteral("Enter");
    }
    const int qtKey = virtualKeyToQtKey(virtualKey);
    const QString keyName =
        normalizeKeyDisplayName(QKeySequence(qtKey).toString(QKeySequence::NativeText));
    if (!keyName.isEmpty()) {
        return keyName;
    }
#endif
    return QStringLiteral("VK %1").arg(virtualKey);
}

QString HotkeyBinding::displayString() const {
    if (isEmpty()) {
        return QString();
    }

    QStringList parts;
    if (ctrl) {
        parts.append(QStringLiteral("Ctrl"));
    }
    if (alt) {
        parts.append(QStringLiteral("Alt"));
    }
    if (shift) {
        parts.append(QStringLiteral("Shift"));
    }

    parts.append(keyDisplayName(virtualKey));

    return parts.join(QStringLiteral("+"));
}

#ifdef _WIN32

bool HotkeyBinding::isMouseVirtualKey(int vk) {
    switch (vk) {
    case VK_LBUTTON:
    case VK_RBUTTON:
    case VK_MBUTTON:
    case VK_XBUTTON1:
    case VK_XBUTTON2:
        return true;
    default:
        return false;
    }
}

bool HotkeyBinding::isMouseButton() const {
    return isMouseVirtualKey(virtualKey);
}

bool HotkeyBinding::modifiersMatch(bool allowExtraModifiers) const {
    const bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    const bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    if (allowExtraModifiers) {
        if (ctrl && !ctrlDown) {
            return false;
        }
        if (alt && !altDown) {
            return false;
        }
        if (shift && !shiftDown) {
            return false;
        }
        return true;
    }

    return ctrl == ctrlDown && alt == altDown && shift == shiftDown;
}

unsigned int HotkeyBinding::winModifiers() const {
    unsigned int mods = MOD_NOREPEAT;
    if (alt) {
        mods |= MOD_ALT;
    }
    if (ctrl) {
        mods |= MOD_CONTROL;
    }
    if (shift) {
        mods |= MOD_SHIFT;
    }
    return mods;
}

bool HotkeyBinding::isPressed() const {
    if (isEmpty()) {
        return false;
    }
    const bool keyDown = (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
    if (!keyDown) {
        return false;
    }
    return modifiersMatch();
}

bool HotkeyBinding::matchesVirtualKey(int vkCode) const {
    return !isEmpty() && virtualKey == vkCode;
}

bool HotkeyBinding::isModifierOnlyVirtualKey(int vk) {
    switch (vk) {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LWIN:
    case VK_RWIN:
        return true;
    default:
        return false;
    }
}

int HotkeyBinding::qtKeyToVirtualKey(int qtKey) {
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        return 'A' + (qtKey - Qt::Key_A);
    }
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        return '0' + (qtKey - Qt::Key_0);
    }
    switch (qtKey) {
    case Qt::Key_Space:
        return VK_SPACE;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return VK_RETURN;
    case Qt::Key_Escape:
        return VK_ESCAPE;
    case Qt::Key_Tab:
        return VK_TAB;
    case Qt::Key_Backspace:
        return VK_BACK;
    case Qt::Key_Delete:
        return VK_DELETE;
    case Qt::Key_Home:
        return VK_HOME;
    case Qt::Key_End:
        return VK_END;
    case Qt::Key_PageUp:
        return VK_PRIOR;
    case Qt::Key_PageDown:
        return VK_NEXT;
    case Qt::Key_Left:
        return VK_LEFT;
    case Qt::Key_Right:
        return VK_RIGHT;
    case Qt::Key_Up:
        return VK_UP;
    case Qt::Key_Down:
        return VK_DOWN;
    case Qt::Key_F1:
        return VK_F1;
    case Qt::Key_F2:
        return VK_F2;
    case Qt::Key_F3:
        return VK_F3;
    case Qt::Key_F4:
        return VK_F4;
    case Qt::Key_F5:
        return VK_F5;
    case Qt::Key_F6:
        return VK_F6;
    case Qt::Key_F7:
        return VK_F7;
    case Qt::Key_F8:
        return VK_F8;
    case Qt::Key_F9:
        return VK_F9;
    case Qt::Key_F10:
        return VK_F10;
    case Qt::Key_F11:
        return VK_F11;
    case Qt::Key_F12:
        return VK_F12;
    default:
        return qtKey & 0xFF;
    }
}

int HotkeyBinding::qtMouseButtonToVirtualKey(Qt::MouseButton button) {
    switch (button) {
    case Qt::LeftButton:
        return VK_LBUTTON;
    case Qt::RightButton:
        return VK_RBUTTON;
    case Qt::MiddleButton:
        return VK_MBUTTON;
    case Qt::XButton1:
        return VK_XBUTTON1;
    case Qt::XButton2:
        return VK_XBUTTON2;
    default:
        return 0;
    }
}

int HotkeyBinding::virtualKeyToQtKey(int vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return Qt::Key_A + (vk - 'A');
    }
    if (vk >= '0' && vk <= '9') {
        return Qt::Key_0 + (vk - '0');
    }
    switch (vk) {
    case VK_SPACE:
        return Qt::Key_Space;
    case VK_RETURN:
        return Qt::Key_Return;
    case VK_ESCAPE:
        return Qt::Key_Escape;
    case VK_TAB:
        return Qt::Key_Tab;
    case VK_BACK:
        return Qt::Key_Backspace;
    case VK_DELETE:
        return Qt::Key_Delete;
    case VK_HOME:
        return Qt::Key_Home;
    case VK_END:
        return Qt::Key_End;
    case VK_PRIOR:
        return Qt::Key_PageUp;
    case VK_NEXT:
        return Qt::Key_PageDown;
    case VK_LEFT:
        return Qt::Key_Left;
    case VK_RIGHT:
        return Qt::Key_Right;
    case VK_UP:
        return Qt::Key_Up;
    case VK_DOWN:
        return Qt::Key_Down;
    case VK_F1:
        return Qt::Key_F1;
    case VK_F2:
        return Qt::Key_F2;
    case VK_F3:
        return Qt::Key_F3;
    case VK_F4:
        return Qt::Key_F4;
    case VK_F5:
        return Qt::Key_F5;
    case VK_F6:
        return Qt::Key_F6;
    case VK_F7:
        return Qt::Key_F7;
    case VK_F8:
        return Qt::Key_F8;
    case VK_F9:
        return Qt::Key_F9;
    case VK_F10:
        return Qt::Key_F10;
    case VK_F11:
        return Qt::Key_F11;
    case VK_F12:
        return Qt::Key_F12;
    default:
        return vk;
    }
}

#endif
