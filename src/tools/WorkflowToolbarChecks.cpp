#include "app/MainWindow.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include "ui/WorkflowEditorPanel.h"
#include <QApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QPushButton>
#include <QFile>
#include <iostream>

class WorkflowToolbarUiChecks {
public:
    static bool run() {
        MainWindow window;window.m_foregroundMonitor->stop();window.m_triggerMonitor->stop();
        window.ensurePolished();window.grab();
        auto* panel=window.m_workflowEditor;
        QList<QPushButton*> buttons=panel->m_addTypeButtons;buttons<<panel->m_removeAllWaitButton<<panel->m_insertWaitBetweenButton<<panel->m_loopRegionsButton;
        QJsonArray reads;bool contained=true;
        for(auto* button:buttons) {
            const QRect rect(button->mapTo(&window,QPoint()),button->size());
            const QRect pane(button->mapTo(panel,QPoint()),button->size());
            const bool valid=window.rect().contains(rect) && panel->rect().contains(pane);
            contained &= valid;
            reads.append(QJsonObject{{"text",button->text()},{"left",rect.left()},{"right",rect.right()},
                {"top",rect.top()},{"bottom",rect.bottom()},{"width",rect.width()},{"hint",button->minimumSizeHint().width()},{"contained",valid}});
        }
        const auto path=QDir(qEnvironmentVariable("PIPBONG_DATA_DIR")).filePath("workflow-toolbar-bounds.json");
        QFile report(path);if(!report.open(QIODevice::WriteOnly))return false;
        report.write(QJsonDocument(QJsonObject{{"windowWidth",window.width()},{"windowHeight",window.height()},
            {"panelWidth",panel->width()},{"buttons",reads},{"contained",contained}}).toJson());report.close();
        if(!window.prepareForShutdown())return false;
        if(!contained)std::cerr<<"Workflow toolbar FAILED: button outside window/editor; read-only bounds in "<<path.toStdString()<<'\n';
        return contained;
    }
};
bool runWorkflowToolbarChecks(){return WorkflowToolbarUiChecks::run();}
