#pragma once
#include <QObject>
#include <QPointer>
#include <QHash>
#include <functional>
#include <memory>
#include <vector>
#include "ui/FeatureListPanel.h"
class WorkflowEditorPanel;
class Project;
struct FeatureRunSession;

class WorkspaceEditorController : public QObject {
    friend class WorkspaceEditorChecks;
public:
    struct Source {
        std::shared_ptr<Project> project;
        QString owner,modelOwner,path,directory,profileName,selectedId,previewId,previewDirectory;
        quint64 generation=0;
        Feature* selected=nullptr;
        std::shared_ptr<Feature> preview;
    };
    struct Host {
        // Source/session/pending queries are read-only; remaining callbacks may reenter.
        std::function<Source()> source;
        std::function<FeatureRunSession*(const std::string&)> session;
        std::function<std::vector<std::string>()> sessions,pending;
        std::function<void(const FeatureRunSession*)> timing;
        std::function<void()> clearPreview,changed;
        std::function<bool()> autoSelect;
    };
    WorkspaceEditorController(WorkflowEditorPanel&,FeatureListPanel&,Host,QObject* parent=nullptr);
    void refresh();
    void selectionChanged();
    void saveSelection();
    void restoreSelection();
    void selectRunning(Feature*);
    bool displayed(const FeatureRunSession*)const;
    QHash<QString,FeatureRunVisualKind> visuals()const;
    static QString preferenceKey(const QString& path);
private:
    struct Ticket;
    Ticket ticket()const;
    static bool same(const Source&,const Source&);
    static bool ready(const Source& s){return s.modelOwner.isEmpty() || s.modelOwner==s.owner;}
    static bool owns(const Source&,const FeatureRunSession&);
    Host m_host;
    QPointer<WorkflowEditorPanel> m_editor;
    QPointer<FeatureListPanel> m_list;
};
