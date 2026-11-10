#include "app/TargetWindowDetailsController.h"
#include <iostream>
#include <map>

namespace {
bool check(bool value, const char* message) {
    if (!value) std::cerr << "Target details FAILED: " << message << '\n';
    return value;
}
#ifdef _WIN32
using Controller = TargetWindowDetailsController;
using State = Controller::Result::State;
HWND marker(int value) { return reinterpret_cast<HWND>(quintptr(value)); }
struct Fixture {
    std::map<HWND, ScreenCapture::TargetWindowInfo> windows;
    std::map<HWND, int> reads;
    HWND main = nullptr, sub = nullptr;
    int idle = 0, applied = 0, saved = 0, lists = 0, scaled = 0;
    bool saveSuccess = true;
    Controller::WindowAccess access() {
        return {
            [this](HWND hwnd) { return windows.count(hwnd) != 0; },
            [this](HWND hwnd, ScreenCapture::TargetWindowInfo& info) {
                ++reads[hwnd]; info = windows.at(hwnd); return info.found;
            },
            [this](const QString& title, const QString&) { return title == "Game" ? main : sub; },
            [this](const QString& title, HWND hwnd, const Controller::Bindings& b, bool isSub) {
                const auto& binding = isSub ? b.subTitle : b.mainTitle;
                const auto& path = isSub ? b.subProcessPath : b.mainProcessPath;
                // Deliberately permissive title fallback: production controller must reject a
                // different configured process even if the scope matcher accepts its title.
                return !binding.isEmpty() && (title.contains(binding, Qt::CaseInsensitive)
                    || (!path.isEmpty() && path.compare(QString::fromStdWString(windows.at(hwnd).processPath), Qt::CaseInsensitive) == 0));
            }};
    }
    Controller::Host host() {
        return {[this] { ++idle; }, [this](HWND, bool) { ++applied; },
            [this](const QString&, const QString&, const QString&, bool) { ++saved; return saveSuccess; },
            [this] { ++lists; }, [this] { ++scaled; }};
    }
    void add(int id, const wchar_t* title, const wchar_t* path) {
        auto& info = windows[marker(id)]; info.found = true; info.hwndValue = id;
        info.title = title; info.processPath = path; info.visible = true;
        info.width = 1280; info.height = 720; info.x = -1904; info.y = -1064;
        info.clientWidth = 1200; info.clientHeight = 680;
        info.monitorNumber = 2; info.monitorWidth = 1920; info.monitorHeight = 1080; info.monitorDpi = 120;
    }
};
#endif
}

bool runTargetWindowDetailsChecks() {
#ifdef _WIN32
    Fixture f;
    f.add(1,L"Game live",L"C:\\game.exe"); f.add(2,L"Launcher live",L"C:\\launcher.exe");
    f.add(3,L"Game live",L"C:\\other.exe"); f.add(4,L"Game live",L"");
    Controller controller(f.access()); controller.setHost(f.host());
    Controller::Request request; request.profileId = "profile-one";
    request.bindings = {"Game", "Launcher", "C:\\game.exe", "C:\\launcher.exe"};
    request.foreground = marker(1);
    if (!check(controller.refresh(request).state == State::GlobalDefault && f.reads.empty() && !f.idle,
        "global profile performs no native reads or capture synchronization")) return false;
    request.globalDefault = false;
    const auto bindings = request.bindings; request.bindings = {};
    if (!check(controller.refresh(request).state == State::Unbound && f.reads.empty() && !f.idle,
        "unbound profile cannot adopt ambient windows")) return false;
    request.bindings = bindings;
    auto result = controller.refresh(request);
    if (!check(result.state == State::Live && result.hwnd == marker(1) && !result.subTarget
        && result.data.frameBounds == QStringLiteral("1280×720 @ (-1904, -1064)")
        && result.data.monitor == QStringLiteral("2번 · 1920×1080 · 125%")
        && result.data.processName == "game.exe" && f.idle == 1 && f.applied == 1
        && !f.saved && f.scaled == 1 && f.reads[marker(1)] == 1,
        "live main selection, formatting, one read per HWND and initial scale update")) return false;
    controller.refresh(request);
    if (!check(f.scaled == 1, "unchanged client size does not refresh editor")) return false;

    request.pipbongForeground = true; request.foreground = nullptr; request.remembered = marker(3);
    f.main = marker(1); f.sub = nullptr;
    result = controller.refresh(request);
    if (!check(result.hwnd == marker(1), "remembered same-title wrong process is rejected after profile change/reuse")) return false;
    f.main = nullptr;
    result = controller.refresh(request);
    if (!check(result.state == State::Stored && result.storedTitle == "Game" && result.storedProcessPath == "C:\\game.exe",
        "stale remembered handle cannot replace saved binding")) return false;
    request.remembered = marker(4); request.hint = marker(3); f.main = marker(3);
    if (!check(controller.refresh(request).state == State::Stored,
        "unreadable configured process, wrong-process hint and invalid lookup are rejected")) return false;
    request.remembered = marker(2); request.hint = nullptr; f.main = nullptr;
    result = controller.refresh(request);
    if (!check(result.state == State::Live && result.subTarget && result.data.subTarget,
        "remembered window role is reclassified against current bindings")) return false;

    request.pipbongForeground = false; request.remembered = nullptr; f.main = marker(1); f.sub = marker(2);
    f.windows[marker(1)].minimized = true;
    if (!check(controller.refresh(request).hwnd == marker(2), "visible non-minimized target wins fallback")) return false;
    f.windows[marker(1)].minimized = false; request.remembered = marker(1);
    if (!check(controller.refresh(request).hwnd == marker(1), "remembered current target breaks equivalent fallback tie")) return false;
    request.remembered = nullptr; f.windows[marker(2)].visible = false;
    if (!check(controller.refresh(request).hwnd == marker(1), "hidden enumerated window is rejected")) return false;

    request.foreground = marker(1); request.captureOwnedBySession = true;
    request.bindings.mainProcessPath.clear();
    const int idle = f.idle, applied = f.applied, saved = f.saved;
    if (!check(controller.refresh(request).state == State::Live && f.idle == idle && f.applied == applied && f.saved == saved,
        "running capture owner blocks every idle capture mutation and binding save")) return false;
    request.captureOwnedBySession = false;
    request.allowIdleUpdates = false;
    if (!check(controller.refresh(request).state == State::Live && f.idle == idle && f.applied == applied && f.saved == saved,
        "workspace transaction gate preserves read-only detail rendering without idle writes")) return false;
    request.allowIdleUpdates = true;
    result = controller.refresh(request);
    if (!check(result.state == State::Live && f.saved == saved + 1 && f.lists == 1,
        "missing process path can be learned for an existing binding while idle")) return false;
    request.deferProfileRefresh = true; controller.refresh(request);
    if (!check(f.lists == 1, "deferred profile pipeline suppresses list refresh")) return false;
    request.deferProfileRefresh = false; f.saveSuccess = false; controller.refresh(request);
    if (!check(f.lists == 1, "failed binding save does not report profile list refresh")) return false;
    request.bindings = bindings; request.foreground = marker(1);
    f.windows[marker(1)].processPath = L"c:\\GAME.exe";
    if (!check(controller.refresh(request).state == State::Live, "process paths compare case insensitively")) return false;
    f.windows[marker(1)].found = false; f.main = nullptr; f.sub = nullptr;
    if (!check(controller.refresh(request).state == State::Stored, "window disappearing or unreadable between ticks retains saved display")) return false;

    for(int phase=0;phase<6;++phase) {
        Fixture graph;graph.add(1,L"Game live",L"C:\\game.exe");auto access=graph.access();
        std::unique_ptr<Controller> owner;int after=0;bool reached=false;
        const auto visit=[&](int step){if(!owner)++after;if(step==phase){reached=true;owner.reset();}};
        const auto read=access.read;access.read=[&](HWND h,ScreenCapture::TargetWindowInfo& info){const bool ok=read(h,info);visit(1);return ok;};
        owner=std::make_unique<Controller>(access);Controller::Host host;
        host.synchronizeIdleCapture=[&]{visit(0);};host.applyIdleTarget=[&](HWND,bool){visit(2);};
        host.updateProcessPath=[&](const auto&,const auto&,const auto&,bool){visit(3);return true;};
        host.refreshProfiles=[&]{visit(4);};host.workflowScaleChanged=[&]{visit(5);};owner->setHost(host);
        Controller::Request source;source.globalDefault=false;source.profileId="owned";source.foreground=marker(1);source.bindings.mainTitle="Game";
        owner->refresh(source);
        if(!check(reached && !owner && after==0,"controller destruction in idle/read/apply/save/list/scale callback stops all later callbacks"))return false;
    }
    {
        Fixture graph;graph.add(1,L"Game live",L"C:\\game.exe");Controller guarded(graph.access());
        Controller::Request source;source.globalDefault=false;source.profileId="one";source.revision=1;source.foreground=marker(1);source.bindings.mainTitle="Game";
        auto now=source;auto host=graph.host();host.current=[&](const auto& q){return q.profileId==now.profileId && q.revision==now.revision && q.bindings.mainProcessPath==now.bindings.mainProcessPath;};
        host.synchronizeIdleCapture=[&]{++graph.idle;guarded.refresh(source);now.revision=2;};guarded.setHost(host);guarded.refresh(source);
        if(!check(graph.idle==1 && graph.reads.empty() && !graph.applied && !graph.saved,"nested refresh is coalesced and source replacement prevents later reads/writes"))return false;
        now=source;host.synchronizeIdleCapture=[&]{++graph.idle;};
        host.updateProcessPath=[&](const auto&,const auto&,const auto& path,bool){++graph.saved;now.bindings.mainProcessPath=path;return true;};
        guarded.setHost(host);guarded.refresh(source);
        if(!check(graph.saved==1 && graph.lists==1 && graph.scaled==1,"own successful process-path healing updates expected source before list/scale callbacks"))return false;
    }
    // Positive control against a real, already existing window. No activation, movement or input.
    HWND foreground = GetForegroundWindow();
    ScreenCapture::TargetWindowInfo native;
    if (foreground && ScreenCapture::queryWindowInfo(foreground, native) && !native.processPath.empty()) {
        const auto oldTarget = ScreenCapture::targetWindow();
        const auto oldTitle = ScreenCapture::targetWindowTitle(), oldSub = ScreenCapture::subTargetWindowTitle();
        Controller nativeController; Controller::Request live;
        live.globalDefault = false; live.captureOwnedBySession = true; live.foreground = foreground;
        live.bindings.mainTitle = native.title.empty() ? QStringLiteral("native read control") : QString::fromStdWString(native.title);
        live.bindings.mainProcessPath = QString::fromStdWString(native.processPath);
        const auto detail = nativeController.refresh(live);
        if (!check(detail.state == State::Live && detail.hwnd == foreground
            && detail.info.processPath == native.processPath && ScreenCapture::targetWindow() == oldTarget
            && ScreenCapture::targetWindowTitle() == oldTitle && ScreenCapture::subTargetWindowTitle() == oldSub,
            "default native reader selects real foreground without changing capture state")) return false;
        std::cout << "Target details native read: existing foreground HWND and process match OK (no activation/move/input)\n";
    } else {
        std::cout << "Target details native read: unavailable foreground metadata; positive control not verified\n";
    }
    std::cout << "Target details: stale/reused HWND, configured-process mismatch, unreadable/hidden targets, main/sub fallback, owned capture isolation, save/defer guards and formatting OK (synthetic window graph)\n";
#endif
    return true;
}
