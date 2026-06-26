#include "app/ClickCursorHotkeySettings.h"

#include <QSettings>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr auto kPrefix = "hotkeys/clickCursorPosition";

HotkeyBinding defaultBinding() {
    HotkeyBinding binding;
#ifdef _WIN32
    binding.virtualKey = VK_F1;
#endif
    return binding;
}

HotkeyBinding readBinding(QSettings& settings) {
    const QString base = QString::fromLatin1(kPrefix);
    HotkeyBinding binding;
    binding.virtualKey = settings.value(base + QStringLiteral("/virtualKey"), 0).toInt();
    binding.ctrl = settings.value(base + QStringLiteral("/ctrl"), false).toBool();
    binding.alt = settings.value(base + QStringLiteral("/alt"), false).toBool();
    binding.shift = settings.value(base + QStringLiteral("/shift"), false).toBool();
    return binding;
}

void writeBinding(QSettings& settings, const HotkeyBinding& binding) {
    const QString base = QString::fromLatin1(kPrefix);
    settings.setValue(base + QStringLiteral("/virtualKey"), binding.virtualKey);
    settings.setValue(base + QStringLiteral("/ctrl"), binding.ctrl);
    settings.setValue(base + QStringLiteral("/alt"), binding.alt);
    settings.setValue(base + QStringLiteral("/shift"), binding.shift);
}

void removeBinding(QSettings& settings) {
    settings.remove(QString::fromLatin1(kPrefix));
}

} // namespace

HotkeyBinding ClickCursorHotkeySettings::binding() {
    QSettings settings;
    HotkeyBinding binding = readBinding(settings);
    if (binding.isEmpty()) {
        return defaultBinding();
    }
    return binding;
}

void ClickCursorHotkeySettings::setBinding(const HotkeyBinding& binding) {
    QSettings settings;
    if (binding.isEmpty() || isDefaultBinding(binding)) {
        removeBinding(settings);
        return;
    }
    writeBinding(settings, binding);
}

bool ClickCursorHotkeySettings::isDefaultBinding(const HotkeyBinding& binding) {
    const HotkeyBinding defaultValue = defaultBinding();
    return binding.virtualKey == defaultValue.virtualKey && !binding.ctrl && !binding.alt && !binding.shift;
}
