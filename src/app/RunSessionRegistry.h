#pragma once

#include "app/FeatureRunSession.h"

#include <cstddef>
#include <map>
#include <string>

/// Live feature run sessions keyed by feature id (AGENTS.md §8.21 R5.2).
/// Map storage lives here; insert/erase/tear-down via `RunLifecycleCoordinator`.
class RunSessionRegistry {
public:
    using Map = std::map<std::string, FeatureRunSession>;

    Map& sessions() { return m_sessions; }
    const Map& sessions() const { return m_sessions; }

    std::size_t size() const { return m_sessions.size(); }
    bool empty() const { return m_sessions.empty(); }

private:
    Map m_sessions;
};
