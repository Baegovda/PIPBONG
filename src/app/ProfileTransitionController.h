#pragma once
#include <QObject>
#include <QString>
#include <functional>
class ProfileManager;
class ProjectWorkspaceController;


/// Serializes save/stage/manifest/model installation; GUI presentation is host-owned.
class ProfileTransitionController : public QObject {
public:
    struct State { bool switching=false,pipeline=false,deferTargetDetails=false; };
    struct AutomaticCallbacks {
        std::function<void(const QString&)> defer;
        std::function<void(int)> schedule;
        std::function<void()> committed;
        std::function<void(const QString&,bool)> manualCommitted;
        std::function<bool()> hasDeferred;
        std::function<void()> clearDeferred;
    };
    struct HostCallbacks {
        std::function<bool(bool automatic)> canBegin;
        std::function<bool()> dragActive,saveMemo;
        std::function<void()> cancelDrags,detach;
        std::function<void(bool)> lockUi;
        std::function<void(const QString&)> selectProfile;
        std::function<void()> resyncSelection;
        std::function<void(bool)> loaded,completed,finishViews,restoreTriggersAndReconcile;
        std::function<void()> foregroundChanged;
        std::function<void(const QString&)> warning;
    };
    ProfileTransitionController(ProfileManager&,ProjectWorkspaceController&,QObject* parent=nullptr);
    void setAutomatic(AutomaticCallbacks callbacks) { m_auto=std::move(callbacks); }
    State& state() { return m_state; }
    void setHost(HostCallbacks host) { m_host=std::move(host); }
    bool start(const QString& id,bool automatic);
    void abort(bool resync=true);
private:
    void complete(bool automatic,quint64 generation);
    void retryManual(int delayMs=0);
    ProfileManager& m_profiles;
    ProjectWorkspaceController& m_workspace;
    AutomaticCallbacks m_auto;
    HostCallbacks m_host;
    State m_state;
    QString m_pendingManual;
    bool m_manualRetryScheduled=false;
    quint64 m_generation=0;
};
