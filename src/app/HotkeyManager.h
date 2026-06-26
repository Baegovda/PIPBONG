#pragma once

#include "core/input/HotkeyBinding.h"

#include <QObject>

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
    };

    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;

    std::vector<RegistrationFailure> syncFromProject(const Project& project);
    void unregisterAll();

    static const Feature* findDuplicateHotkey(const Project& project,
                                              const std::string& featureId,
                                              const HotkeyBinding& binding);

    bool isHoldBindingDown(const std::string& featureId) const;

#ifdef _WIN32
    /** Returns true when the event was consumed as a feature hotkey (block from target apps). */
    bool handleHoldKeyEvent(int vkCode, bool keyDown);
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
        bool keyDown = false;
    };

    struct MouseBindingEntry {
        std::string featureId;
        HotkeyBinding binding;
        bool holdMode = false;
        bool buttonDown = false;
    };

    void handleHotkey(int hotkeyId);
#ifdef _WIN32
    void installHoldHook();
    void uninstallHoldHook();
    void installMouseHook();
    void uninstallMouseHook();
#endif

    class NativeFilter;
    std::unique_ptr<NativeFilter> m_filter;
    std::unordered_map<int, std::string> m_idToFeatureId;
    std::vector<HoldBindingEntry> m_holdBindings;
    std::vector<MouseBindingEntry> m_mouseBindings;
    int m_nextId = 1;
#ifdef _WIN32
    void* m_hotkeyHostHwnd = nullptr;
    void* m_holdKeyboardHook = nullptr;
    void* m_mouseHook = nullptr;
    bool m_holdHookInstalled = false;
    bool m_mouseHookInstalled = false;
#endif
};
