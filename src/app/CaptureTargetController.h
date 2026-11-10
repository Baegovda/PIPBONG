#pragma once
#include "app/ForegroundRunGate.h"
#include "model/FeatureCaptureTargetScope.h"
#include <QString>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
class ProfileManager;
class Feature;
struct FeatureRunSession;
class CaptureTargetController {
    friend class CaptureTargetUiChecks;
public:
    using Bindings=ForegroundRunGate::ProfileBindings;
#ifdef _WIN32
    using Handle=HWND;
#else
    using Handle=void*;
#endif
    struct Window {
        Handle hwnd=nullptr;
        QString title,path;
        unsigned long processId=0;
        bool visible=true,minimized=false;
    };
    struct Selection {QString title,path;Handle hwnd=nullptr;bool sub=false,foreground=false;};
    struct Applied {Bindings bindings;Selection target;};
    struct Ports {
        std::function<std::optional<Window>(Handle)> read;
        std::function<std::optional<Window>(const QString&,const QString&)> find;
        std::function<void(const Applied&)> apply;
    };
    struct Host {
        std::function<QString()> mainTitle;
        std::function<ForegroundWindowState()> foreground;
        std::function<bool(const QString&)> foregroundAllowed;
        std::function<bool()> mayApply;
        std::function<Feature*(FeatureRunSession&)> feature;
        std::function<void(Handle,const QString&)> remember;
        std::function<void()> monitorChanged;
        std::function<void(const FeatureRunSession&)> sessionChanged;
    };
    explicit CaptureTargetController(ProfileManager& profiles);
    CaptureTargetController(ProfileManager& profiles,Ports ports);
    void setHost(Host host) {m_host=std::move(host);}
    void setSessions(std::map<std::string,FeatureRunSession>* sessions) {m_sessions=sessions;}
    Bindings bindings(const QString& profileId={}) const;
    QString effective() const;
    QString resolve(FeatureCaptureTargetScope scope=FeatureCaptureTargetScope::Auto,bool background=false,
                    const QString& profileId={}) const;
    Selection choose(FeatureCaptureTargetScope scope=FeatureCaptureTargetScope::Auto,bool background=false,
                     const QString& profileId={}) const;
    QString lookup() const;
    bool adoptForeground();
    void sync();
    void applyTitle(const QString& title) const;
    bool backgroundVisible(FeatureCaptureTargetScope scope,const QString& profileId={}) const;
    bool mayUpdateGlobal() const {return mayApply();}
    bool refreshSession(FeatureRunSession& session,const Feature& feature);
    QString sessionTitle(FeatureRunSession& session,const Feature* feature,bool refresh);
    std::wstring sessionTitleForRun(FeatureRunSession& session,bool refresh);
private:
    std::optional<bool> classify(const Bindings&,const Window&) const;
    std::optional<Selection> foreground(const Bindings&) const;
    std::optional<Window> visible(const QString&,const QString&,bool allowMinimized=false) const;
    void apply(const Bindings&,const Selection&) const;
    void refreshMonitors();
    bool mayApply() const;
    static Selection selected(const Bindings&,bool sub,Handle hwnd=nullptr,bool foreground=false);
    static QString pathForTitle(const Bindings&,FeatureCaptureTargetScope,const QString&);
    ProfileManager& m_profiles;
    Ports m_ports;
    Host m_host;
    std::map<std::string,FeatureRunSession>* m_sessions=nullptr;
};
