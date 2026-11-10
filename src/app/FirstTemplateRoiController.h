#pragma once
#include "ui/editors/RoiPreviewOverlay.h"
#include <QObject>
#include <memory>
class Project;
class Feature;
struct FeatureRunSession;
class ExecutionContext;

// Transactional first-ROI preview. Callbacks reacquire the document/session;
// native preview geometry and presentation remain replaceable ports.
class FirstTemplateRoiController:public QObject {
    friend class FirstTemplateRoiChecks;
    friend class RoiPreviewOwnershipChecks;
public:
    struct Workspace {Project* project=nullptr;QString owner;quint64 generation=0;};
    struct Presentation {
        SearchArea searchArea;CaptureRegion legacy;PercentRegion percent;
        std::vector<CaptureRegion> regions;
        RoiPreviewOverlay::VisibilityHandler visibility;
        RoiPreviewOverlay::EditableOptions editable;
    };
    struct Host {
        // Queries are read-only; publication/launch/cancel may replace owners.
        std::function<Workspace()> workspace;
        std::function<FeatureRunSession*(const std::string&)> session;
        std::function<void(const std::string&)> cancel;
        std::function<void()> changed;
        std::function<void(FeatureRunSession&,Feature&)> launch;
    };
    struct Ports {
        std::function<QRect(const std::shared_ptr<ExecutionContext>&)> bounds;
        std::function<bool(const Presentation&)> show;
        std::function<void()> dismiss;
    };
    FirstTemplateRoiController(QWidget&,Host);
    FirstTemplateRoiController(Host,Ports,QObject* parent=nullptr);
    ~FirstTemplateRoiController()override;
    bool begin(FeatureRunSession&,Feature*);
    // Called by lifecycle teardown; never calls back into host cancellation.
    void retire(FeatureRunSession&);
private:
    struct Request;
    void cancel(std::shared_ptr<Request>,bool dismiss);
    Host m_host;Ports m_ports;
    std::shared_ptr<Request> m_current;
};
