#include "app/FirstTemplateRoiController.h"
#include "app/FeatureRunSession.h"
#include "core/capture/TargetWindowBinding.h"
#include <limits>
#include "model/Project.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include <QPointer>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace {
using Controller=FirstTemplateRoiController;
bool same(const Controller::Workspace& a,const Controller::Workspace& b){return a.project==b.project && a.owner==b.owner && a.generation==b.generation;}
bool finite(const PercentRegion& r){return std::isfinite(r.x)&&std::isfinite(r.y)&&std::isfinite(r.width)&&std::isfinite(r.height)&&r.width>0&&r.height>0;}
CaptureRegion physical(const PercentRegion& p,const QRect& bounds){
    if(!finite(p))return {};
    const double x=std::clamp(p.x,0.0,100.0),y=std::clamp(p.y,0.0,100.0);
    const double w=std::clamp(p.width,0.0,100.0-x),h=std::clamp(p.height,0.0,100.0-y);
    return {bounds.x()+static_cast<int>(std::lround(bounds.width()*x/100)),bounds.y()+static_cast<int>(std::lround(bounds.height()*y/100)),
        static_cast<int>(std::lround(bounds.width()*w/100)),static_cast<int>(std::lround(bounds.height()*h/100))};
}
}
struct Controller::Request {
    QPointer<Controller> life;Host host;Ports ports;Workspace source;
    std::string id;Feature* feature=nullptr;ImageFindBlock* block=nullptr;int blockIndex=-1;
    std::weak_ptr<ExecutionContext> context;QPointer<WorkflowEngine> engine;
    std::pair<std::wstring,std::wstring> binding;
    FeatureRunMode mode;bool pending=true,confirming=false,edited=false;QRect bounds;
    nlohmann::json baseline;
    std::vector<PercentRegion> staged;std::vector<size_t> indices;
    FeatureRunSession* owned()const{
        if(!life || !host.session)return nullptr;
        const auto ctx=context.lock();if(!ctx)return nullptr;
        auto* session=host.session(id);
        return life && session && session->profileId==source.owner && session->sessionContext==ctx
            && session->engine.get()==engine.data() && !session->userStopRequested && !ctx->shouldStop()?session:nullptr;
    }
    ImageFindBlock* current()const{
        const auto ctx=context.lock();
        if(!life || !host.workspace || !ctx || ctx->targetWindowBinding()!=binding)return nullptr;
        const auto first=host.workspace();if(!life || !same(source,first) || !owned())return nullptr;
        const auto latest=host.workspace();if(!life || !same(source,latest))return nullptr;
        auto* active=source.project?source.project->featureById(id):nullptr;
        if(active!=feature || !active || !active->enabled() || active->runMode()!=mode || !active->editFirstTemplateRoiOnStart())return nullptr;
        const auto& blocks=active->workflow().blocks();
        if(blockIndex<0 || blockIndex>=static_cast<int>(blocks.size()) || blocks[blockIndex].get()!=block)return nullptr;
        return ctx->targetWindowBinding()==binding && block->toJson()==baseline?block:nullptr;
    }
};
Controller::FirstTemplateRoiController(QWidget& owner,Host host):QObject(&owner),m_host(std::move(host)){
    const QPointer<QWidget> widget(&owner);
    m_ports.bounds=[](const std::shared_ptr<ExecutionContext>& context)->QRect{
        if(qEnvironmentVariableIntValue("PIPBONG_SELF_TEST_UI")==1 || qEnvironmentVariableIntValue("PIPBONG_SMOKE_TEST")==1
            || qEnvironmentVariableIntValue("PIPBONG_WORKFLOW_DRY_RUN")==1 || !context || context->shouldStop())return {};
#ifdef _WIN32
        const auto binding=context->targetWindowBinding();ScreenCapture::TargetWindowInfo info;
        if(!ScreenCapture::queryWindowInfo(context->targetWindow(),info) || !info.visible || info.minimized || info.width<=0 || info.height<=0
            || qint64(info.x)+info.width>std::numeric_limits<int>::max() || qint64(info.y)+info.height>std::numeric_limits<int>::max()
            || !targetWindowBindingMatches(binding,info.title,info.processPath) || binding!=context->targetWindowBinding())return {};
        return QRect(info.x,info.y,info.width,info.height);
#else
        return {};
#endif
    };
    const auto previewOwner=std::make_shared<int>(0);
    m_ports.show=[widget,previewOwner](const Presentation& p){
        auto editable=p.editable;editable.owner=previewOwner;
        return widget && RoiPreviewOverlay::show(p.searchArea,p.legacy,p.percent,p.regions,widget,p.visibility,editable);
    };
    m_ports.dismiss=[previewOwner] {RoiPreviewOverlay::dismiss(previewOwner);};
}
Controller::FirstTemplateRoiController(Host host,Ports ports,QObject* parent):QObject(parent),m_host(std::move(host)),m_ports(std::move(ports)){}
Controller::~FirstTemplateRoiController(){if(m_current && (m_current->pending || m_current->confirming)){m_current->pending=false;m_current->confirming=false;if(m_ports.dismiss)m_ports.dismiss();}}
void Controller::retire(FeatureRunSession& session){
    const auto request=m_current;
    if(!request || (!request->pending && !request->confirming) || request->id!=session.featureId || request->source.owner!=session.profileId || request->context.lock()!=session.sessionContext)return;
    request->pending=false;request->confirming=false;session.awaitingFirstTemplateRoi=false;m_current.reset();
    // Teardown must not synchronously reenter the registry through a native
    // dismiss callback. A newer preview already replaces the old native one.
    const QPointer<Controller> self(this);const auto dismiss=request->ports.dismiss;
    QTimer::singleShot(0,this,[self,dismiss]{if(self && !self->m_current && dismiss)dismiss();});
}
void Controller::cancel(std::shared_ptr<Request> request,bool dismiss){
    if(!request->pending && !request->confirming)return;request->pending=false;request->confirming=false;
    if(m_current==request){m_current.reset();if(dismiss && request->ports.dismiss)request->ports.dismiss();}
    if(!request->life)return;
    // Dismissal may synchronously install another preview. Never erase it.
    if(m_current && m_current->context.lock()==request->context.lock())return;
    if(auto* session=request->owned()){session->awaitingFirstTemplateRoi=false;if(request->host.cancel)request->host.cancel(request->id);}
}
bool Controller::begin(FeatureRunSession& session,Feature* feature){
    if(!feature || !feature->enabled() || !feature->editFirstTemplateRoiOnStart() || !m_host.workspace || !m_ports.bounds || !m_ports.show)return false;
    const QPointer<Controller> life(this);const Host host=m_host;const Ports ports=m_ports;
    const auto source=host.workspace();if(!life)return true;
    if(!source.project || source.owner!=session.profileId || source.project->featureById(session.featureId)!=feature)return false;
    if(m_current && (m_current->pending || m_current->confirming) && m_current->id==session.featureId && m_current->context.lock()==session.sessionContext)return true;
    auto request=std::make_shared<Request>();request->life=this;request->host=host;request->ports=ports;request->source=source;
    request->id=session.featureId;request->feature=feature;request->engine=session.engine.get();request->mode=feature->runMode();
    const auto& blocks=feature->workflow().blocks();
    for(size_t i=0;i<blocks.size();++i)if(blocks[i] && blocks[i]->type()==BlockType::ImageFind){
        auto* block=static_cast<ImageFindBlock*>(blocks[i].get());
        if(block->hasTemplates() && !block->customRegionsWindowPercent.empty()){request->block=block;request->blockIndex=static_cast<int>(i);break;}
    }
    if(!request->block)return false;
    if(!session.sessionContext){
        session.sessionContext=std::make_shared<ExecutionContext>();
        session.sessionContext->setTargetWindowBindingForWorker(session.lockedCaptureTargetTitle,session.lockedCaptureTargetProcessPath);
    }
    const auto context=session.sessionContext;request->context=context;request->binding=context->targetWindowBinding();
    request->baseline=request->block->toJson();request->staged=request->block->customRegionsWindowPercent;
    const auto area=request->block->searchArea;const auto percent=request->block->percentRegion;
    request->bounds=ports.bounds(context);if(!life)return true;
    if(!request->current()){if(life)cancel(request,false);return true;}
    if(!request->bounds.isValid())return false;
    Presentation p;p.searchArea=area;p.percent=percent;
    for(size_t i=0;i<request->staged.size();++i){
        const auto r=physical(request->staged[i],request->bounds);
        if(r.width<2 || r.height<2)continue;
        p.regions.push_back(r);request->indices.push_back(i);
    }
    if(p.regions.empty())return false;
    p.legacy=p.regions.front();p.editable.enabled=true;p.editable.activeIndex=0;p.editable.fixedTargetBounds=request->bounds;
    p.editable.onRoiEdited=[request](int index,const CaptureRegion& region){
        if(!request->life || !request->pending)return;
        if(!request->current()){if(request->life)request->life->cancel(request,true);return;}
        if(index<0 || index>=static_cast<int>(request->indices.size()) || region.width<2 || region.height<2)return;
        const QRect clipped=QRect(region.x,region.y,region.width,region.height).intersected(request->bounds);
        if(clipped.width()<2 || clipped.height()<2)return;
        auto& value=request->staged[request->indices[index]];
        value={(clipped.x()-request->bounds.x())*100.0/request->bounds.width(),(clipped.y()-request->bounds.y())*100.0/request->bounds.height(),
            clipped.width()*100.0/request->bounds.width(),clipped.height()*100.0/request->bounds.height()};request->edited=true;
    };
    p.editable.onConfirm=[request]{
        if(!request->life || !request->pending)return;
        if(!request->current()){if(request->life)request->life->cancel(request,true);return;}
        request->pending=false;request->confirming=true;
        if(request->ports.dismiss)request->ports.dismiss();
        if(!request->life || request->life->m_current!=request)return;
        auto* block=request->current();if(!block){if(request->life)request->life->cancel(request,false);return;}
        if(request->edited){
            block->customRegionsWindowPercent=request->staged;block->customRegionsAnchoredToTargetWindow=true;
            block->customRegions.clear();block->customRegion={};block->searchArea=SearchArea::CustomRegion;
            request->baseline=block->toJson();
            if(request->host.changed)request->host.changed();
        }
        if(!request->life || request->life->m_current!=request)return;
        if(!request->current()){if(request->life)request->life->cancel(request,false);return;}
        request->confirming=false;
        auto* active=request->owned();if(active){active->awaitingFirstTemplateRoi=false;if(request->host.launch)request->host.launch(*active,*request->feature);}
        if(request->life && request->life->m_current==request)request->life->m_current.reset();
    };
    p.visibility=[request](bool visible){if(!visible && request->life && request->pending)request->life->cancel(request,false);};
    if(m_current)cancel(m_current,true);
    if(!life)return true;
    if(!request->current()){if(life)cancel(request,false);return true;}
    m_current=request;if(auto* active=request->owned())active->awaitingFirstTemplateRoi=true;
    const bool shown=ports.show(p);
    if(!request->pending)return true;
    if(!life)return true;
    if(!request->current()){if(life)cancel(request,true);return true;}
    if(!shown){request->pending=false;if(auto* active=request->owned())active->awaitingFirstTemplateRoi=false;if(m_current==request)m_current.reset();}
    return shown;
}
