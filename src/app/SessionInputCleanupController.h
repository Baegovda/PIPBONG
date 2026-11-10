#pragma once
#include <QPoint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#endif
class Feature;
struct FeatureRunSession;
class SessionInputCleanupController {
    friend class SessionInputCleanupUiChecks;
public:
#ifdef _WIN32
    using Handle=HWND;
#else
    using Handle=void*;
#endif
    struct Binding {std::wstring title,path;};
    struct Window {Handle hwnd=nullptr;unsigned long processId=0;std::wstring title,path;bool visible=true;};
    struct Ports {
        std::function<std::optional<Window>(const Binding&)> find;
        std::function<void(const Window&,int)> release;
        std::function<void(int)> cleanup;
        std::function<std::optional<QPoint>()> cursor;
        std::function<void(QPoint)> move;
    };
    enum class Outcome { Duplicate, Unavailable, NotInjected, TargetAttempted, CleanupAttempted };
    SessionInputCleanupController();
    explicit SessionInputCleanupController(Ports ports):m_ports(std::move(ports)){}
    static void bind(FeatureRunSession& session,const Feature& feature);
    void captureCursor(FeatureRunSession& session);
    void restoreCursor(const FeatureRunSession& session) const;
    Outcome releaseHold(FeatureRunSession& session);
private:
    static Binding binding(const FeatureRunSession& session);
    static bool injected(const FeatureRunSession& session,int key);
    static bool matches(const Window& window,const Binding& binding);
    Ports m_ports;
};
