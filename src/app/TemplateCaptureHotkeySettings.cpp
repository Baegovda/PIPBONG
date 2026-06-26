#include "app/TemplateCaptureHotkeySettings.h"

#include <QSettings>

namespace {

constexpr auto kCapturePrefix = "hotkeys/templateCapture";
constexpr auto kLegacyMatchTestPrefix = "hotkeys/matchTest";

HotkeyBinding readBinding(QSettings& settings, const char* prefix) {
    const QString base = QString::fromLatin1(prefix);
    HotkeyBinding binding;
    binding.virtualKey = settings.value(base + QStringLiteral("/virtualKey"), 0).toInt();
    binding.ctrl = settings.value(base + QStringLiteral("/ctrl"), false).toBool();
    binding.alt = settings.value(base + QStringLiteral("/alt"), false).toBool();
    binding.shift = settings.value(base + QStringLiteral("/shift"), false).toBool();
    return binding;
}

void writeBinding(QSettings& settings, const char* prefix, const HotkeyBinding& binding) {
    const QString base = QString::fromLatin1(prefix);
    settings.setValue(base + QStringLiteral("/virtualKey"), binding.virtualKey);
    settings.setValue(base + QStringLiteral("/ctrl"), binding.ctrl);
    settings.setValue(base + QStringLiteral("/alt"), binding.alt);
    settings.setValue(base + QStringLiteral("/shift"), binding.shift);
}

void removeBinding(QSettings& settings, const char* prefix) {
    settings.remove(QString::fromLatin1(prefix));
}

void migrateLegacyMatchTestSettings(QSettings& settings) {
    const HotkeyBinding legacy = readBinding(settings, kLegacyMatchTestPrefix);
    if (legacy.isEmpty()) {
        return;
    }
    writeBinding(settings, kCapturePrefix, legacy);
    removeBinding(settings, kLegacyMatchTestPrefix);
}

} // namespace

HotkeyBinding TemplateCaptureHotkeySettings::binding() {
    QSettings settings;
    HotkeyBinding binding = readBinding(settings, kCapturePrefix);
    if (binding.isEmpty()) {
        migrateLegacyMatchTestSettings(settings);
        binding = readBinding(settings, kCapturePrefix);
    }
    return binding;
}

void TemplateCaptureHotkeySettings::setBinding(const HotkeyBinding& binding) {
    QSettings settings;
    if (binding.isEmpty()) {
        removeBinding(settings, kCapturePrefix);
        return;
    }
    writeBinding(settings, kCapturePrefix, binding);
}

void TemplateCaptureHotkeySettings::importLegacyIfUnset(const HotkeyBinding& legacy) {
    if (legacy.isEmpty()) {
        return;
    }
    QSettings settings;
    if (!readBinding(settings, kCapturePrefix).isEmpty()) {
        return;
    }
    migrateLegacyMatchTestSettings(settings);
    if (!readBinding(settings, kCapturePrefix).isEmpty()) {
        return;
    }
    writeBinding(settings, kCapturePrefix, legacy);
}
