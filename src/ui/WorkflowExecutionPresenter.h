#pragma once
#include "app/PointerFeedbackSettings.h"
#include "model/WorkflowExecutionHighlight.h"
#include "ui/editors/WorkflowMatchFeedbackOverlay.h"
#include <QObject>
#include <QPointer>
#include <QPoint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
class WorkflowEditorPanel;
class Workflow;
class ExecutionContext;
class QPixmap;
struct FeatureRunSession;
enum class BlockProgressKind;
enum class LogLineKind;

// GUI presentation of validated engine events. Native feedback is a port;
// session scheduling, engine lifecycle and MainWindow remain outside this view.
class WorkflowExecutionPresenter : public QObject {
    friend class WorkflowExecutionUiChecks;
public:
    struct Pulse {
        QString profileId;std::string featureId;QPoint point;RunPointerFeedbackKind kind;
        std::optional<ClickPointerFeedbackSettings> settings;
        std::shared_ptr<ExecutionContext> context;
        std::optional<PointerFeedbackFrame> frame;
    };
    struct Ports {
        std::function<void(const Pulse&)> pulse;
        std::function<std::optional<PointerFeedbackFrame>(const std::shared_ptr<ExecutionContext>&)> frame;
    };
    struct Host {
        std::function<FeatureRunSession*(QObject*)> session;
        // Read-only predicates; callbacks below may retire/replace the session.
        std::function<bool(const FeatureRunSession&)> displayed,details;
        std::function<void(const FeatureRunSession&,const QString&,LogLineKind)> log;
        std::function<void()> refresh;
        std::function<void(FeatureRunSession&)> matched;
        std::function<void(FeatureRunSession&,int)> failed;
    };
    explicit WorkflowExecutionPresenter(WorkflowEditorPanel&,QObject* parent=nullptr);
    WorkflowExecutionPresenter(WorkflowEditorPanel&,Ports,QObject* parent=nullptr);
    void setHost(Host host){m_host=std::move(host);}
    void log(QObject*,const QString&);
    void prepared(QObject*,std::shared_ptr<Workflow>,std::shared_ptr<ExecutionContext>);
    void started(QObject*);
    void blockStarted(QObject*,int,const QString&);
    void progress(QObject*,int,BlockProgressKind);
    void match(QObject*,int,double,double,const QPixmap&,bool,bool,int,int);
    void pointer(QObject*,int,int);
    void blockFinished(QObject*,int,bool,const QString&,qint64,qint64,int);
    void attempt(QObject*,int,int,double,double,bool);
    void failureHandling(QObject*,int,int,int);
    void returnToPrevious(QObject*,int,int);
    void highlight(FeatureRunSession&,int,WorkflowExecutionHighlight);
    static WorkflowExecutionHighlight mapHighlight(const FeatureRunSession&,int,WorkflowExecutionHighlight);
private:
    struct Ticket;
    Ticket ticket(QObject*) const;
    static void show(Ticket&,int,WorkflowExecutionHighlight);
    static void pulse(Ticket&,Pulse);
    QPointer<WorkflowEditorPanel> m_editor;
    Ports m_ports;
    Host m_host;
};
