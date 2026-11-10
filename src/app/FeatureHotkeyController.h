#pragma once
#include <QObject>
#include <QHash>
#include <QStringList>
#include <functional>
#include <utility>
class Project;
struct FeatureRunSession;

// GUI-thread dispatch and registration feedback. Native hooks remain in HotkeyManager;
// session scheduling remains in the lifecycle coordinator. No MainWindow/widgets.
class FeatureHotkeyController : public QObject {
    friend class FeatureHotkeyUiChecks;
public:
    enum class Block { None, Unavailable, Editor, Target };
    struct Workspace { Project* project=nullptr; QString owner; quint64 generation=0; };
    struct Gate { bool unavailable=true,editor=false,targetRequired=false,profileMatches=true,
        backgroundAllowed=false,ownWindow=false; };
    struct Registration { bool keyboardHook=true,mouseHook=true; QStringList failedNames; };
    struct Host {
        std::function<Workspace()> workspace;
        std::function<Gate(const QString&)> gate;
        std::function<void(Block)> blocked;
        std::function<bool(const std::string&)> running;
        std::function<FeatureRunSession*(const std::string&)> session;
        std::function<bool(const FeatureRunSession&)> holdBlocked;
        std::function<void()> enterBurst,leaveBurst,foregroundReady,holdVisuals,prune;
        std::function<void(const std::string&)> start,stop,erase,queueStart,cancelStart,queueFinish;
        std::function<void(FeatureRunSession&)> repeat,release,stopLane;
        std::function<Registration(const Project&)> registerBindings;
        std::function<void(const QString&,bool)> log; // bool: error rather than warning
    };
    explicit FeatureHotkeyController(QObject* parent=nullptr):QObject(parent){}
    void setHost(Host host){m_host=std::move(host);}
    static Block block(const Gate& value);
    void triggered(const QString& id);
    void holdStarted(const QString& id);
    void holdEnded(const QString& id);
    void sync();
    bool acceptQueuedStart(const QString& id);
    QStringList visibleQueuedStarts()const;
private:
    Host m_host;
    QHash<QString,Workspace> m_pending;
};
