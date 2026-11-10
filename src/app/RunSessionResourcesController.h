#pragma once
#include <QObject>
#include <QPointer>
#include <functional>
#include <string>
#include <vector>
class Project;
class Feature;
class RunUiPublisher;
struct FeatureRunSession;

// GUI-side ownership/configuration; worker callbacks own immutable controls
// and UI channels, never this controller, a Feature or MainWindow.
class RunSessionResourcesController:public QObject {
public:
    struct Workspace {Project* project=nullptr;QString owner;quint64 generation=0;};
    struct Binding {std::wstring title,process;std::string directory;};
    struct Host {
        // Lookups/predicates are read-only. Binding may refresh this session's
        // capture binding. Warning/afterBatch can reenter application code.
        std::function<Workspace()> workspace;
        std::function<std::vector<std::string>()> sessions;
        std::function<FeatureRunSession*(const std::string&)> session;
        std::function<Feature*(FeatureRunSession&)> feature;
        std::function<Binding(FeatureRunSession&,bool)> binding;
        std::function<bool(FeatureRunSession&,Feature*)> targetAllowed,repeatAllowed;
        std::function<bool(const FeatureRunSession&)> captureAllowed;
        std::function<std::function<bool()>(const std::string&)> holdReader;
        std::function<void(const QString&)> warning;
        std::function<void(FeatureRunSession&,Feature*)> afterBatch;
    };
    RunSessionResourcesController(RunUiPublisher&,Host,QObject* parent=nullptr);
    bool refreshWorkflow(FeatureRunSession&,Feature*);
    bool applyDeferred(FeatureRunSession&,Feature*);
    void requestRefresh(const std::string&);
    bool ensure(FeatureRunSession&,Feature*,bool refresh,bool deferClone);
    // Also supports caller-owned contexts outside the registry (boundary probes).
    bool syncContext(FeatureRunSession&,bool refreshBinding=true);
    bool applyPolicies(FeatureRunSession&,Feature*);
    bool configureRepeat(FeatureRunSession&,Feature*);
    void synchronize();
    void afterBatch(FeatureRunSession&);
private:
    struct Ticket;
    Ticket ticket(FeatureRunSession&)const;
    Host m_host;
    QPointer<RunUiPublisher> m_publisher;
};
