#include "app/FeatureHotkeyGate.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QWidget>

#include <atomic>

namespace {

std::atomic<int> g_featureHotkeyBlockCount{0};
std::function<bool(int, bool)> g_keyboardHookDeferPredicate;

constexpr char kFeatureHotkeyGateExemptProperty[] = "pipbong_featureHotkeyGateExempt";

bool isTextInputFocusWidget(const QWidget* widget) {
    if (!widget) {
        return false;
    }
    return qobject_cast<const QLineEdit*>(widget) || qobject_cast<const QTextEdit*>(widget)
           || qobject_cast<const QPlainTextEdit*>(widget)
           || qobject_cast<const QAbstractSpinBox*>(widget);
}

bool focusInExemptDialogTextInput() {
    QWidget* focus = QApplication::focusWidget();
    if (!isTextInputFocusWidget(focus)) {
        return false;
    }
    for (QWidget* ancestor = focus; ancestor; ancestor = ancestor->parentWidget()) {
        const auto* dialog = qobject_cast<const QDialog*>(ancestor);
        if (!dialog || !dialog->isVisible()) {
            continue;
        }
        if (dialog->property(kFeatureHotkeyGateExemptProperty).toBool()) {
            return true;
        }
    }
    return false;
}

bool anyVisibleAppDialogOpen() {
    if (!QApplication::instance()) {
        return false;
    }
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (QWidget* widget : tops) {
        const auto* dialog = qobject_cast<const QDialog*>(widget);
        if (!dialog || !dialog->isVisible()) {
            continue;
        }
        // Workflow-independent tools (memo, calculator, CPU watch) stay open while gaming.
        if (dialog->property(kFeatureHotkeyGateExemptProperty).toBool()) {
            continue;
        }
        return true;
    }
    return false;
}

} // namespace

bool FeatureHotkeyGate::isFeatureHotkeysBlocked() {
    if (g_featureHotkeyBlockCount.load() > 0) {
        return true;
    }
    if (focusInExemptDialogTextInput()) {
        return true;
    }
    // Any editing / tool dialog left open (modal or modeless) suppresses feature bindings
    // so hotkeys cannot start unrelated features while the user is configuring UI.
    return anyVisibleAppDialogOpen();
}

void FeatureHotkeyGate::setKeyboardHookDeferPredicate(
    std::function<bool(int vkCode, bool keyDown)> predicate) {
    g_keyboardHookDeferPredicate = std::move(predicate);
}

bool FeatureHotkeyGate::shouldDeferKeyboardHook(int vkCode, bool keyDown) {
    return g_keyboardHookDeferPredicate && g_keyboardHookDeferPredicate(vkCode, keyDown);
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
