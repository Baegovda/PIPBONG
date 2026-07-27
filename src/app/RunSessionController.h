#pragma once

#include "model/FeatureRunMode.h"

#include "FeatureRunSession.h"

#include <functional>
#include <map>
#include <unordered_map>

#include <QObject>

class Feature;
class Project;

class RunSessionController : public QObject {
    Q_OBJECT

public:
    struct HostCallbacks {
        std::function<bool()> shouldSkipForegroundGateReconcile;
        std::function<bool(Feature* feature)> runForegroundGateActive;
        std::function<void()> updateRunUiState;
        std::function<void(FeatureRunSession& session)> stopSessionEngine;
        std::function<void(FeatureRunSession& session, Feature* feature, bool firstStartUi)>
            launchTriggerMonitor;
        std::function<void(FeatureRunSession& session, Feature* feature, bool repeatIteration)>
            launchWorkflowRun;
        std::function<bool(const std::string& featureId)> isHoldBindingDown;
    };

    explicit RunSessionController(QObject* parent = nullptr);

    void setProject(Project* project);
    void setRunSessions(std::map<std::string, FeatureRunSession>* sessions);
    void setHostCallbacks(const HostCallbacks& callbacks);

    void reconcileWithForegroundGate();
    bool deferRunUntilScopedTargetForeground(FeatureRunSession& session, Feature* feature);
    void resumeWaitingScopedTargetForegroundSessions();
    void scheduleScopedTargetForegroundResumePoll();

    struct FinishGateCallbacks {
        std::function<bool()> switchingProfile;
        std::function<bool()> profileSwitchPipelineActive;
        std::function<void()> flushDeferredProfileSwitchIfIdle;
        std::function<void()> scheduleEnsureTriggerMonitorEnginesRunning;
        std::function<void()> updateTargetWindowDetails;
    };

    void finishForegroundSessionGate(const FinishGateCallbacks& callbacks);

private:
    Project* m_project = nullptr;
    std::map<std::string, FeatureRunSession>* m_runSessions = nullptr;
    HostCallbacks m_host;
    bool m_scopedTargetForegroundResumePending = false;
};
