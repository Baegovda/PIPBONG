#include "app/ForegroundSessionController.h"
#include "app/CaptureTargetController.h"
#include "app/ProfileManager.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/FeatureRunSession.h"
#include "core/capture/ScreenCapture.h"
#include "model/Feature.h"
#include <QPointer>
#include <QTimer>

namespace {
using Controller=ForegroundSessionController;
Controller::Ports nativePorts(ProfileManager& profiles) {
    Controller::Ports ports;
    ports.resolve=[&profiles](const auto& state){return ProfileForegroundResolver::resolve(profiles,state);};
    ports.readWindow=[](Controller::Handle hwnd,bool details) {
        Controller::Window value;
#ifdef _WIN32
        value.valid=hwnd && IsWindow(hwnd);if(!value.valid)return value;
        value.pipbong=ForegroundWindowMonitor::isPipbongProcessWindow(hwnd);
        value.shell=ForegroundWindowMonitor::isShellTransientWindow(hwnd);
        if(details) {ScreenCapture::TargetWindowInfo info;if(ScreenCapture::queryWindowInfo(hwnd,info))value.path=QString::fromStdWString(info.processPath);}
#endif
        return value;
    };
    ports.gate=[](Controller::Gate gate,const ForegroundRunGate::GateInput& input) {
        switch(gate) {
        case Controller::Gate::Binding:return ForegroundRunGate::activeProfileForegroundBindingMatches(input);
        case Controller::Gate::Profile:return ForegroundRunGate::foregroundProfileMatchesActive(input);
        case Controller::Gate::MainOrSub:return ForegroundRunGate::profileMainOrSubForegroundActive(input);
        case Controller::Gate::Run:return ForegroundRunGate::runForegroundGateActive(input);
        case Controller::Gate::Scoped:return ForegroundRunGate::scopedTargetForegroundActive(input);
        }
        return false;
    };
    return ports;
}
ForegroundRunGate::FeatureGateOptions options(const Feature& feature) {
    return {feature.runMode(),feature.triggerRunWithoutTargetForeground(),feature.requireScopedTargetForeground(),feature.captureTargetScope()};
}
}
ForegroundSessionController::ForegroundSessionController(ProfileManager& profiles,CaptureTargetController& targets,QObject* parent)
    : ForegroundSessionController(profiles,targets,nativePorts(profiles),parent) {}
ForegroundSessionController::ForegroundSessionController(ProfileManager& profiles,CaptureTargetController& targets,Ports ports,QObject* parent)
    : QObject(parent),m_profiles(profiles),m_targets(targets),m_ports(std::move(ports)),m_postSwitch(new QTimer(this)) {
    m_postSwitch->setSingleShot(true);m_postSwitch->setInterval(0);
    connect(m_postSwitch,&QTimer::timeout,this,[this] {
        if(snapshot().shutdown)return;
        const QPointer<ForegroundSessionController> guard(this);
        if(m_allowAutomatic) {m_manualReadSequence=0;applySwitch(foreground());}
        else if(m_host.syncDesktop) {
            m_readingDesktop=true;m_host.syncDesktop();if(!guard)return;
            m_readingDesktop=false;m_manualReadSequence=foreground().monotonicSeq;
        }
        if(guard)finishGate();
    });
}
ForegroundSessionController::Snapshot ForegroundSessionController::snapshot() const {
    auto value=m_host.snapshot?m_host.snapshot():Snapshot{};value.shutdown|=m_stopped;return value;
}
ForegroundWindowState ForegroundSessionController::foreground() const {return m_host.foreground?m_host.foreground():ForegroundWindowState{};}
ProfileForegroundResolver::ResolveResult ForegroundSessionController::resolve(const ForegroundWindowState& state) const {
    return m_ports.resolve?m_ports.resolve(state):ProfileForegroundResolver::ResolveResult{};
}
void ForegroundSessionController::stop(){m_stopped=true;m_postSwitch->stop();m_lastLatchWindow=nullptr;}
void ForegroundSessionController::postSwitch(bool automatic) {
    if(snapshot().shutdown)return;
    if(m_postSwitch->isActive())m_allowAutomatic&=automatic; // A manual completion wins this coalesced turn.
    else {m_allowAutomatic=automatic;m_postSwitch->start();}
}
void ForegroundSessionController::applySwitch(const ForegroundWindowState& state) {
    const auto value=snapshot();if(value.shutdown || value.historyBusy)return;
    if(m_host.applySwitch)m_host.applySwitch(state);
}
void ForegroundSessionController::altReleased() {
    if(snapshot().shutdown)return;
    const QPointer<ForegroundSessionController> guard(this);
    if(m_host.flushDeferred)m_host.flushDeferred();
    if(guard && m_host.resetLatch)m_host.resetLatch();
}
void ForegroundSessionController::finishGate() {
    if(snapshot().shutdown)return;
    const QPointer<ForegroundSessionController> guard(this);
    if(m_host.synchronize)m_host.synchronize();
    if(!guard)return;
    auto value=snapshot();if(value.shutdown || value.switching || value.pipeline || value.historyBusy)return;
    if((!m_host.modifierHeld || !m_host.modifierHeld()) && m_host.flushDeferred)m_host.flushDeferred();
    if(!guard)return;
    // A flush can start an asynchronous profile install. Its completion owns resume.
    value=snapshot();if(value.shutdown || value.switching || value.pipeline || value.historyBusy)return;
    if(m_host.reconcileSessions)m_host.reconcileSessions();if(!guard)return;
    if(m_host.resumeSessions)m_host.resumeSessions();if(!guard)return;
    if(m_host.ensureTriggers)m_host.ensureTriggers();if(!guard)return;
    if(m_host.updateDetails)m_host.updateDetails();
}
void ForegroundSessionController::changed(const ForegroundWindowState& notification) {
    if(snapshot().shutdown)return;
    const QPointer<ForegroundSessionController> guard(this);
    if(m_host.synchronize)m_host.synchronize();if(!guard)return;
    if(snapshot().shutdown || snapshot().historyBusy)return;
    if(m_readingDesktop)return;
    auto state=notification;const auto live=foreground();
    if(state.monotonicSeq && live.monotonicSeq>state.monotonicSeq)state=live;
    const bool manualRead=m_manualReadSequence && state.monotonicSeq==m_manualReadSequence;
    if(state.monotonicSeq>=m_manualReadSequence)m_manualReadSequence=0;
    if(manualRead) {
#ifdef _WIN32
        resetLatchFor(state.rootHwnd);
#endif
        if(guard)finishGate();return;
    }
#ifdef _WIN32
    const auto value=snapshot();
    if(!value.switching && !value.pipeline && state.rootHwnd && !state.pipbong && !state.shellTransient && value.deferred.isEmpty()) {
        const auto profile=resolve(state);if(!guard)return;
        if(profile.profileId==m_profiles.activeProfileId()) {
            if(m_host.captureHints)m_host.captureHints(state.rootHwnd,state.title);if(!guard)return;
            resetLatchFor(state.rootHwnd);if(guard)finishGate();return;
        }
    }
#endif
    applySwitch(state);if(!guard)return;
#ifdef _WIN32
    if(!state.pipbong) {
        if(!m_targets.adoptForeground())m_targets.sync();if(!guard)return;
        resetLatchFor(state.rootHwnd);
    } else m_lastLatchWindow=nullptr;
    if(!guard)return;
    const auto after=snapshot();if(!after.switching && !after.pipeline)finishGate();
#else
    m_targets.sync();
#endif
}
void ForegroundSessionController::pipbongFocused() {
    const auto value=snapshot();if(value.shutdown)return;
    const QPointer<ForegroundSessionController> guard(this);
    if(!value.lastLinked.isEmpty() && m_profiles.profileById(value.lastLinked) && m_profiles.isDefaultProfile(m_profiles.activeProfileId())
        && !m_profiles.isDefaultProfile(value.lastLinked) && value.restoreRecent && m_host.executeSwitch)
        m_host.executeSwitch(value.lastLinked,true);
    if(guard && !snapshot().switching && !snapshot().pipeline)finishGate();
}
void ForegroundSessionController::resetLatchFor(Handle hwnd) {
    if(snapshot().shutdown || !m_ports.readWindow)return;
    const auto window=m_ports.readWindow(hwnd,false);
    if(!window.valid || window.pipbong || window.shell) {
        if(!hwnd || window.pipbong)m_lastLatchWindow=nullptr;
        return;
    }
    if(!mainOrSub() && !profileMatches())return;
    if(hwnd==m_lastLatchWindow)return;
    m_lastLatchWindow=hwnd;if(m_host.resetLatch)m_host.resetLatch();
}
void ForegroundSessionController::ensureHotkeyReady() {
    if(snapshot().shutdown || snapshot().runWithout)return;
    const QPointer<ForegroundSessionController> guard(this);
    if(m_host.flushDeferred)m_host.flushDeferred();if(!guard)return;
    switchToForeground();if(!guard)return;
    if(!m_targets.adoptForeground())m_targets.sync();if(!guard)return;
#ifdef _WIN32
    resetLatchFor(foreground().rootHwnd);
#endif
}
bool ForegroundSessionController::switchToForeground() {
    const auto value=snapshot();if(value.shutdown || value.switching || value.pipeline)return false;
    const auto before=m_profiles.activeProfileId();const QPointer<ForegroundSessionController> guard(this);
    applySwitch(foreground());if(!guard)return false;
    return m_profiles.activeProfileId()!=before || evaluate(Gate::Binding);
}
QString ForegroundSessionController::foregroundProfile() const {
    const auto state=foreground();
#ifdef _WIN32
    if(!state.rootHwnd || state.pipbong || state.shellTransient)return {};
    return resolve(state).profileId;
#else
    return {};
#endif
}
QString ForegroundSessionController::profileForWindow(Handle hwnd) const {
    if(!m_ports.readWindow)return {};
    const auto window=m_ports.readWindow(hwnd,true);
    return window.valid && !window.path.isEmpty()?ProfileForegroundResolver::profileIdForProcessPath(m_profiles,window.path):QString{};
}
ForegroundRunGate::GateInput ForegroundSessionController::gateInput(const Feature* feature,const QString& id) const {
    ForegroundRunGate::GateInput input;input.bindings=m_targets.bindings(id);input.foreground=foreground();
    input.runWithoutTargetWindow=snapshot().runWithout;
#ifdef _WIN32
    if(input.foreground.rootHwnd && !input.foreground.pipbong && !input.foreground.shellTransient)
        input.foregroundResolvedProfileId=resolve(input.foreground).profileId;
#endif
    if(!input.foregroundResolvedProfileId.isEmpty())input.foregroundResolvedIsDefault=m_profiles.isDefaultProfile(input.foregroundResolvedProfileId);
    if(feature)input.triggerBackgroundVisible=backgroundGate(feature,id);
    return input;
}
bool ForegroundSessionController::backgroundGate(const Feature* feature,const QString& id) const {
    if(!feature || feature->runMode()!=FeatureRunMode::Trigger || !feature->triggerRunWithoutTargetForeground())return false;
    if(snapshot().runWithout)return true;
    const auto owner=id.isEmpty()?m_profiles.activeProfileId():id;
    if(!m_profiles.profileById(owner) || m_profiles.isDefaultProfile(owner))return false;
    return m_targets.backgroundVisible(feature->captureTargetScope(),owner);
}
bool ForegroundSessionController::evaluate(Gate gate,const Feature* feature,const QString& id) const {
    const auto state=snapshot();
    if(state.shutdown || state.switching || state.pipeline || state.historyBusy || !m_ports.gate)return false;
    auto input=gateInput(feature,id);
    const auto settings=feature?options(*feature):ForegroundRunGate::FeatureGateOptions{};
    if(feature)input.feature=&settings;
    return m_ports.gate(gate,input);
}
bool ForegroundSessionController::profileMatches() const{return evaluate(Gate::Profile);}
bool ForegroundSessionController::mainOrSub() const{return evaluate(Gate::MainOrSub);}
bool ForegroundSessionController::runGate(const Feature* feature) const{return evaluate(Gate::Run,feature);}
bool ForegroundSessionController::scopedGate(const Feature* feature) const{return !feature || evaluate(Gate::Scoped,feature);}
bool ForegroundSessionController::sessionGate(const FeatureRunSession& session,const Feature* feature) const {
    const auto state=snapshot();
    if(state.shutdown || state.switching || state.pipeline || state.historyBusy
        || (!session.profileId.isEmpty() && !m_profiles.profileById(session.profileId)))return false;
    if(!session.profileId.isEmpty() && session.profileId!=m_profiles.activeProfileId())return true; // Existing concurrent-profile policy.
    return runGate(feature);
}
