#include "ui/WorkspaceEditorController.h"
#include "ui/WorkflowEditorPanel.h"
#include "app/FeatureRunSession.h"
#include "app/RunSessionPolicySnapshot.h"
#include "model/Project.h"
#include <QFileInfo>
#include <QSettings>

using Controller=WorkspaceEditorController;
bool Controller::same(const Source& a,const Source& b) {
    return a.project==b.project && a.owner==b.owner && a.modelOwner==b.modelOwner && a.generation==b.generation && a.path==b.path
        && a.selected==b.selected && a.selectedId==b.selectedId && a.preview==b.preview && a.previewId==b.previewId
        && a.directory==b.directory && a.profileName==b.profileName && a.previewDirectory==b.previewDirectory;
}
bool Controller::owns(const Source& view,const FeatureRunSession& s) {
    return ready(view) && (!s.profileId.isEmpty()?s.profileId==view.owner:view.project && view.project->featureById(s.featureId));
}
struct Controller::Ticket {
    QPointer<const Controller> life;QPointer<WorkflowEditorPanel> editor;QPointer<FeatureListPanel> list;Host host;Source source;
    bool valid()const{return life && editor && list && host.source && Controller::same(source,host.source());}
};
Controller::Ticket Controller::ticket()const {
    return {this,m_editor,m_list,m_host,m_editor && m_list && m_host.source?m_host.source():Source{}};
}
Controller::WorkspaceEditorController(WorkflowEditorPanel& editor,FeatureListPanel& list,Host host,QObject* parent)
    :QObject(parent),m_host(std::move(host)),m_editor(&editor),m_list(&list){}
QString Controller::preferenceKey(const QString& path) {
    return QStringLiteral("project/selectedFeatureId/%1").arg(QString::number(qHash(QFileInfo(path).absoluteFilePath())));
}
void Controller::saveSelection() {
    const auto t=ticket();if(!t.valid() || !ready(t.source) || t.source.path.isEmpty() || t.source.selectedId.isEmpty() || t.source.preview)return;
    QSettings().setValue(preferenceKey(t.source.path),t.source.selectedId);
}
void Controller::restoreSelection() {
    const auto t=ticket();if(!t.valid() || !ready(t.source) || t.source.path.isEmpty() || !t.source.project)return;
    QString id=QSettings().value(preferenceKey(t.source.path)).toString();
    if(!t.source.project->featureById(id.toStdString()))id=t.source.project->features().empty()?QString():QString::fromStdString(t.source.project->featureAt(0)->id());
    if(!id.isEmpty())t.list->selectFeatureById(id);
}
void Controller::selectionChanged() {
    auto t=ticket();if(!t.valid() || !ready(t.source))return;
    if(t.host.clearPreview)t.host.clearPreview();
    if(!t.life || !t.list || !t.editor)return;const auto now=t.host.source?t.host.source():Source{};
    if(now.project!=t.source.project || now.owner!=t.source.owner || now.generation!=t.source.generation)return;
    saveSelection();if(t.life)refresh();
}
bool Controller::displayed(const FeatureRunSession* s)const {
    const auto t=ticket();return s && t.valid() && !t.source.preview && owns(t.source,*s)
        && t.source.selected && t.source.selectedId==QString::fromStdString(s->featureId) && t.editor->displayedFeature()==t.source.selected;
}
void Controller::selectRunning(Feature* feature) {
    auto t=ticket();if(!t.valid() || !ready(t.source) || !feature || !t.source.project || !t.host.autoSelect || !t.host.autoSelect())return;
    const auto id=feature->id();if(t.source.project->featureById(id)!=feature)return;
    const bool preview=bool(t.source.preview);
    if(preview && t.host.clearPreview)t.host.clearPreview();
    if(!t.life || !t.list || !t.editor)return;const auto now=t.host.source?t.host.source():Source{};
    if(now.project!=t.source.project || now.owner!=t.source.owner || now.generation!=t.source.generation || !now.project || now.project->featureById(id)!=feature)return;
    if(t.list->selectedFeatureId()!=QString::fromStdString(id))t.list->selectFeatureById(QString::fromStdString(id));
    else if(preview)refresh();
}
void Controller::refresh() {
    const auto t=ticket();if(!t.valid() || !ready(t.source))return;
    const bool preview=bool(t.source.preview) && !t.source.previewId.isEmpty();
    const auto* feature=preview?t.source.preview.get():t.source.selected;
    const QString owner=preview?t.source.previewId:t.source.owner;
    const QString path=QFileInfo(t.source.path).absoluteFilePath();
    const QString scope=(preview?QStringLiteral("library:"):QStringLiteral("profile:"))
        +QString::number(owner.size())+QLatin1Char(':')+owner+QString::number(path.size())+QLatin1Char(':')+path;
    t.editor->setProjectDirectory(preview?t.source.previewDirectory:t.source.directory,false);if(!t.valid())return;
    t.editor->setProfileDisplayName(preview?tr("라이브러리"):t.source.profileName);if(!t.valid())return;
    std::shared_ptr<void> lifetime=preview?std::shared_ptr<void>(t.source.preview):std::shared_ptr<void>(t.source.project);
    t.editor->bindFeature(const_cast<Feature*>(feature),std::move(lifetime),scope);if(!t.valid())return;
    if(preview){
        t.editor->clearLoopTiming();if(!t.valid())return;
        t.editor->clearExecutionHighlight();if(!t.valid())return;
        t.editor->setEditingEnabled(false);if(!t.valid())return;
    } else {
        auto* s=feature && t.host.session?t.host.session(t.source.selectedId.toStdString()):nullptr;
        if(s && owns(t.source,*s)){
            const auto context=s->sessionContext;const auto owner=s->profileId;const auto* engine=s->engine.get();
            const auto generation=s->holdRepeatGeneration;
            const auto current=[&]{if(!t.valid())return false;auto* value=t.host.session(t.source.selectedId.toStdString());return t.valid() && value==s && value
                && value->sessionContext==context && value->profileId==owner && value->engine.get()==engine && value->holdRepeatGeneration==generation;};
            if(s->runningBlockIndex>=0){t.editor->setActiveBlockIndex(s->runningBlockIndex,s->runningBlockHighlight);if(!current())return;}
            if(t.host.timing)t.host.timing(s);if(!current())return;
        } else if(!feature) {
            t.editor->clearLoopTiming();if(!t.valid())return;
            t.editor->clearExecutionHighlight();if(!t.valid())return;
        }
    }
    if(t.host.changed)t.host.changed();
}
QHash<QString,FeatureRunVisualKind> Controller::visuals()const {
    const auto t=ticket();QHash<QString,FeatureRunVisualKind> out;if(!t.valid() || !ready(t.source) || !t.host.sessions || !t.host.session)return out;
    for(const auto& id:t.host.sessions()){
        const auto* s=t.host.session(id);if(!s || !owns(t.source,*s))continue;
        const auto qid=QString::fromStdString(id);
        if(s->runningMode==FeatureRunMode::Hold){if(s->holdRunActive)out.insert(qid,FeatureRunVisualKind::ActiveRun);continue;}
        if(!SessionRunPolicy::isSessionActive(runSessionPolicyInput(*s)))continue;
        if(s->runningMode!=FeatureRunMode::Trigger || !s->repeatSession){out.insert(qid,FeatureRunVisualKind::ActiveRun);continue;}
        if(s->triggerPhase==TriggerSessionPhase::Monitoring)out.insert(qid,FeatureRunVisualKind::TriggerWatch);
        else if(s->triggerPhase==TriggerSessionPhase::Cooldown)out.insert(qid,FeatureRunVisualKind::TriggerCooldown);
        else if(s->triggerPhase==TriggerSessionPhase::RunningAction)out.insert(qid,FeatureRunVisualKind::ActiveRun);
    }
    if(t.host.pending)for(const auto& id:t.host.pending())if(!out.contains(QString::fromStdString(id)))out.insert(QString::fromStdString(id),FeatureRunVisualKind::ActiveRun);
    return t.valid()?out:QHash<QString,FeatureRunVisualKind>{};
}
