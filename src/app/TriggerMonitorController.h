#pragma once
#include <QObject>
#include <QString>
#include <functional>
#include <string>
#include <vector>
class QTimer;
class Project;
class Feature;
class ProfileManager;
struct FeatureRunSession;

// GUI-thread persisted watch state and monitor supervision. No widgets/MainWindow.
class TriggerMonitorController : public QObject {
    friend class TriggerMonitorUiChecks;
public:
    struct Workspace {Project* project=nullptr;QString owner;quint64 generation=0;bool blocked=true;};
    struct Host {
        std::function<Workspace()> workspace;
        std::function<void()> prepare,resume;
        std::function<FeatureRunSession*(const std::string&)> session;
        std::function<Feature*(FeatureRunSession&)> feature;
        std::function<bool(const FeatureRunSession&)> active;
        std::function<bool(const std::string&)> running;
        std::function<void(const QString&)> start,stop;
        std::function<std::vector<std::string>()> sessions;
        std::function<bool(FeatureRunSession&,Feature*)> mayContinue;
        std::function<void(FeatureRunSession&)> ensureEngine;
        std::function<void(FeatureRunSession&,Feature*)> launch;
        std::function<void(const QString&)> warning;
    };
    explicit TriggerMonitorController(ProfileManager& profiles,QObject* parent=nullptr);
    void setHost(Host host){m_host=std::move(host);}
    bool& suppressed(){return m_suppressed;} // shared lifecycle bulk-teardown guard
    bool persist(const QString& id,bool armed);
    bool restore();
    bool prune();
    void scheduleRestore();
    void scheduleEnsure();
    void ensure();
    void stop();
private:
    bool filter(bool restore);
    bool current(const Workspace& source,const Host& host) const;
    ProfileManager& m_profiles;
    Host m_host;
    QTimer* m_restore=nullptr;
    QTimer* m_ensure=nullptr;
    bool m_suppressed=false,m_busy=false,m_stopped=false,m_ensureDeferred=false;
};
