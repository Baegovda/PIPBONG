#pragma once
#include "app/TargetWindowDetailsController.h"
#include <QObject>
#include <QPointer>
#include <functional>
class QToolButton;
class QTimer;
class TargetWindowDetailPanel;

// Owns target-panel presentation and its refresh timer; never picks or moves a window.
class TargetPanelController : public QObject {
public:
    using Request=TargetWindowDetailsController::Request;
    struct Controls {
        QPointer<QToolButton> pickMain,listMain,pickSub,listSub,showMain,showSub,pinMain,pinSub;
    };
    using Snapshot=std::function<Request()>;
    TargetPanelController(TargetWindowDetailPanel&,Controls,TargetWindowDetailsController&,Snapshot,QObject* parent=nullptr);
    void applyControls(bool globalDefault);
    void refresh();
    void start();
    void stop();
    bool current(const Request&)const;
    static bool sameSource(const Request&,const Request&);
private:
    QPointer<TargetWindowDetailPanel> m_panel;
    Controls m_controls;
    TargetWindowDetailsController& m_details;
    Snapshot m_snapshot;
    QTimer* m_timer;
    bool m_refreshing=false,m_stopped=false;
};
