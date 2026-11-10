#pragma once
#include <QObject>
#include <functional>
class QWidget;

class ProgramSettingsController : public QObject {
    friend class ProgramSettingsChecks;
public:
    struct Source {const void* project=nullptr;QString owner;quint64 generation=0;};
    using Current=std::function<bool()>;
    struct Host {
        // These queries do not mutate/pump events. All other callbacks may.
        std::function<bool()> allowed;
        std::function<Source()> source;
        std::function<bool()> saveProfile;
        std::function<void()> worker,closePolicy,updatePolicy,logLimit;
        std::function<bool(const std::function<bool()>&)> handoff;
        std::function<QString()> lastError;
        std::function<void()> quit;
    };
    struct Ports {
        std::function<bool(Current)> edit;
        std::function<bool()> sync,elevated,confirmRestart,launchElevated;
        std::function<void(const QString&)> warning;
    };
    ProgramSettingsController(QWidget&,Host,Ports={},QObject* parent=nullptr);
    void open();
    bool busy()const{return m_busy;}
private:
    Host m_host;
    Ports m_ports;
    bool m_busy=false;
};
