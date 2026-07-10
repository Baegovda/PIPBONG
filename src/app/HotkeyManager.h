#pragma once

#include "core/input/HotkeyBinding.h"

#include <QObject>

#include <QtGlobal>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Project;
class Feature;
class QAbstractNativeEventFilter;

class HotkeyManager : public QObject {
    Q_OBJECT
public:
    struct RegistrationFailure {
        std::string featureId;
        std::string featureName;
#ifdef _WIN32
        unsigned long win32Error = 0;
#endif
    };

    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;

    std::vector<RegistrationFailure> syncFromProject(const Project& project);
    void unregisterAll();

    static const Feature* findDuplicateHotkey(const Project& project,
                                              const std::string& featureId,
                                              const HotkeyBinding& binding);

    bool isHoldBindingDown(const std::string& featureId) const;
    /** Clears stale hook state when the binding is no longer physically down; may emit hold ended. */
    bool reconcileHoldBindingDown(const std::string& featureId);
    bool matchesAnyRegisteredFeatureHotkey(int vkCode) const;
#ifdef _WIN32
    bool isKeyboardHookActive() const { return m_keyboardHookInstalled; }
    bool isMouseHookActive() const { return m_mouseHookInstalled; }
    void dispatchWin32Hotkey(int hotkeyId);
#endif

#ifdef _WIN32
    /** Returns true when the event was consumed as a feature hotkey (block from target apps). */
    bool handleKeyboardHookEvent(int vkCode, bool keyDown);
    bool handleMouseButtonEvent(int vkCode, bool buttonDown);
#endif

signals:
    void hotkeyTriggered(const QString& featureId);
    void hotkeyHoldStarted(const QString& featureId);
    void hotkeyHoldEnded(const QString& featureId);

private:
    struct HoldBindingEntry {
        std::string featureId;
        HotkeyBinding binding;
        bool allowExtraModifiers = false;
        bool keyDown = false;
    };

    struct ToggleBindingEntry {
        std::string featureId;
        HotkeyBinding binding;
        bool allowExtraModifiers = false;
        bool armed = true;
    };

    struct MouseBindingEntry {
        std::string featureId;
        HotkeyBinding binding;
        bool allowExtraModifiers = false;
        bool holdMode = false;
        bool buttonDown = false;
    };

    void handleHotkey(int hotkeyId);
    void emitHotkeyTriggered(const std::string& featureId);
    void emitHotkeyHoldStarted(const std::string& featureId);
    void emitHotkeyHoldEnded(const std::string& featureId);
#ifdef _WIN32
    void scheduleHoldReleaseRecheck(const std::string& featureId, int vkCode, int attempt = 0);
    bool finalizeHoldReleaseIfPhysicallyUp(const std::string& featureId, int vkCode);
#endif
#ifdef _WIN32
    void installKeyboardHook();
    void uninstallKeyboardHook();
    void installMouseHook();
    void uninstallMouseHook();
    bool registerHotKeyFallback(const std::string& featureId, const HotkeyBinding& binding);
#endif

    class NativeFilter;
    std::unique_ptr<NativeFilter> m_filter;
    std::unordered_map<int, std::string> m_idToFeatureId;
    std::vector<HoldBindingEntry> m_holdBindings;
    std::vector<ToggleBindingEntry> m_toggleBindings;
    std::vector<MouseBindingEntry> m_mouseBindings;
    int m_nextId = 1;
#ifdef _WIN32
    void* m_hotkeyHostHwnd = nullptr;
    void* m_keyboardHook = nullptr;
    void* m_mouseHook = nullptr;
    bool m_keyboardHookInstalled = false;
    bool m_mouseHookInstalled = false;
    std::unordered_map<std::string, quint64> m_holdReleaseRecheckGeneration;
#endif
};
