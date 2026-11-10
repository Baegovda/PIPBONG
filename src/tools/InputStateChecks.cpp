#include "core/input/InputSimulator.h"
#include "core/workflow/ExecutionContext.h"
#include <iostream>
#include <vector>

bool runInputStateChecks() {
#ifdef _WIN32
    std::vector<INPUT> recorded;
    int physicallyHeld = 0;
    InputSimulator::KeyboardBackendForTest backend;
    backend.keyDown = [&](int vk) { return physicallyHeld != 0 && vk == physicallyHeld; };
    backend.record = [&](const INPUT* inputs, unsigned count) { recorded.insert(recorded.end(), inputs, inputs + count); };
    InputSimulator::ScopedKeyboardBackendForTest recording(backend);
    struct ContextGuard {
        ExecutionContext* previous = InputSimulator::activeExecutionContext();
        ~ContextGuard() { InputSimulator::setActiveExecutionContext(previous); }
    } guard;
    const auto expect = [](bool value, const char* message) {
        if (!value) std::cerr << "Input state check failed: " << message << '\n';
        return value;
    };
    for (int side : {VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU}) {
        physicallyHeld = side;
        recorded.clear();
        ExecutionContext context;
        InputSimulator::setActiveExecutionContext(&context);
        context.beginRunKeyboardSessionIfNeeded();
        const int generic = (side == VK_LSHIFT || side == VK_RSHIFT) ? VK_SHIFT
            : (side == VK_LCONTROL || side == VK_RCONTROL) ? VK_CONTROL : VK_MENU;
        InputSimulator::sendKey(generic, KeyAction::Tap);
        if (!expect(recorded.empty(), "tap does not disturb physically held modifier")) return false;
        InputSimulator::sendKey('A', KeyAction::Down);
        context.noteSyntheticKeyDown(side); // Even a stale tracked modifier must be protected.
        context.endRunKeyboardSession();
        if (!expect(recorded.size() == 2 && (recorded.back().ki.dwFlags & KEYEVENTF_KEYUP)
                    && !context.hasPipbongSyntheticKeyDown('A') && !context.hasPipbongSyntheticKeyDown(side),
                    "session end releases only injected A, protects user modifier, clears tracking")) return false;
    }
    physicallyHeld = 0;
    recorded.clear();
    ExecutionContext context;
    InputSimulator::setActiveExecutionContext(&context);
    context.beginRunKeyboardSessionIfNeeded();
    InputSimulator::sendKey(VK_LSHIFT, KeyAction::Down);
    context.restoreRunKeyboard();
    if (!expect(recorded.size() == 2 && !(recorded.front().ki.dwFlags & KEYEVENTF_KEYUP)
                && (recorded.back().ki.dwFlags & KEYEVENTF_KEYUP), "synthetic modifier released per iteration")) return false;
    context.restoreRunKeyboard();
    context.endRunKeyboardSession();
    if (!expect(recorded.size() == 2, "repeat teardown emits no duplicate releases")) return false;
    physicallyHeld = VK_LSHIFT;
    recorded.clear();
    KeyPressModifierActions actions;
    actions.shift = ModifierKeyAction::Tap;
    InputSimulator::sendKey('A', KeyAction::Tap, actions);
    if (!expect(recorded.size() == 2, "modified key tap preserves pre-block physical Shift")) return false;
    InputSimulator::setActiveExecutionContext(nullptr);
    std::cout << "Input state: recorded production keyboard paths preserve six held modifiers and release synthetic keys; no OS input OK\n";
#endif
    return true;
}
