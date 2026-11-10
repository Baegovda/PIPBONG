#pragma once
#include <QObject>
#include <QString>
#include <functional>
#include <optional>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#endif
class QTimer;

// Owns center-pin scheduling and preference publication, independent of MainWindow/widgets.
class TargetCenterPinController : public QObject {
    friend class TargetCenterPinUiChecks;
public:
    enum class Role { Main, Sub };
#ifdef _WIN32
    using Handle=HWND;
#else
    using Handle=void*;
#endif
    struct Snapshot {
        QString profileId,mainTitle,subTitle,mainPath,subPath;
        quint64 generation=0;
        bool globalDefault=true,main=false,sub=false,blocked=false,movementAllowed=true;
    };
    struct Window {
        Handle hwnd=nullptr;
        QString processPath;
        unsigned long processId=0;
        bool visible=true,minimized=false;
    };
    struct Ports {
        std::function<std::optional<Window>(const QString&,const QString&)> resolve;
        std::function<bool(const Window&,bool force)> move;
    };
    struct Host {
        std::function<Snapshot()> snapshot;
        std::function<QString(const Snapshot&,bool main,bool sub)> save;
        std::function<void(bool main,bool sub)> view;
        std::function<void()> changed;
        std::function<void(const QString&)> status,warning;
    };
    explicit TargetCenterPinController(QObject* parent=nullptr);
    TargetCenterPinController(Ports ports,QObject* parent=nullptr);
    void setHost(Host host) {m_host=std::move(host);}
    void reconcile();
    void toggle(Role role,bool checked);
    void tick();
    void stop();
private:
    static bool sameScope(const Snapshot& a,const Snapshot& b);
    void remember(const Snapshot& state);
    Ports m_ports;
    Host m_host;
    QTimer* m_periodic=nullptr;
    QTimer* m_queued=nullptr;
    QString m_profileId;
    quint64 m_generation=0;
    unsigned m_forceMask=0;
    bool m_ticking=false;
};
