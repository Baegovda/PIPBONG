#pragma once
#include <QObject>
#include <QPoint>
#include <QString>
#include <functional>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
class ExecutionContext;
struct FeatureRunSession;

// GUI-thread ownership of temporary trigger pauses and cursor restoration.
// OS cursor operations are ports; no MainWindow/widgets.
class TriggerPreemptionController : public QObject {
    friend class TriggerPreemptionUiChecks;
public:
    using Sessions=std::map<std::string,FeatureRunSession>;
    struct Ports {std::function<std::optional<QPoint>()> cursor;std::function<void(QPoint)> move;};
    struct Host {
        std::function<void()> reconcile,changed;
        std::function<void(const std::string&,const QString&,bool)> log;
    };
    TriggerPreemptionController(Sessions& sessions,QObject* parent=nullptr);
    TriggerPreemptionController(Sessions& sessions,Ports ports,QObject* parent=nullptr);
    ~TriggerPreemptionController() override;
    void setHost(Host host){m_host=std::move(host);}
    void pause(FeatureRunSession& trigger);
    void resume(FeatureRunSession& trigger);
    void shutdown(); // releases owned pauses; deliberately does not move the cursor
private:
    struct Claim {std::size_t owners=0;std::uint64_t pause=0;QString profile;};
    struct Frame {std::optional<QPoint> cursor;bool finished=false;};
    bool active(quint64 token) const;
    using Weak=std::weak_ptr<ExecutionContext>;
    Sessions& m_sessions;
    Ports m_ports;
    Host m_host;
    std::map<Weak,Claim,std::owner_less<Weak>> m_claims;
    std::map<quint64,Frame> m_frames;
    quint64 m_next=0;
    bool m_stopped=false;
};
