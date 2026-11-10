#pragma once
#include <QObject>
#include <QPoint>
#include <QRect>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include "app/MouseLockClaims.h"
class QTimer;
struct FeatureRunSession;
class ExecutionContext;

// Coordinates session intent with owner-scoped claims; backend never selects a target.
class SessionMouseLockController : public QObject {
    friend class SessionMouseLockUiChecks;
public:
    struct Ports {
        std::function<std::optional<QPoint>()> cursor;
        std::function<std::optional<QRect>(const std::shared_ptr<ExecutionContext>&)> targetRect; // read-only owned frame
        std::shared_ptr<MouseLockClaims> locks;
    };
    using Sessions = std::map<std::string, FeatureRunSession>;
    explicit SessionMouseLockController(Sessions& sessions, QObject* parent = nullptr);
    SessionMouseLockController(Sessions& sessions, Ports ports, QObject* parent = nullptr);
    ~SessionMouseLockController()override;
    void setFailureHandler(std::function<void(const FeatureRunSession&)> handler) { m_failed = std::move(handler); }
    static bool hasFeatureLock(const FeatureRunSession& session);
    static bool earlyWindow(const FeatureRunSession& session);
    void capturePosition(FeatureRunSession& session);
    bool engageEarly(FeatureRunSession& session);
    void updateFromMatch(FeatureRunSession& session);
    void releaseEarly(FeatureRunSession& session);
    void syncEarly(FeatureRunSession& session);
    void blockFailed(FeatureRunSession& session);
    void engageFeature(FeatureRunSession& session);
    void reconcile();
    void schedule();
    void tick();
    void shutdown();
private:
    MouseLockClaims::Owner owner(const FeatureRunSession&)const;
    void retain(const FeatureRunSession* extra=nullptr);
    MouseLockClaims::Provider pointProvider(const MouseLockClaims::Owner&,std::optional<QPoint>,bool center,QPoint offset)const;
    static bool eligible(const FeatureRunSession&);
    void capturePositionImpl(FeatureRunSession&);
    void releaseSession(FeatureRunSession&);
    Sessions& m_sessions;
    Ports m_ports;
    QTimer* m_timer = nullptr;
    std::function<void(const FeatureRunSession&)> m_failed;
    std::shared_ptr<int> m_lifetime=std::make_shared<int>(0);
    bool m_stopped=false;
};
