#pragma once

#include "core/capture/ScreenCapture.h"
#include "model/TargetWindowDetailData.h"
#include <QString>
#include <functional>
#include <utility>
#include <memory>

// Read-only window selection and detail refresh policy. No widgets or MainWindow dependency.
class TargetWindowDetailsController {
    friend class TargetWindowDetailsUiChecks;
public:
    struct Bindings {
        QString mainTitle, subTitle, mainProcessPath, subProcessPath;
    };
    struct Request {
        Bindings bindings;
        QString profileId;
        bool globalDefault = true;
        bool captureOwnedBySession = false;
        bool allowIdleUpdates = true;
        bool deferProfileRefresh = false;
        quint64 revision = 0;
#ifdef _WIN32
        HWND foreground = nullptr;
        bool pipbongForeground = false;
        HWND remembered = nullptr;
        HWND hint = nullptr;
#endif
    };
    struct Result {
        enum class State { GlobalDefault, Unbound, Stored, Live, Unsupported };
        State state = State::Unbound;
        QString storedTitle, storedProcessPath;
        bool subTarget = false;
        TargetWindowDetailData data;
#ifdef _WIN32
        HWND hwnd = nullptr;
        ScreenCapture::TargetWindowInfo info;
#endif
    };
    struct Host {
        std::function<void()> synchronizeIdleCapture;
#ifdef _WIN32
        std::function<void(HWND, bool)> applyIdleTarget;
#endif
        std::function<bool(const QString&, const QString&, const QString&, bool)> updateProcessPath;
        std::function<void()> refreshProfiles;
        std::function<void()> workflowScaleChanged;
        // Read-only source/lifetime guard, rechecked after callbacks.
        std::function<bool(const Request&)> current;
    };
#ifdef _WIN32
    struct WindowAccess {
        std::function<bool(HWND)> exists;
        std::function<bool(HWND, ScreenCapture::TargetWindowInfo&)> read;
        std::function<HWND(const QString&, const QString&)> findVisible;
        std::function<bool(const QString&, HWND, const Bindings&, bool)> matchesScope;
    };
    TargetWindowDetailsController();
    explicit TargetWindowDetailsController(WindowAccess windows);
#else
    TargetWindowDetailsController()=default;
#endif
    ~TargetWindowDetailsController(){m_lifetime.reset();}
    void setHost(Host host) { m_host = std::move(host); }
    TargetWindowDetailsController(const TargetWindowDetailsController&)=delete;
    TargetWindowDetailsController& operator=(const TargetWindowDetailsController&)=delete;
    Result refresh(const Request& request);

private:
    std::shared_ptr<int> m_lifetime=std::make_shared<int>(0);
    bool m_refreshing=false;
    Host m_host;
    int m_clientWidth = 0;
    int m_clientHeight = 0;
#ifdef _WIN32
    WindowAccess m_windows;
#endif
};
