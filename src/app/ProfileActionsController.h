#pragma once
#include "app/ProfileManager.h"
#include <QObject>
#include <functional>
#include <memory>

class ProfileActionsController : public QObject {
public:
    enum class Action { Add, Edit, Remove };
    struct Request { Action action=Action::Edit; QString id,owner; quint64 generation=0; ProfileManager::Edit edit; };
    struct Result { Action action=Action::Edit; QString id,message; bool success=false,cancelled=false; };
    struct Host {
        std::function<bool()> canBegin;
        std::function<quint64()> generation;
        std::function<bool(const QString&,bool stopRuns)> checkpoint;
        std::function<void(const Result&)> changed;
        std::function<void()> finished;
    };
    ProfileActionsController(ProfileManager&,Host,QObject* parent=nullptr);
    Result execute(Request request);
    bool busy() const { return *m_busy; }
private:
    ProfileManager& m_profiles;
    Host m_host;
    std::shared_ptr<bool> m_busy=std::make_shared<bool>(false);
};
