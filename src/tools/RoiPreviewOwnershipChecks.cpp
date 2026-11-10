#include "ui/editors/RoiPreviewOverlay.h"
#include "ui/editors/ImageFindEditor.h"
#include "ui/editors/BlockEditorDialog.h"
#include "app/FirstTemplateRoiController.h"
#include "app/FeatureRunSession.h"
#include "model/Project.h"
#include <QApplication>
#include <iostream>

class RoiPreviewOwnershipChecks {
    using Overlay=RoiPreviewOverlay;
    struct Fixture {
        bool visible=false,editable=false,showResult=true;
        int selections=0,dismissals=0,lastIndex=-1;
        Overlay::NativeRequest active{};
        std::vector<Overlay::NativeRequest> shown;
        std::vector<Overlay::VisibilityHandler> hidden;
        std::function<void()> onShow,onDismiss,onQuery;
        Overlay::Backend previous;
        Fixture(){
            Overlay::Backend backend;
            backend.show=[this](const auto& request){
                const auto callback=onShow;if(callback)callback();
                if(!request.current())return false;
                if(visible)hidden.push_back(active.visibility);
                visible=false;shown.push_back(request);
                if(!showResult)return false;
                active=request;visible=true;editable=request.editable.enabled;return true;
            };
            backend.visible=[this]{const auto callback=onQuery;if(callback)callback();return visible;};
            backend.editable=[this]{return visible && editable;};
            backend.dismiss=[this]{
                ++dismissals;const auto callback=visible?active.visibility:Overlay::VisibilityHandler{};
                visible=false;editable=false;const auto after=onDismiss;if(after)after();if(callback)callback(false);
            };
            backend.select=[this](int index){++selections;lastIndex=index;};
            previous=Overlay::replaceBackendForChecks(std::move(backend));
        }
        ~Fixture(){onDismiss={};onShow={};onQuery={};Overlay::dismissAll();Overlay::replaceBackendForChecks(std::move(previous));}
        bool show(const Overlay::Owner& owner,Overlay::VisibilityHandler visibility={},Overlay::EditableOptions options={}){
            options.owner=owner;options.enabled=true;options.fixedTargetBounds=QRect(-500,-300,1000,800);
            return Overlay::show(SearchArea::CustomRegion,{-400,-200,200,100},{},{{-400,-200,200,100}},nullptr,std::move(visibility),options);
        }
    };
public:
    static bool run(){
        if(QApplication::platformName()!="offscreen")return false;
        const auto fail=[](const char* text){std::cerr<<"ROI preview ownership FAILED: "<<text<<'\n';return false;};
        {
            Fixture f;const auto a=std::make_shared<int>(1),b=std::make_shared<int>(2);int aHidden=0,bHidden=0,edited=0,selected=0,confirmed=0,added=0,deleted=0;
            Overlay::EditableOptions options;options.onRoiEdited=[&](int,const auto&){++edited;};options.onRoiSelected=[&](int){++selected;};
            options.onConfirm=[&]{++confirmed;};options.onAdd=[&]{++added;};options.onDelete=[&]{++deleted;};
            if(!f.show(a,[&](bool value){if(!value)++aHidden;},options))return fail("first show");
            const auto old=f.active;
            if(!f.show(b,[&](bool value){if(!value)++bHidden;},options))return fail("replacement show");
            if(Overlay::isVisible(a) || Overlay::isEditable(a) || !Overlay::isVisible(b) || !Overlay::isEditable(b))return fail("visibility/editability owner isolation");
            Overlay::setActiveRoiIndex(3,a);Overlay::dismiss(a);
            if(f.selections || f.dismissals || !f.visible)return fail("old owner cannot select/dismiss replacement");
            old.editable.onRoiEdited(0,{1,2,3,4});old.editable.onRoiSelected(0);old.editable.onConfirm();old.editable.onAdd();old.editable.onDelete();
            if(edited || selected || confirmed || added || deleted)return fail("retired action callbacks inert");
            f.hidden.front()(false);
            if(aHidden!=1 || bHidden || !Overlay::isVisible(b))return fail("late hidden event cannot retire replacement");
            f.active.editable.onRoiEdited(0,{1,2,3,4});f.active.editable.onRoiSelected(0);f.active.editable.onConfirm();f.active.editable.onAdd();f.active.editable.onDelete();
            Overlay::setActiveRoiIndex(2,b);
            if(edited!=1 || selected!=1 || confirmed!=1 || added!=1 || deleted!=1 || f.lastIndex!=2)return fail("current actions and selection retained");
            Overlay::dismissAll();if(Overlay::isVisible() || bHidden!=1)return fail("explicit global shutdown retained");
        }
        {
            Fixture f;const auto a=std::make_shared<int>(1),b=std::make_shared<int>(2);int leaked=0;
            f.show(a);f.onShow=[&]{if(Overlay::isVisible(b) || Overlay::isEditable(b))++leaked;Overlay::setActiveRoiIndex(9,b);};
            if(!f.show(b) || leaked || f.selections || !Overlay::isVisible(b))return fail("pending owner cannot query/select previous physical window");
        }
        {
            Fixture f;auto owner=std::make_shared<int>(0);int actions=0;Overlay::EditableOptions options;options.onConfirm=[&]{++actions;};
            f.show(owner,{},options);const auto old=f.active;owner.reset();
            if(old.current())return fail("backend must not retain caller owner");old.editable.onConfirm();if(actions)return fail("expired owner callback");
            const auto fresh=std::make_shared<int>(0);f.showResult=false;
            if(f.show(fresh) || f.visible || Overlay::isVisible(fresh) || Overlay::isEditable(fresh))return fail("failed replacement clears partial native resource");
        }
        {
            Fixture f;const auto a=std::make_shared<int>(1),b=std::make_shared<int>(2);bool nested=false;
            f.onShow=[&]{if(!nested){nested=true;f.show(b);}};
            if(f.show(a) || !Overlay::isVisible(b) || Overlay::isVisible(a) || f.shown.size()!=1)return fail("reentrant show cannot publish obsolete window");
            f.onShow={};f.onDismiss=[&]{f.onDismiss={};f.show(a);};Overlay::dismiss(b);
            if(!Overlay::isVisible(a) || !f.visible)return fail("dismissal reentry preserves new owner");
        }
        {
            Fixture f;const auto owner=std::make_shared<int>(0);f.show(owner);
            f.onQuery=[&]{f.onQuery={};f.show(owner);};
            if(Overlay::hide(owner) || !Overlay::isVisible(owner) || f.dismissals)return fail("stale hide query cannot close newer same-owner generation");
        }
        {
            Fixture f;ImageFindBlock first,second,replacement;
            first.customRegionsWindowPercent={{10,20,30,40}};second.customRegionsWindowPercent={{5,5,20,20}};
            auto a=std::make_unique<ImageFindEditor>(&first,QString(),nullptr,true);
            auto b=std::make_unique<ImageFindEditor>(&second,QString(),nullptr,true);
            const auto ownerA=a->m_roiPreviewOwner,ownerB=b->m_roiPreviewOwner;
            f.show(ownerA,{},a->makeRoiPreviewEditableOptions());const auto old=f.active;
            f.show(ownerB,{},b->makeRoiPreviewEditableOptions());
            a->setBlock(&replacement);a.reset();
            if(!Overlay::isVisible(ownerB) || f.dismissals)return fail("actual unrelated editor rebind/destructor cannot close preview");
            const auto before=second.toJson();old.editable.onRoiEdited(0,{1,2,30,40});old.editable.onConfirm();
            if(second.toJson()!=before || !Overlay::isVisible(ownerB))return fail("destroyed editor callbacks cannot edit/close current document");
            b.reset();if(f.visible || Overlay::isVisible(ownerB) || f.dismissals!=1)return fail("actual owning editor destructor releases its preview");
        }
        {
            Fixture f;ImageFindBlock block;
            auto dialog=std::make_unique<BlockEditorDialog>(&block,QString());
            const auto other=std::make_shared<int>(0);f.show(other);
            static_cast<QDialog*>(dialog.get())->reject();dialog.reset();
            if(!Overlay::isVisible(other) || f.dismissals)return fail("actual block-dialog reject/child teardown cannot close unrelated preview");
        }
        {
            Fixture f;QWidget hostWidget;
            const auto make=[&](Project& project,FeatureRunSession& session){
                auto* feature=project.addFeature("first ROI");feature->setId("roi");feature->setEditFirstTemplateRoiOnStart(true);
                auto block=std::make_unique<ImageFindBlock>();block->templatePaths={"never-executed.png"};block->customRegionsWindowPercent={{10,20,30,40}};feature->workflow().addBlock(std::move(block));
                session.featureId="roi";session.profileId="owner";session.engine=std::make_unique<WorkflowEngine>();
                FirstTemplateRoiController::Host callbacks;
                callbacks.workspace=[&project]{return FirstTemplateRoiController::Workspace{&project,"owner",1};};
                callbacks.session=[&session](const auto&){return &session;};
                auto controller=std::make_unique<FirstTemplateRoiController>(hostWidget,std::move(callbacks));
                controller->m_ports.bounds=[](const auto&) {return QRect(-500,-300,1000,800);};
                if(!controller->begin(session,feature))return std::unique_ptr<FirstTemplateRoiController>();
                return controller;
            };
            Project projectA,projectB;FeatureRunSession sessionA,sessionB;
            auto a=make(projectA,sessionA);auto b=make(projectB,sessionB);
            if(!a || !b || !f.visible)return fail("actual native-port controller presentation");
            const auto latest=f.active;a.reset();
            if(!f.visible || !latest.current() || f.dismissals)return fail("same QWidget controller owners stay independent");
            b.reset();if(f.visible || f.dismissals!=1)return fail("owning first-ROI controller destructor releases preview");
        }
        std::cout<<"  ROI preview ownership: cross-owner close/selection, stale edit/toolbar/visibility callbacks, expired leases, failed/reentrant publication, query generations and actual editor/controller retirement OK (offscreen; recorded native window backend; no overlay/input/hooks)\n";
        return true;
    }
};
bool runRoiPreviewOwnershipChecks(){return RoiPreviewOwnershipChecks::run();}
