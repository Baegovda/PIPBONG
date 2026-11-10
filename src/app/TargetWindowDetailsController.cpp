#include "TargetWindowDetailsController.h"
#include "app/ForegroundRunGate.h"
#include <QCoreApplication>
#include <map>
#include <optional>

#ifdef _WIN32
namespace {
QString tr(const char* text) { return QCoreApplication::translate("TargetWindowDetailPanel", text); }
TargetWindowDetailData formatWindowInfo(const ScreenCapture::TargetWindowInfo& info, bool subTarget) {
    const QString title = QString::fromStdWString(info.title);
    const QString className = QString::fromStdWString(info.className);
    QString processPath = QString::fromStdWString(info.processPath);
    const int slash = processPath.lastIndexOf(QLatin1Char('\\'));
    const QString processName =
        slash >= 0 ? processPath.mid(slash + 1)
                   : (processPath.isEmpty() ? tr("알 수 없음") : processPath);

    QString stateText;
    if (info.minimized) {
        stateText = tr("● 최소화");
    } else if (info.visible) {
        stateText = tr("● 표시");
    } else {
        stateText = tr("● 숨김");
    }

    QString monitorText;
    if (info.monitorNumber > 0 && info.monitorWidth > 0 && info.monitorHeight > 0) {
        if (info.monitorDpi > 0) {
            const int scalePercent = qRound(info.monitorDpi * 100.0 / 96.0);
            monitorText = tr("%1번 · %2×%3 · %4%")
                              .arg(info.monitorNumber)
                              .arg(info.monitorWidth)
                              .arg(info.monitorHeight)
                              .arg(scalePercent);
        } else {
            monitorText = tr("%1번 · %2×%3")
                              .arg(info.monitorNumber)
                              .arg(info.monitorWidth)
                              .arg(info.monitorHeight);
        }
    } else {
        monitorText = tr("알 수 없음");
    }

    TargetWindowDetailData data;
    data.hwnd = QStringLiteral("0x%1").arg(info.hwndValue, 0, 16);
    data.title = title.isEmpty() ? tr("(제목 없음)") : title;
    data.className = className.isEmpty() ? tr("(없음)") : className;
    data.frameBounds =
        tr("%1×%2 @ (%3, %4)").arg(info.width).arg(info.height).arg(info.x).arg(info.y);
    data.clientSize = tr("%1×%2 px").arg(info.clientWidth).arg(info.clientHeight);
    data.monitor = monitorText;
    data.processName = processName;
    data.processPath = processPath;
    data.stateText = stateText;
    data.subTarget = subTarget;
    data.minimized = info.minimized;
    data.visible = info.visible;
    data.monitorDpi = info.monitorDpi;
    return data;
}
}

TargetWindowDetailsController::TargetWindowDetailsController()
    : TargetWindowDetailsController(WindowAccess{
        [](HWND hwnd) { return hwnd && IsWindow(hwnd); },
        [](HWND hwnd, ScreenCapture::TargetWindowInfo& info) { return ScreenCapture::queryWindowInfo(hwnd, info); },
        [](const QString& title, const QString& path) { return ForegroundRunGate::findVisibleTopLevelWindowHwnd(title, path); },
        [](const QString& title, HWND hwnd, const Bindings& bindings, bool sub) {
            return sub
                ? ForegroundRunGate::foregroundMatchesScopedSubTarget(title, hwnd, bindings.mainTitle,
                    bindings.subTitle, bindings.mainProcessPath, bindings.subProcessPath)
                : ForegroundRunGate::foregroundMatchesScopedMainTarget(title, hwnd, bindings.mainTitle,
                    bindings.subTitle, bindings.mainProcessPath, bindings.subProcessPath);
        }}) {}

TargetWindowDetailsController::TargetWindowDetailsController(WindowAccess windows)
    : m_windows(std::move(windows)) {}
#endif

TargetWindowDetailsController::Result TargetWindowDetailsController::refresh(const Request& request) {
    if(m_refreshing)return {};
    m_refreshing=true;const std::weak_ptr<int> lifetime=m_lifetime;
    struct Reset{std::weak_ptr<int> life;bool* busy;~Reset(){if(!life.expired())*busy=false;}} reset{lifetime,&m_refreshing};
    const auto host=m_host;
#ifdef _WIN32
    const auto windows=m_windows;
#endif
    auto expected=request;
    const auto valid=[&]{if(lifetime.expired())return false;const bool current=!host.current || host.current(expected);return current && !lifetime.expired();};
    if(!valid())return {};
    Result result;
    if (request.globalDefault) {
        result.state = Result::State::GlobalDefault;
        return result;
    }
#ifdef _WIN32
    const auto& bindings = request.bindings;
    if (bindings.mainTitle.isEmpty() && bindings.subTitle.isEmpty()) return result;
    result.state = Result::State::Stored;
    result.subTarget = bindings.mainTitle.isEmpty();
    result.storedTitle = result.subTarget ? bindings.subTitle : bindings.mainTitle;
    result.storedProcessPath = result.subTarget ? bindings.subProcessPath : bindings.mainProcessPath;

    // Detail refresh must never rewrite shared capture bindings while a session owns them.
    const bool mayUpdateIdle = !request.captureOwnedBySession && request.allowIdleUpdates;
    if (mayUpdateIdle && host.synchronizeIdleCapture) host.synchronizeIdleCapture();
    if(!valid())return result;
    std::map<HWND, std::optional<ScreenCapture::TargetWindowInfo>> snapshots;
    const auto read = [&](HWND hwnd) -> const ScreenCapture::TargetWindowInfo* {
        if (!valid() || !hwnd) return nullptr;
        const auto cached = snapshots.find(hwnd);
        if (cached != snapshots.end()) return cached->second ? &*cached->second : nullptr;
        auto [entry, inserted] = snapshots.emplace(hwnd, std::nullopt);
        if (inserted && windows.exists && windows.exists(hwnd) && valid()) {
            ScreenCapture::TargetWindowInfo info;
            if (windows.read && windows.read(hwnd, info) && valid()) entry->second = std::move(info);
        }
        return entry->second ? &*entry->second : nullptr;
    };
    const auto classify = [&](HWND hwnd) -> std::optional<bool> {
        const auto* info = read(hwnd);
        if (!info || !windows.matchesScope) return std::nullopt;
        const QString title = QString::fromStdWString(info->title).trimmed();
        const QString path = QString::fromStdWString(info->processPath);
        // A remembered HWND may be stale, reused, or belong to another profile. A configured
        // executable is authoritative, including when reading the live path failed.
        for (bool sub : {true, false}) {
            const auto& binding = sub ? bindings.subTitle : bindings.mainTitle;
            const auto& expectedPath = sub ? bindings.subProcessPath : bindings.mainProcessPath;
            if (binding.isEmpty() || (!expectedPath.isEmpty()
                && path.compare(expectedPath, Qt::CaseInsensitive) != 0)) continue;
            if (valid() && windows.matchesScope(title, hwnd, bindings, sub) && valid()) return sub;
        }
        return std::nullopt;
    };
    const auto accept = [&](HWND hwnd, bool requireVisible = false) {
        const auto role = classify(hwnd);
        const auto* info = read(hwnd);
        if (!role || !info || (requireVisible && !info->visible)) return false;
        result.hwnd = hwnd; result.subTarget = *role; result.info = *info;
        result.state = Result::State::Live;
        return true;
    };
    bool selected = !request.pipbongForeground && accept(request.foreground);
    if (!selected && request.pipbongForeground) selected = accept(request.remembered);
    if (!selected) selected = accept(request.hint);
    if (!selected && windows.findVisible) {
        const auto find = [&](const QString& title, const QString& path) {
            if(title.isEmpty() || !valid())return HWND(nullptr);
            const auto found=windows.findVisible(title,path);return valid()?found:nullptr;
        };
        HWND main = find(bindings.mainTitle, bindings.mainProcessPath);
        HWND sub = find(bindings.subTitle, bindings.subProcessPath);
        // Validate lookup results as well; a process may exit/change between enumeration and read.
        const auto mainRole = classify(main), subRole = classify(sub);
        const auto* mainInfo=mainRole?read(main):nullptr;
        const auto* subInfo=subRole?read(sub):nullptr;
        if (!mainInfo || !mainInfo->visible) main = nullptr;
        if (!subInfo || !subInfo->visible) sub = nullptr;
        HWND preferred = sub ? sub : main;
        if (main && sub && main != sub) {
            if (mainInfo->minimized != subInfo->minimized)
                preferred = mainInfo->minimized ? sub : main;
            else if (request.remembered == main || request.remembered == sub)
                preferred = request.remembered;
        }
        selected = accept(preferred, true);
    }
    if (!selected) selected = accept(request.remembered);
    if (!selected || !valid()) return result;

    result.data = formatWindowInfo(result.info, result.subTarget);
    if (mayUpdateIdle) {
        if (host.applyIdleTarget) host.applyIdleTarget(result.hwnd, result.subTarget);
        if(!valid())return result;
        const QString path = QString::fromStdWString(result.info.processPath);
        const auto& oldPath = result.subTarget ? bindings.subProcessPath : bindings.mainProcessPath;
        const auto& title = result.subTarget ? bindings.subTitle : bindings.mainTitle;
        if (!path.isEmpty() && path.compare(oldPath, Qt::CaseInsensitive) != 0 && host.updateProcessPath) {
            const bool saved=host.updateProcessPath(request.profileId,title,path,result.subTarget);
            if(saved)(result.subTarget?expected.bindings.subProcessPath:expected.bindings.mainProcessPath)=path;
            if(!valid())return result;
            if(saved && !result.subTarget && !request.deferProfileRefresh && host.refreshProfiles)host.refreshProfiles();
            if(!valid())return result;
        }
    }
    if(!valid())return result;
    if (result.info.clientWidth > 0 && result.info.clientHeight > 0
        && (result.info.clientWidth != m_clientWidth || result.info.clientHeight != m_clientHeight)) {
        m_clientWidth = result.info.clientWidth; m_clientHeight = result.info.clientHeight;
        if (host.workflowScaleChanged) host.workflowScaleChanged();
    }
#else
    result.state = Result::State::Unsupported;
#endif
    return result;
}
