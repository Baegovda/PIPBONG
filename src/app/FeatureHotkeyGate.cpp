#include "app/FeatureHotkeyGate.h"

#include <QApplication>
#include <QDialog>
#include <QWidget>

#include <atomic>

namespace {

std::atomic<int> g_featureHotkeyBlockCount{0};

bool anyVisibleAppDialogOpen() {
    if (!QApplication::instance()) {
        return false;
    }
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (QWidget* widget : tops) {
        const auto* dialog = qobject_cast<const QDialog*>(widget);
        if (dialog && dialog->isVisible()) {
            return true;
        }
    }
    return false;
}

} // namespace

bool FeatureHotkeyGate::isFeatureHotkeysBlocked() {
    if (g_featureHotkeyBlockCount.load() > 0) {
        return true;
    }
    // Any editing / tool dialog left open (modal or modeless) suppresses feature bindings
    // so hotkeys cannot start unrelated features while the user is configuring UI.
    return anyVisibleAppDialogOpen();
}

void FeatureHotkeyGate::addBlocker() {
    g_featureHotkeyBlockCount.fetch_add(1);
}

void FeatureHotkeyGate::removeBlocker() {
    g_featureHotkeyBlockCount.fetch_sub(1);
}

FeatureHotkeyGateScope::FeatureHotkeyGateScope() {
    FeatureHotkeyGate::addBlocker();
}

FeatureHotkeyGateScope::~FeatureHotkeyGateScope() {
    if (m_active) {
        FeatureHotkeyGate::removeBlocker();
    }
}
