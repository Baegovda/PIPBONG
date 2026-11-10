#pragma once
#include "ui/TargetWindowBindingRole.h"
#include <QObject>
#include <QString>
#include <functional>
#include <memory>
#include <optional>
#ifdef _WIN32
#include <windows.h>
#endif
class QWidget;
class FeatureHotkeyGateScope;

// User-initiated picking/highlighting. The workspace owns persistence; native interaction
// is behind ports so tests never install hooks, move windows, activate them or send input.
class TargetWindowActionsController : public QObject {
    friend class TargetWindowActionsChecks;
public:
    using Role = TargetWindowBindingRole;
#ifdef _WIN32
    using Handle = HWND;
#else
    using Handle = void*;
#endif
    struct Selection {
        Handle hwnd = nullptr;
        QString title, processPath;
        unsigned long processId = 0;
    };
    struct Snapshot {
        QString profileId,mainTitle,subTitle,mainProcessPath,subProcessPath;
        quint64 generation = 0;
        bool globalDefault = true;
        Handle preferred = nullptr;
    };
    using Completion = std::function<void(std::optional<Selection>)>;
    struct Ports {
        std::function<void(Role,Completion)> pick;
        std::function<void()> cancel;
        std::function<std::optional<Selection>(const Snapshot&,Role)> list;
        std::function<std::optional<Selection>(Handle)> read;
        std::function<Handle(const QString&,const QString&)> find;
        std::function<bool(const Selection&,Role,bool wave)> flash;
    };
    struct Host {
        std::function<Snapshot()> snapshot;
        std::function<QString(bool changingBinding)> blockedReason;
        std::function<bool()> checkpoint;
        std::function<QString(const Snapshot&,Role,const Selection&)> commit;
        std::function<void(const Selection&,Role)> committed;
        std::function<void(const QString&)> persistent,status,warning;
    };
    explicit TargetWindowActionsController(QWidget* parent);
    TargetWindowActionsController(Ports ports,QObject* parent=nullptr);
    ~TargetWindowActionsController() override;
    void setHost(Host host) { m_host=std::move(host); }
    void pick(Role role,bool fromList=false);
    void show(Role role);
    void cancel();
    bool pending() const { return m_pending; }
private:
    void finish(quint64 sequence,Snapshot source,Role role,bool fromList,std::optional<Selection> selection);
    bool sameDocument(const Snapshot& source) const;
    QString blocked(bool changing) const;
    void status(const QString& text) const;
    void warning(const QString& text) const;
    static bool sameProcess(const Selection& expected,const Selection& actual);
    Ports m_ports;
    Host m_host;
    quint64 m_sequence=0;
    bool m_pending=false;
    std::unique_ptr<FeatureHotkeyGateScope> m_gate;
};
