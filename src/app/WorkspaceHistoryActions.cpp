#include "app/WorkspaceHistoryActions.h"
#include "app/ProfilePackageController.h"
#include "ui/widgets/WorkspaceHistoryActivity.h"
#include "ui/memo/MemoDialog.h"
#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <utility>

struct WorkspaceHistoryActions::State : std::enable_shared_from_this<State> {
    QPointer<QWidget> parent;
    QPointer<ProfilePackageController> packages;
    HostCallbacks host;
    std::unique_ptr<WorkspaceHistoryActivity> activity;
    QPointer<MemoDialog> suspendedMemo;
    bool withParent=false,alive=true,busy=false,ending=false,packagesPaused=false;
    bool memoWasSuspended=false,pendingFinished=false;
    quint64 epoch=0;
    bool current()const{return alive && (!withParent || parent) && packages;}
    void end(){
        if(ending)return;ending=true;
        auto progress=std::move(activity);
        const bool resumePackage=std::exchange(packagesPaused,false);
        const auto package=packages;const auto memo=suspendedMemo;
        const bool previous=memoWasSuspended;suspendedMemo=nullptr;
        if(resumePackage && package)package->resume();
        if(memo)memo->setAutoSaveSuspended(previous);
        const bool hadActivity=bool(progress);progress.reset();
        if(hadActivity && current())pendingFinished=true;
        ending=false;
    }
    void retire(){alive=false;pendingFinished=false;end();}
    bool begin(bool restoring){
        if(!current())return false;
        const auto canBegin=host.canBegin;
        if(canBegin && !canBegin(restoring))return false;
        if(!current())return false;
        activity=std::make_unique<WorkspaceHistoryActivity>(parent.data(),restoring);
        const auto suspend=host.suspendAutoSave;if(suspend)suspend();
        if(!current())return false;
        const auto findMemo=host.memo;
        const QPointer<MemoDialog> memo=findMemo?findMemo():nullptr;
        if(!current())return false;
        if(memo){
            if(!memo->saveNow() || !memo || !current())return false;
            suspendedMemo=memo;memoWasSuspended=memo->autoSaveSuspended();
            memo->setAutoSaveSuspended(true);
        }
        if(!current())return false;
        // Retirement during the nested wait cannot release this borrow until
        // pauseAndWait has reported whether it acquired or rolled it back.
        const auto package=packages;
        const bool paused=package && package->pauseAndWait(true);
        packagesPaused=paused && bool(package);
        if(!current()){end();return false;}
        return packagesPaused;
    }
    void finish(){
        if(!pendingFinished || !current())return;pendingFinished=false;
        const auto callback=host.finished;if(!callback)return;
        const auto generation=epoch;const std::weak_ptr<State> weak=shared_from_this();
        QObject* context=parent?static_cast<QObject*>(parent.data()):qApp;
        QTimer::singleShot(0,context,[weak,generation,callback]{
            const auto state=weak.lock();
            if(state && state->current() && !state->busy && state->epoch==generation)callback();
        });
    }
};

WorkspaceHistoryActions::WorkspaceHistoryActions(QWidget* parent,ProfilePackageController& packages,HostCallbacks host)
    :m_state(std::make_shared<State>()){
    const auto state=m_state;state->parent=parent;state->withParent=bool(parent);state->packages=&packages;state->host=std::move(host);
    if(parent){
        const std::weak_ptr<State> weak=state;
        QObject::connect(parent,&QObject::destroyed,[weak]{if(const auto state=weak.lock())state->retire();});
    }
    auto callbacks=state->host.history;
    callbacks.validate=[state]{
        if(!state->current())return false;
        const auto query=state->host.history.validate;
        const bool allowed=!query || query();return state->current() && allowed;
    };
    callbacks.beginOperation=[state](bool restoring){return state->begin(restoring);};
    callbacks.endOperation=[state]{state->end();};
    callbacks.prepare=[state](bool restoring){
        if(!state->current())return false;
        const auto prepare=state->host.history.prepare;
        const bool ready=prepare && prepare(restoring);return state->current() && ready;
    };
    callbacks.paths=[state](const QString& profile){
        if(!state->current())return WorkspaceHistoryController::Paths{};
        const auto query=state->host.history.paths;
        const auto paths=query?query(profile):WorkspaceHistoryController::Paths{};
        return state->current()?paths:WorkspaceHistoryController::Paths{};
    };
    callbacks.restored=[state](auto scope,const QString& profile){
        if(!state->current())return;
        if(scope==WorkspaceHistoryController::Scope::Workspace){
            const auto query=state->host.memo;
            const QPointer<MemoDialog> memo=query?query():nullptr;
            if(!state->current())return;
            if(memo)memo->prepareForWorkspaceReload();
        }
        if(!state->current())return;
        const auto restored=state->host.history.restored;if(restored)restored(scope,profile);
    };
    m_history=std::make_shared<WorkspaceHistoryController>(std::move(callbacks));
}
WorkspaceHistoryActions::~WorkspaceHistoryActions(){const auto state=m_state;state->retire();}

bool WorkspaceHistoryActions::run(Operation operation,QString reason){
    const auto state=m_state;const auto history=m_history;
    if(!state->current() || state->busy)return false;
    state->busy=true;++state->epoch;
    struct Finish {
        std::shared_ptr<State> state;
        ~Finish(){state->busy=false;state->finish();}
    } finish{state};
    const bool applied=operation==Operation::Push?history->push(reason):operation==Operation::Undo?history->undo():history->redo();
    const QString error=history->lastError();
    const bool result=applied && (operation!=Operation::Push || state->current());
    const auto report=state->host.reportError;
    if(!applied && !error.isEmpty() && state->current() && report)report(error);
    return result;
}
bool WorkspaceHistoryActions::push(const QString& reason){return run(Operation::Push,reason);}
bool WorkspaceHistoryActions::undo(){return run(Operation::Undo);}
bool WorkspaceHistoryActions::redo(){return run(Operation::Redo);}
