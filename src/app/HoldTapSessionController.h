#pragma once
#include "app/FeatureRunSession.h"
#include "core/workflow/HoldKeyTapMultiplexer.h"
#include <QObject>
#include <QPointer>
#include <functional>
#include <map>
#include <vector>
class Feature;

// GUI-owned launch/completion and deferred presentation. The worker receives
// only a context and a value-owned hold reader. Host queries must be read-only.
class HoldTapSessionController : public QObject {
    friend class HoldTapSessionChecks;
public:
    using Current = std::function<bool()>;
    using Reader = HoldKeyTapMultiplexer::ShouldContinueFn;
    struct Host {
        std::function<FeatureRunSession*(const std::string&)> session;
        std::function<Feature*(FeatureRunSession&)> feature;
        std::function<bool(const FeatureRunSession&)> activeOwner, displayed, logDetails;
        std::function<bool(FeatureRunSession&,Feature*)> deferForeground;
        std::function<void(FeatureRunSession&,Feature*)> syncContext, applyPolicies, syncInterrupt;
        std::function<Reader(const std::string&)> holdReader;
        std::function<void()> synchronize, refresh, persist;
        std::function<void(Feature*)> select;
        std::function<void(FeatureRunSession&,Current)> decorate;
        std::function<void(FeatureRunSession&)> logStart;
        std::function<void(const std::string&)> scheduleStart, scheduleEnd;
        std::function<bool()> deferStopped;
        std::function<void(const std::string&,bool,const QString&,bool)> finish;
    };
    struct Ports {
        // Completion must be queued to the GUI thread, never inside start().
        std::function<quint64(const std::string&,int,int,Reader,const std::shared_ptr<ExecutionContext>&)> start;
        std::function<void(const std::string&,quint64)> stop;
    };
    HoldTapSessionController(HoldKeyTapMultiplexer&,Host,QObject* parent=nullptr);
    HoldTapSessionController(Host,Ports,QObject* parent=nullptr);
    void launch(FeatureRunSession&,Feature*,int virtualKey);
    void finished(const std::string&,quint64 token,bool success,const QString&);
    void queueStarted(FeatureRunSession&);
    void presentStarts(const std::vector<std::string>&);
    void rememberEnd(FeatureRunSession&);
    void finishPending(const std::string&);
    void retire(FeatureRunSession&);
private:
    struct Ticket {
        QPointer<const HoldTapSessionController> life;
        Host host;
        std::string id;
        QString owner;
        FeatureRunSession* original=nullptr;
        std::shared_ptr<ExecutionContext> context;
        WorkflowEngine* engine=nullptr; // identity only, never dereferenced
        quint64 generation=0,token=0;
        FeatureRunSession* current(bool checkGeneration=true,bool checkToken=true) const;
        Feature* feature(bool checkGeneration=true,bool checkToken=true) const;
    };
    struct Lane {Ticket ticket;Reader reader;};
    Ticket ticket(FeatureRunSession&);
    void launch(FeatureRunSession&,Feature*,int,Reader);
    Host m_host;
    Ports m_ports;
    std::map<std::string,Lane> m_lanes;
    std::map<std::string,Lane> m_prepared;
    std::map<std::string,Ticket> m_starts,m_ends;
};
