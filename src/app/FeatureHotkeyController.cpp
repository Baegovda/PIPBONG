#include "app/FeatureHotkeyController.h"
#include "app/FeatureRunSession.h"
#include "core/diagnostics/AppStutterProfiler.h"
#include "model/Feature.h"
#include "model/Project.h"
#include <QCoreApplication>
#include <QPointer>

namespace {
using Controller=FeatureHotkeyController;
bool same(const Controller::Workspace& a,const Controller::Workspace& b) {
    return a.project==b.project && a.owner==b.owner && a.generation==b.generation;
}
Feature* feature(const Controller::Workspace& source,const QString& id) {
    return source.project?source.project->featureById(id.toStdString()):nullptr;
}
struct Burst {
    QPointer<Controller> life;
    std::function<void()> leave;
    Burst(Controller* controller,const Controller::Host& host):life(controller),leave(host.leaveBurst) {
        if(host.enterBurst)host.enterBurst();
    }
    ~Burst(){if(life && leave)leave();}
};
QString hotkeyText(const char* text){return QCoreApplication::translate("MainWindow",text);}
}

Controller::Block Controller::block(const Gate& value) {
    if(value.unavailable)return Block::Unavailable;
    if(value.editor)return Block::Editor;
    if(value.targetRequired && !value.profileMatches && !value.backgroundAllowed)return Block::Target;
    return value.ownWindow?Block::Target:Block::None;
}

void Controller::triggered(const QString& id) {
    const Host host=m_host;const QPointer<Controller> life(this);
    const auto source=host.workspace?host.workspace():Workspace{};
    const auto* selected=feature(source,id);
    if(!selected || !selected->enabled())return;
    const auto denied=block(host.gate?host.gate(id):Gate{});
    if(!life)return;
    if(denied!=Block::None){if(host.blocked)host.blocked(denied);return;}
    if(!host.workspace || !same(source,host.workspace()))return;
    selected=feature(host.workspace(),id);
    if(!selected || !selected->enabled())return;
    if(selected->runMode()==FeatureRunMode::Hold)return;
    const auto key=id.toStdString();
    if(host.running && host.running(key)){if(host.stop)host.stop(key);return;}
    if(host.foregroundReady)host.foregroundReady();
    if(!life || !host.workspace || !same(source,host.workspace()))return;
    // Foreground reconciliation can replace/delete a feature even within this document.
    selected=feature(host.workspace(),id);
    if(!selected || !selected->enabled() || selected->runMode()==FeatureRunMode::Hold)return;
    const auto after=block(host.gate?host.gate(id):Gate{});
    if(!life || !same(source,host.workspace()))return;
    if(after!=Block::None){if(host.blocked)host.blocked(after);return;}
    if(host.start)host.start(key);
}

void Controller::holdStarted(const QString& id) {
    const Host host=m_host;const QPointer<Controller> life(this);Burst burst(this,host);
    if(!life)return;
    const auto source=host.workspace?host.workspace():Workspace{};
    const auto* selected=feature(source,id);
    if(!selected || !selected->enabled())return;
    const auto denied=block(host.gate?host.gate(id):Gate{});
    if(!life)return;
    if(denied!=Block::None){if(host.blocked)host.blocked(denied);return;}
    if(!host.workspace || !same(source,host.workspace()))return;
    selected=feature(host.workspace(),id);
    if(!selected || !selected->enabled())return;
    if(selected->runMode()!=FeatureRunMode::Hold)return;
    const auto key=id.toStdString();
    if(auto* session=host.session?host.session(key):nullptr) {
        if(session->holdRunActive) {
            if(!session->holdKeyTapLaneActive && (!session->engine || !session->engine->isRunning())) {
                session->repeatSession=true;if(host.repeat)host.repeat(*session);
            }
            return;
        }
        if(host.holdBlocked && host.holdBlocked(*session))return;
        if(host.erase)host.erase(key);
        if(!life)return;
    }
    if(!host.workspace || !same(source,host.workspace()))return;
    m_pending.insert(id,source);
    if(host.queueStart)host.queueStart(key);
    if(life && host.holdVisuals)host.holdVisuals();
}

void Controller::holdEnded(const QString& id) {
    const Host host=m_host;const QPointer<Controller> life(this);Burst burst(this,host);
    if(!life)return;
    const auto key=id.toStdString();
    m_pending.remove(id);
    if(host.cancelStart)host.cancelStart(key);
    if(!life)return;
    auto* session=host.session?host.session(key):nullptr;
    if(!session || session->runningMode!=FeatureRunMode::Hold) {
        if(host.holdVisuals)host.holdVisuals();return;
    }
    ++session->holdRepeatGeneration;
    if(!session->holdRunActive){if(host.holdVisuals)host.holdVisuals();return;}
    session->holdRunActive=false;session->repeatSession=false;
    const auto* released=session;
    const auto context=session->sessionContext;const auto* engine=session->engine.get();
    const auto owner=session->profileId;const auto generation=session->holdRepeatGeneration;
    // Release by started session ownership even if the active feature/profile changed.
    if(host.release)host.release(*session);
    if(!life)return;
    if(host.holdVisuals)host.holdVisuals();
    if(!life)return;
    session=host.session?host.session(key):nullptr;
    if(!session || session!=released || session->runningMode!=FeatureRunMode::Hold
        || session->sessionContext!=context || session->engine.get()!=engine || session->profileId!=owner
        || session->holdRepeatGeneration!=generation || session->holdRunActive)return;
    if(session->holdKeyTapLaneActive && host.stopLane) {
        session->userStopRequested=true;host.stopLane(*session);return;
    }
    if(session->engine && session->engine->isRunning()) {
        session->userStopRequested=true;session->engine->stop();return;
    }
    session->userStopRequested=false;
    if(host.queueFinish)host.queueFinish(key);
}

void Controller::sync() {
    AppStutterOperationScope scope("hotkey.sync",QStringLiteral("project sync"));
    const Host host=m_host;const QPointer<Controller> life(this);
    const auto source=host.workspace?host.workspace():Workspace{};
    if(!source.project || !host.registerBindings)return;
    bool keyboard=false,mouse=false;
    for(const auto& item:source.project->features()) {
        if(item->hotkey().isEmpty())continue;
        (item->hotkey().isMouseButton()?mouse:keyboard)=true;
    }
    const auto report=host.registerBindings(*source.project);
    if(!life || !host.workspace || !same(source,host.workspace()))return;
    if(!report.keyboardHook && keyboard && host.log)
        host.log(hotkeyText("키보드 단축키를 등록하지 못했습니다. 백업 방식으로 다시 시도합니다. "
                    "계속 안 되면 관리자 권한으로 실행하거나 보안 프로그램 예외를 확인하세요."),false);
    if(!life)return;
    if(!report.mouseHook && mouse && host.log)
        host.log(hotkeyText("마우스 단축키를 등록하지 못했습니다. 마우스 단축키가 동작하지 않을 수 있습니다."),false);
    for(const auto& name:report.failedNames) {
        if(!life)return;
        if(host.log)host.log(hotkeyText("단축키 등록 실패 (%1) — 다른 프로그램이 사용 중이거나 권한이 부족할 수 있습니다.").arg(name),true);
    }
    if(life && host.workspace && same(source,host.workspace()) && host.prune)host.prune();
}

bool Controller::acceptQueuedStart(const QString& id) {
    const auto pending=m_pending.find(id);
    if(pending==m_pending.end())return false;
    const auto source=pending.value();m_pending.erase(pending);
    const Host host=m_host;const QPointer<Controller> life(this);
    if(!host.workspace || !same(source,host.workspace()))return false;
    const auto* selected=feature(source,id);
    if(!selected || !selected->enabled() || selected->runMode()!=FeatureRunMode::Hold)return false;
    const auto denied=block(host.gate?host.gate(id):Gate{});
    if(!life || !same(source,host.workspace()))return false;
    if(denied!=Block::None){if(host.blocked)host.blocked(denied);return false;}
    return true;
}
QStringList Controller::visibleQueuedStarts()const {
    const auto pending=m_pending;const auto host=m_host;const QPointer<const Controller> life(this);
    if(!host.workspace)return {};
    const auto source=host.workspace();if(!life)return {};
    QStringList result;
    for(auto it=pending.begin();it!=pending.end();++it){
        if(!same(it.value(),source))continue;
        const auto* f=feature(source,it.key());if(f && f->enabled() && f->runMode()==FeatureRunMode::Hold)result.push_back(it.key());
    }
    return result;
}
