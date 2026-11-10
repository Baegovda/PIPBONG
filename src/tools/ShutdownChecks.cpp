#include "app/ShutdownCoordinator.h"
#include <iostream>
#include <vector>

bool runShutdownChecks() {
    const auto expect = [](bool value, const char* message) {
        if (!value) std::cerr << "Shutdown check failed: " << message << '\n';
        return value;
    };
    {
        bool allowed=false;
        int writes=0;
        ShutdownCoordinator::HostCallbacks host;
        host.canBegin=[&] { return allowed; };
        host.quiesce=[&] { ++writes; };
        host.saveProject=host.saveSettings=host.saveMemo=[] { return true; };
        host.finalize=[] {};
        ShutdownCoordinator coordinator(host);
        if (!expect(!coordinator.checkpoint() && !coordinator.prepare() && writes == 0,
                    "active transaction prevents quiescence, saves and teardown")) return false;
        allowed=true;
        if (!expect(coordinator.checkpoint() && writes == 1, "transaction release permits later checkpoint")) return false;
    }
    for (int failure = 1; failure <= 3; ++failure) {
        std::vector<int> calls;
        bool allow = false;
        bool nestedRejected = false;
        ShutdownCoordinator* active = nullptr;
        ShutdownCoordinator::HostCallbacks host;
        host.quiesce = [&] { calls.push_back(0); nestedRejected = !active->prepare(); };
        host.saveProject = [&] { calls.push_back(1); return failure != 1 || allow; };
        host.saveSettings = [&] { calls.push_back(2); return failure != 2 || allow; };
        host.saveMemo = [&] { calls.push_back(3); return failure != 3 || allow; };
        host.finalize = [&] { calls.push_back(4); };
        ShutdownCoordinator coordinator(host);
        active = &coordinator;
        if (!expect(!coordinator.prepare() && !coordinator.ready() && !coordinator.busy()
                    && calls.back() == failure && nestedRejected && !coordinator.lastError().isEmpty(),
                    "save failure prevents teardown and rejects reentry")) return false;
        allow = true;
        calls.clear();
        if (!expect(coordinator.checkpoint() && !coordinator.ready() && calls == std::vector<int>({0,1,2,3}),
                    "update checkpoint persists without teardown")) return false;
        calls.clear();
        if (!expect(coordinator.prepare() && coordinator.ready() && calls == std::vector<int>({0,1,2,3,4}),
                    "retry saves all data before finalizing")) return false;
        calls.clear();
        if (!expect(coordinator.prepare() && coordinator.checkpoint() && calls.empty(), "repeated close finalizes once")) return false;
    }
    std::cout << "Shutdown: failed project/settings/memo saves block teardown, checkpoint/reentry/retry/idempotent close OK\n";
    return true;
}
