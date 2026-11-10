#include "app/TargetWindowDetailsController.h"
#include "app/MainWindow.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/WorkspaceHistoryActions.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/blocks/WaitBlock.h"
#include "ui/TargetWindowDetailPanel.h"
#include "ui/TargetPanelController.h"
#include <QToolButton>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QThread>
#include <QTimer>
#include <iostream>

class TargetWindowDetailsUiChecks {
public:
static bool run(const QString& folder) {
    if (QApplication::platformName() != QStringLiteral("offscreen")) return false;
#ifdef _WIN32
    using Controller = TargetWindowDetailsController;
    const HWND marker = reinterpret_cast<HWND>(quintptr(1));
    Controller::WindowAccess access{
        [=](HWND hwnd) { return hwnd == marker; },
        [](HWND, ScreenCapture::TargetWindowInfo& info) {
            info.found = true; info.hwndValue = 1; info.visible = true;
            info.title = L"샘플 메인 창 <A&B> — 긴 제목으로 표시와 줄임 상태 확인";
            info.className = L"SampleWindow"; info.processPath = L"C:\\sample\\example.exe";
            info.x = -1904; info.y = -1064; info.width = 1280; info.height = 820;
            info.clientWidth = 1260; info.clientHeight = 780;
            info.monitorNumber = 2; info.monitorWidth = 1920; info.monitorHeight = 1080; info.monitorDpi = 120;
            return true;
        },
        [=](const QString&, const QString&) -> HWND { return marker; },
        [](const QString&, HWND, const Controller::Bindings&, bool sub) { return !sub; }};
    Controller controller(access);
    Controller::Request request;
    request.globalDefault = false; request.captureOwnedBySession = true; request.foreground = marker;
    request.bindings.mainTitle = QStringLiteral("샘플"); request.bindings.mainProcessPath = "C:\\sample\\example.exe";
    auto result = controller.refresh(request);
    if (result.state != Controller::Result::State::Live) return false;
    TargetWindowDetailPanel panel; panel.resize(540,180); panel.show();
    panel.showDetails(result.data); QApplication::processEvents();
    auto* title = panel.findChild<QLabel*>(QStringLiteral("targetWindowDetailTitle"));
    auto* state = panel.findChild<QLabel*>(QStringLiteral("targetWindowDetailStatus"));
    auto* primary = panel.findChild<QLabel*>(QStringLiteral("targetWindowDetailPrimary"));
    auto* secondary = panel.findChild<QLabel*>(QStringLiteral("targetWindowDetailSecondary"));
    if (!title || !state || !primary || !secondary || !title->toolTip().contains(QStringLiteral("<A&B>"))
        || !primary->text().contains("example.exe") || !secondary->text().contains("125%")
        || !panel.grab().save(QDir(folder).filePath("ui-target-live.png"))) return false;
    panel.resize(270,240); QApplication::processEvents();
    if (title->geometry().intersects(state->geometry())
        || !panel.grab().save(QDir(folder).filePath("ui-target-narrow.png"))) return false;
    result.data.subTarget = true; panel.showDetails(result.data);
    if (!state->text().contains(QStringLiteral("서브"))) return false;
    request.foreground = nullptr;
    auto noLookup = access;
    noLookup.findVisible = [](const QString&, const QString&) -> HWND { return nullptr; };
    controller.m_windows = noLookup;
    result = controller.refresh(request);
    if (result.state != Controller::Result::State::Stored) return false;
    panel.showStoredTargetBinding(result.storedTitle, "example.exe", result.storedProcessPath);
    QApplication::processEvents();
    if (!state->text().contains(QStringLiteral("미실행"))
        || !panel.grab().save(QDir(folder).filePath("ui-target-stored.png"))) return false;
    panel.showStoredTargetBinding(QStringLiteral("샘플 서브 창"), "example.exe", result.storedProcessPath, true);
    panel.resize(540,180); QApplication::processEvents();
    if (!state->text().contains(QStringLiteral("서브"))) return false;
    panel.resize(270,240); QApplication::processEvents();
    if (!state->text().contains(QStringLiteral("서브"))
        || !panel.grab().save(QDir(folder).filePath("ui-target-stored-sub.png"))) return false;

    {
        TargetWindowDetailPanel view;view.resize(360,180);view.show();QWidget buttons;
        TargetPanelController::Controls controls{new QToolButton(&buttons),new QToolButton(&buttons),new QToolButton(&buttons),new QToolButton(&buttons),
            new QToolButton(&buttons),new QToolButton(&buttons),new QToolButton(&buttons),new QToolButton(&buttons)};
        Controller details(access);Controller::Request source;source.globalDefault=false;source.profileId="one";source.revision=10;
        int snapshots=0;auto presenter=std::make_unique<TargetPanelController>(view,controls,details,[&]{++snapshots;return source;});
        auto* timer=presenter->findChild<QTimer*>();if(!timer || timer->interval()!=250)return false;
        presenter->applyControls(true);
        for(const auto& button:{controls.pickMain,controls.listMain,controls.pickSub,controls.listSub,controls.showMain,controls.showSub,controls.pinMain,controls.pinSub})
            if(button->isEnabled() || !button->toolTip().contains(QStringLiteral("전역 기본")))return false;
        presenter->applyControls(false);
        if(!controls.pickMain->isEnabled() || !controls.pickSub->isEnabled() || !controls.showMain->isEnabled()
            || !controls.pickMain->toolTip().contains(QStringLiteral("초록색")) || !controls.pickSub->toolTip().contains(QStringLiteral("파란색")))return false;
        presenter->refresh();auto* message=view.findChild<QLabel*>("targetWindowDetailMessage");
        if(!message || !message->text().contains(QStringLiteral("'지정' 버튼")) || !view.grab().save(QDir(folder).filePath("ui-target-panel-unbound.png")))return false;
        source.bindings.mainTitle=QStringLiteral("샘플");source.bindings.mainProcessPath="C:\\sample\\example.exe";source.foreground=marker;
        Controller::Host host;host.current=[&](const auto& request){return presenter && presenter->current(request);};
        host.synchronizeIdleCapture=[&]{++source.revision;};details.setHost(host);view.showMessage("preserved");presenter->refresh();
        if(message->text()!="preserved")return false;
        source.globalDefault=true;presenter->refresh();
        if(controls.pickMain->isEnabled())return false;
        presenter->stop();const int before=snapshots;QMetaObject::invokeMethod(timer,"timeout",Qt::QueuedConnection);QApplication::processEvents();
        if(timer->isActive() || snapshots!=before)return false;
        for(int i=0;i<10;++i)presenter->start();
        if(presenter->findChildren<QTimer*>().size()!=1)return false;
        timer->setInterval(1);QElapsedTimer deadline;deadline.start();
        while(snapshots<before+8 && deadline.elapsed()<1500){QApplication::processEvents();QThread::msleep(1);}
        if(snapshots<before+8)return false;
        source.globalDefault=false;host.synchronizeIdleCapture=[&]{presenter.reset();};details.setHost(host);
        presenter->refresh();if(presenter)return false;
    }
    {
        auto view=std::make_unique<TargetWindowDetailPanel>();Controller details(access);int snapshots=0;
        TargetPanelController presenter(*view,{},details,[&]{++snapshots;return Controller::Request{};});auto* timer=presenter.findChild<QTimer*>();
        view.reset();presenter.refresh();if(!timer || timer->isActive() || snapshots)return false;
    }
    const bool oldFocus = ProgramSettings::focusTargetWindowOnProfileSelect();
    ProgramSettings::setFocusTargetWindowOnProfileSelect(false);
    struct Restore { bool focus; ~Restore() { ProgramSettings::setFocusTargetWindowOnProfileSelect(focus); } } restore{oldFocus};
    {
        MainWindow window;
        window.m_foregroundMonitor->stop();
        ProfileSwitchCoordinator::HostCallbacks quiet;
        quiet.isWorkspacePinned = [] { return true; };
        quiet.flushDeferredProfileSwitchIfIdle = [] {};
        window.m_profileSwitchCoordinator.setHostCallbacks(quiet);
        const QString id = window.m_profileManager->addProfile(QStringLiteral("상세 표시 검사"),false);
        Project project; project.setTargetWindowTitle("샘플"); project.addFeature("대기 기능");
        if (id.isEmpty() || !JsonSerializer::saveToFile(project,window.m_profileManager->projectPath(id),window.m_profileManager->projectDirectory(id))
            || !window.m_profileManager->updateProfileTargetBinding(id,QStringLiteral("샘플"),"C:\\sample\\example.exe")) return false;
        window.m_targetDetails.m_windows = access;
        if (!window.executeProfileSwitch(id,false)) return false;
        QElapsedTimer deadline; deadline.start();
        while (window.m_profileSwitchPipelineActive && deadline.elapsed() < 4000) { QApplication::processEvents(); QThread::msleep(1); }
        if (window.m_profileSwitchPipelineActive) return false;
        FeatureRunSession session; session.featureId = "detail-capture-owner"; session.profileId = id;
        session.engine = std::make_unique<WorkflowEngine>(); session.sessionContext = std::make_shared<ExecutionContext>();
        auto workflow = std::make_shared<Workflow>(); auto wait = std::make_unique<WaitBlock>(); wait->ms = 5000;
        workflow->addBlock(std::move(wait)); session.engine->run(workflow,session.sessionContext);
        auto* engine = session.engine.get(); window.runSessions().emplace(session.featureId,std::move(session));
        ScreenCapture::setTargetWindowTitle(L"running-main-sentinel");
        ScreenCapture::setSubTargetWindowTitle(L"running-sub-sentinel"); ScreenCapture::setTargetWindow(nullptr);
        const bool running = window.hasAnyRunningSession();
        window.updateTargetWindowDetails();
        auto* liveTitle = window.m_targetWindowDetailPanel->findChild<QLabel*>(QStringLiteral("targetWindowDetailTitle"));
        const bool coherent = running && liveTitle && liveTitle->toolTip().contains(QStringLiteral("샘플 메인"))
            && ScreenCapture::targetWindowTitle() == L"running-main-sentinel"
            && ScreenCapture::subTargetWindowTitle() == L"running-sub-sentinel"
            && ScreenCapture::targetWindow() == nullptr;
        engine->stopAndWaitBounded(1000); window.runSessions().erase("detail-capture-owner");
        bool historyObserved = false, historyPreserved = false;
        QTimer::singleShot(0,&window,[&] {
            if (!window.m_workspaceHistory->isBusy()) return;
            historyObserved = true;
            const auto mainBefore = ScreenCapture::targetWindowTitle();
            const auto subBefore = ScreenCapture::subTargetWindowTitle();
            const auto hwndBefore = ScreenCapture::targetWindow();
            window.updateTargetWindowDetails();
            historyPreserved = ScreenCapture::targetWindowTitle() == mainBefore
                && ScreenCapture::subTargetWindowTitle() == subBefore && ScreenCapture::targetWindow() == hwndBefore;
        });
        const bool recorded = coherent && window.pushGlobalUiUndoSnapshot("target-details-guard");
        const bool shutdown = recorded && historyObserved && historyPreserved && window.prepareForShutdown();
        if (!shutdown) {
            std::cerr << "Target details integration FAILED: engine=" << coherent << " snapshot=" << recorded
                << " busy-observed=" << historyObserved << " detail-preserved=" << historyPreserved << '\n';
            return false;
        }
        // Quiescing sessions and queued foreground work may legitimately change the idle target.
        // Compare immediately around the detail refresh, rather than across those other actions.
        const auto readyMain = ScreenCapture::targetWindowTitle(), readySub = ScreenCapture::subTargetWindowTitle();
        const auto readyHwnd = ScreenCapture::targetWindow();
        if(!window.m_targetPanel || window.m_targetPanel->findChild<QTimer*>()->isActive())return false;
        window.updateTargetWindowDetails();
        if (ScreenCapture::targetWindowTitle() != readyMain || ScreenCapture::subTargetWindowTitle() != readySub
            || ScreenCapture::targetWindow() != readyHwnd) {
            std::cerr << "Target details integration FAILED: detail refresh mutated shutdown-ready capture\n";
            return false;
        }
    }
    std::cout << "Target panel: owned button policy/help, current-source/lifetime cancellation, one live Qt timer, queued-stop/deleted-view and controller deletion during refresh OK (offscreen; synthetic metadata; no native input)\n";
    std::cout << "Target details UI: controller-to-panel live/stored/sub states, negative coordinates, 125% DPI and narrow title/status separation OK (offscreen; synthetic metadata; no native input)\n";
    std::cout << "Target details MainWindow: real Wait engine owns capture, history I/O/shutdown keep detail refresh read-only, main/sub/HWND sentinels retained, stored sub role survives adaptive resize OK (offscreen; synthetic metadata; no native activation/input)\n";
#endif
    return true;
}
};
bool runTargetWindowDetailsUiChecks(const QString& folder) { return TargetWindowDetailsUiChecks::run(folder); }
