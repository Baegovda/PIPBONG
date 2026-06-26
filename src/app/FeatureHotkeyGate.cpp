#include "app/FeatureHotkeyGate.h"

#include <atomic>

namespace {

std::atomic<int> g_featureHotkeyBlockCount{0};

} // namespace

bool FeatureHotkeyGate::isFeatureHotkeysBlocked() {
    return g_featureHotkeyBlockCount.load() > 0;
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
