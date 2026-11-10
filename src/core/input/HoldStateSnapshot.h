#pragma once
#include "core/input/HotkeyBinding.h"
#include <atomic>
#include <functional>
#include <memory>

/// Copies share an atomic latch; hook/GUI mutation never exposes a container to workers.
class SharedHoldLatch {
public:
    SharedHoldLatch(bool value = false) : m_value(std::make_shared<std::atomic<bool>>(value)) {}
    operator bool() const { return m_value->load(std::memory_order_relaxed); }
    SharedHoldLatch& operator=(bool value) { m_value->store(value, std::memory_order_relaxed); return *this; }
    std::shared_ptr<std::atomic<bool>> state() const { return m_value; }
private:
    std::shared_ptr<std::atomic<bool>> m_value;
};

struct HoldStateSnapshot {
    HotkeyBinding binding;
    bool allowExtraModifiers = false;
    std::shared_ptr<std::atomic<bool>> latched;
    std::shared_ptr<std::atomic<bool>> registered;
    bool active(const std::function<bool()>& physicalQuery = {}) const {
        if (!registered || !registered->load(std::memory_order_relaxed)) return false;
        bool physical = physicalQuery ? physicalQuery() : false;
#ifdef _WIN32
        if (!physicalQuery) physical = binding.isPhysicallyDown(allowExtraModifiers);
#endif
        return physical || (latched && latched->load(std::memory_order_relaxed));
    }
};
