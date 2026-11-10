#pragma once
#include "app/ForegroundRunGate.h"
#include "app/ProfileForegroundResolver.h"
#include <QObject>
#include <functional>
#include <utility>
class QTimer;
class ProfileManager;
class CaptureTargetController;
class Feature;
struct FeatureRunSession;

// Foreground orchestration and gate composition. No widgets or MainWindow access.
class ForegroundSessionController : public QObject {
    friend class ForegroundSessionUiChecks;
    friend class FeatureHotkeyUiChecks;
public:
#ifdef _WIN32
    using Handle=HWND;
#else
    using Handle=void*;
#endif
    enum class Gate { Binding, Profile, MainOrSub, Run, Scoped };
    struct Snapshot {
        bool switching=false,pipeline=false,historyBusy=false,runWithout=false,shutdown=true;
        QString deferred,lastLinked;
        bool restoreRecent=false;
    };
    struct Window { bool valid=false,pipbong=false,shell=false;QString path; };
    struct Ports {
        std::function<ProfileForegroundResolver::ResolveResult(const ForegroundWindowState&)> resolve;
        std::function<Window(Handle,bool)> readWindow;
        std::function<bool(Gate,const ForegroundRunGate::GateInput&)> gate;
    };
    struct Host {
        std::function<Snapshot()> snapshot;
        std::function<ForegroundWindowState()> foreground;
        std::function<bool()> modifierHeld;
        std::function<void()> synchronize,flushDeferred,ensureTriggers,updateDetails,resetLatch,syncDesktop;
        std::function<void()> reconcileSessions,resumeSessions;
        std::function<void(const ForegroundWindowState&)> applySwitch;
        std::function<bool(const QString&,bool)> executeSwitch;
        std::function<void(Handle,const QString&)> captureHints;
    };
    ForegroundSessionController(ProfileManager& profiles,CaptureTargetController& targets,QObject* parent=nullptr);
    ForegroundSessionController(ProfileManager& profiles,CaptureTargetController& targets,Ports ports,QObject* parent=nullptr);
    void setHost(Host host){m_host=std::move(host);}
    void changed(const ForegroundWindowState& state);
    void altReleased();
    void pipbongFocused();
    void applySwitch(const ForegroundWindowState& state);
    void postSwitch(bool allowAutomatic);
    void finishGate();
    void resetLatchFor(Handle hwnd);
    void ensureHotkeyReady();
    bool switchToForeground();
    QString foregroundProfile() const;
    QString profileForWindow(Handle hwnd) const;
    ForegroundRunGate::GateInput gateInput(const Feature* feature=nullptr,const QString& profileId={}) const;
    bool profileMatches() const;
    bool mainOrSub() const;
    bool backgroundGate(const Feature* feature,const QString& profileId={}) const;
    bool runGate(const Feature* feature) const;
    bool scopedGate(const Feature* feature) const;
    bool sessionGate(const FeatureRunSession& session,const Feature* feature) const;
    void stop();
private:
    Snapshot snapshot() const;
    ForegroundWindowState foreground() const;
    ProfileForegroundResolver::ResolveResult resolve(const ForegroundWindowState&) const;
    bool evaluate(Gate gate,const Feature* feature=nullptr,const QString& profileId={}) const;
    ProfileManager& m_profiles;
    CaptureTargetController& m_targets;
    Ports m_ports;
    Host m_host;
    QTimer* m_postSwitch=nullptr;
    bool m_allowAutomatic=false,m_stopped=false;
    bool m_readingDesktop=false;
    quint64 m_manualReadSequence=0;
    Handle m_lastLatchWindow=nullptr;
};
