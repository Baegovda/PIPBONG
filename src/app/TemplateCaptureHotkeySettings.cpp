#include "app/TemplateCaptureHotkeySettings.h"

#include <QSettings>

namespace {

constexpr auto kCapturePrefix = "hotkeys/templateCapture";

HotkeyBinding readBinding(QSettings& settings) {
    const QString base = QString::fromLatin1(kCapturePrefix);
    HotkeyBinding binding;
    binding.virtualKey = settings.value(base + QStringLiteral("/virtualKey"), 0).toInt();
    binding.ctrl = settings.value(base + QStringLiteral("/ctrl"), false).toBool();
    binding.alt = settings.value(base + QStringLiteral("/alt"), false).toBool();
    binding.shift = settings.value(base + QStringLiteral("/shift"), false).toBool();
    return binding;
}

void writeBinding(QSettings& settings, const HotkeyBinding& binding) {
    const QString base = QString::fromLatin1(kCapturePrefix);
    settings.setValue(base + QStringLiteral("/virtualKey"), binding.virtualKey);
    settings.setValue(base + QStringLiteral("/ctrl"), binding.ctrl);
    settings.setValue(base + QStringLiteral("/alt"), binding.alt);
    settings.setValue(base + QStringLiteral("/shift"), binding.shift);
}

void removeBinding(QSettings& settings) {
    settings.remove(QString::fromLatin1(kCapturePrefix));
}

} // namespace

HotkeyBinding TemplateCaptureHotkeySettings::binding() {
    QSettings settings;
    return readBinding(settings);
}

void TemplateCaptureHotkeySettings::setBinding(const HotkeyBinding& binding) {
    QSettings settings;
    if (binding.isEmpty()) {
        removeBinding(settings);
        return;
    }
    writeBinding(settings, binding);
}
