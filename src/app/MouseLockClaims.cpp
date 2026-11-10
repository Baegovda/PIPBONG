#include "app/MouseLockClaims.h"
#include <algorithm>
#include <limits>
#include <map>
namespace {
bool same(const std::weak_ptr<void>& a,const std::weak_ptr<void>& b){return !a.owner_before(b) && !b.owner_before(a);}
}
bool MouseLockClaims::Owner::live()const{return !controller.expired() && !context.expired() && !feature.isEmpty();}
bool MouseLockClaims::Owner::same(const Owner& b)const {
    return ::same(controller,b.controller) && ::same(context,b.context) && profile==b.profile && feature==b.feature && binding==b.binding && run==b.run;
}
struct MouseLockClaims::State {
    struct Claim {Token token;Owner owner;Kind kind;Provider point;quint64 order;};
    struct Order {Owner owner;quint64 sequence;};
    Backend backend;std::map<Token,Claim> claims;std::vector<Order> owners;std::optional<Winner> winner;
    Token next=0;quint64 nextOrder=0,revision=0;int batches=0;bool alive=true,syncing=false,applied=false;
};
MouseLockClaims::MouseLockClaims(Backend backend):m_state(std::make_shared<State>()){m_state->backend=std::move(backend);}
MouseLockClaims::~MouseLockClaims(){auto s=m_state;s->alive=false;s->claims.clear();s->owners.clear();s->winner.reset();
    if(s->applied){s->applied=false;try{if(s->backend.release)s->backend.release();}catch(...){}}}
MouseLockClaims::Batch::Batch(std::shared_ptr<State> s):m_state(std::move(s)){++m_state->batches;}
MouseLockClaims::Batch::~Batch(){if(m_state && --m_state->batches==0)MouseLockClaims::sync(m_state);}
MouseLockClaims::Batch MouseLockClaims::batch(){return Batch(m_state);}
MouseLockClaims::Token MouseLockClaims::upsert(Token old,const Owner& owner,Kind kind,Provider point) {
    auto s=m_state;if(!s->alive || !owner.live())return 0;
    auto record=std::find_if(s->owners.begin(),s->owners.end(),[&](const auto& r){return r.owner.same(owner);});
    if(record==s->owners.end()){s->owners.push_back({owner,++s->nextOrder});record=std::prev(s->owners.end());}
    auto claim=s->claims.find(old);
    if(claim==s->claims.end() || !claim->second.owner.same(owner) || claim->second.kind!=kind)
        claim=std::find_if(s->claims.begin(),s->claims.end(),[&](const auto& pair){return pair.second.owner.same(owner) && pair.second.kind==kind;});
    Token token=claim==s->claims.end()?++s->next:claim->first;if(!token)token=++s->next;
    s->claims.insert_or_assign(token,State::Claim{token,owner,kind,std::move(point),record->sequence});++s->revision;sync(s);return token;
}
void MouseLockClaims::release(Token token){auto s=m_state;if(!s->alive)return;if(s->claims.erase(token)){++s->revision;sync(s);}}
void MouseLockClaims::retain(const std::weak_ptr<void>& controller,const std::vector<Owner>& known) {
    auto s=m_state;if(!s->alive)return;const auto stale=[&](const Owner& owner){return ::same(controller,owner.controller)
        && std::none_of(known.begin(),known.end(),[&](const auto& current){return current.same(owner);});};
    bool changed=false;for(auto it=s->claims.begin();it!=s->claims.end();)if(stale(it->second.owner)){it=s->claims.erase(it);changed=true;}else ++it;
    const auto count=s->owners.size();s->owners.erase(std::remove_if(s->owners.begin(),s->owners.end(),[&](const auto& r){return stale(r.owner);}),s->owners.end());
    if(changed || count!=s->owners.size()){++s->revision;sync(s);}
}
void MouseLockClaims::releaseController(const std::weak_ptr<void>& controller){retain(controller,{});}
void MouseLockClaims::clear(){auto s=m_state;if(!s->alive)return;s->claims.clear();s->owners.clear();++s->revision;sync(s);}
void MouseLockClaims::refresh(){sync(m_state);}
bool MouseLockClaims::hasClaims(const std::weak_ptr<void>& controller)const {
    return std::any_of(m_state->claims.begin(),m_state->claims.end(),[&](const auto& r){return ::same(controller,r.second.owner.controller);});
}
std::optional<MouseLockClaims::Winner> MouseLockClaims::winner()const{return m_state->winner;}
void MouseLockClaims::sync(const std::shared_ptr<State>& s) {
    if(!s->alive || s->batches || s->syncing)return;s->syncing=true;
    struct Reset{std::shared_ptr<State> s;~Reset(){s->syncing=false;}} reset{s};
    for(int attempt=0;attempt<8 && s->alive;++attempt){
        const auto revision=s->revision;std::vector<State::Claim> candidates;for(const auto& pair:s->claims)candidates.push_back(pair.second);
        std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.order!=b.order?a.order>b.order:a.kind>b.kind;});
        std::optional<Winner> next;bool replaced=false;
        for(const auto& claim:candidates){if(!claim.owner.live())continue;std::optional<QPoint> point;
            try{if(claim.point)point=claim.point();}catch(...){}
            if(!s->alive)return;if(revision!=s->revision){replaced=true;break;}
            if(point && claim.owner.live() && point->x()<std::numeric_limits<int>::max() && point->y()<std::numeric_limits<int>::max()){
                next=Winner{claim.token,claim.owner,claim.kind,*point};break;}}
        if(replaced)continue;s->winner=next;
        if(next){s->applied=true;const auto apply=s->backend.apply;
            try{if(apply)apply(next->point);}catch(...){s->winner.reset();s->applied=false;try{if(s->backend.release)s->backend.release();}catch(...){}}}
        else if(s->applied){s->applied=false;const auto release=s->backend.release;try{if(release)release();}catch(...){}}
        if(!s->alive || revision==s->revision)return;
    }
    if(s->alive && s->applied){s->winner.reset();s->applied=false;try{if(s->backend.release)s->backend.release();}catch(...){}}
}
