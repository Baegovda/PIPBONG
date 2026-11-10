#include "ui/editors/PointerFeedbackLayers.h"
#include "ui/editors/FeedbackCaptureBarrier.h"
#include "ui/editors/FeedbackHideBatch.h"
#include <future>
#include <stdexcept>
#include <iostream>


namespace {
bool check(bool value,const char* what){if(!value)std::cerr<<"Pointer feedback FAILED: "<<what<<'\n';return value;}
}
bool runPointerFeedbackChecks() {
    if(!check(ownedPointerFeedbackWindow(42,42,7,7,true)
        && !ownedPointerFeedbackWindow(43,42,7,7,true) && !ownedPointerFeedbackWindow(0,42,7,7,true)
        && !ownedPointerFeedbackWindow(42,42,7,8,true) && !ownedPointerFeedbackWindow(42,42,0,0,true)
        && !ownedPointerFeedbackWindow(42,42,7,7,false),"foreign/reused/invalid overlay HWND identity rejected before render/hide/destroy"))return false;
    PointerFeedbackLayers layers;
    auto first=std::make_shared<int>(1),second=std::make_shared<int>(2);
    PointerFeedbackOwner a{"profile-A","same",first},b{"profile-B","same",second};
    PointerFeedbackFrame left{11,101,QRect(-1900,-1000,1280,720)},right{22,202,QRect(100,200,1600,900)};
    const auto one=layers.observe(a,left),two=layers.observe(b,right);
    if(!check(one.token && two.token!=one.token && layers.size()==2 && layers.find(one.token)->frame==left
        && layers.find(two.token)->frame==right && layers.observe(a,left).token==one.token,"negative-origin peer frames and stable owner identity"))return false;
    auto moved=left;moved.clientBounds.translate(90,80);const auto successor=layers.observe(a,moved);
    if(!check(successor.retired==std::vector<std::uint64_t>{one.token} && !layers.find(one.token)
        && layers.find(two.token) && layers.find(successor.token)->frame==moved,"moving one owner retires only its old frame"))return false;
    PointerFeedbackOwner replacement{"profile-A","same",second};
    const auto fresh=layers.observe(replacement,moved);
    if(!check(fresh.token!=successor.token && layers.find(successor.token),"new context with same profile/feature/frame gets its own layer"))return false;
    auto invalid=moved;invalid.process=0;const auto count=layers.size();
    if(!check(!layers.observe(a,invalid).token && layers.size()==count,"invalid frame cannot retire valid peers"))return false;
    std::weak_ptr<int> weak=first;first.reset();a.lifetime.reset();
    if(!check(!weak.expired(),"visible old run retains owner lease against address reuse"))return false;
    layers.retire(successor.token);
    if(!check(weak.expired(),"retired layer releases context lease"))return false;
    for(int i=0;i<20;++i)layers.observe({QString::number(i),"feature",{}},right);
    if(!check(layers.size()==16 && !layers.find(two.token),"bounded layer count evicts oldest token"))return false;
    if(!check(pointerFeedbackBindingMatches({L"Game",L"C:\\APP\\game.exe"},L"My Game Window",L"c:/app/game.exe")
        && !pointerFeedbackBindingMatches({L"Game",L"C:\\APP\\game.exe"},L"Other",L"c:/app/game.exe")
        && !pointerFeedbackBindingMatches({L"Game",L"C:\\APP\\game.exe"},L"Game",L"c:/other/game.exe")
        && !pointerFeedbackBindingMatches({L"Game",L"C:\\APP\\game.exe"},L"Game",L"")
        && !pointerFeedbackBindingMatches({L"",L""},L"Game",L""),"title/process constraints reject stale/foreign/unreadable targets"))return false;
    FeedbackCaptureBarrier barrier;int published=0;
    {
        auto capture=barrier.capture([]{return std::vector<int>{1,2};});
        if(!check(capture.second.size()==2 && !barrier.publish([&]{++published;}),"capture registration blocks later publication"))return false;
        {
            auto other=barrier.capture([]{return 3;});
            auto worker=std::async(std::launch::async,[&]{return barrier.publish([&]{++published;});});
            if(!check(!worker.get(),"concurrent capture blocks worker publication without waiting on GUI"))return false;
        }
        if(!check(!barrier.publish([&]{++published;}),"one remaining capture lease still blocks"))return false;
    }
    if(!check(barrier.publish([&]{++published;}) && published==1,"last capture release permits publication"))return false;
    try{barrier.capture([]()->int{throw std::runtime_error("snapshot");});return false;}catch(const std::runtime_error&){}
    if(!check(barrier.publish([]{}),"throwing snapshot releases capture registration"))return false;
    {
        auto capture=barrier.capture([]{return 0;});
        auto worker=std::async(std::launch::async,[lease=std::move(capture.first)]()mutable{FeedbackCaptureBarrier::Lease owned(std::move(lease));});worker.get();
        if(!check(barrier.publish([]{}),"capture lease may retire on another thread"))return false;
    }
    std::uint64_t now=1000;std::vector<unsigned> waits;std::vector<std::uint64_t> tokens;
    FeedbackHidePorts ports{[&]{return now;},[](quintptr window){return window!=99;},
        [&](const auto& request,unsigned remaining){waits.push_back(remaining);tokens.push_back(request.token);now+=80;return true;}};
    if(!check(hideFeedbackBatch({{99,1},{11,2},{22,3}},ports) && waits==std::vector<unsigned>{200,120}
        && tokens==std::vector<std::uint64_t>{2,3},"retired HWND skipped; batch shares one timeout budget and owned tokens"))return false;
    waits.clear();now=1000;
    if(!check(!hideFeedbackBatch({{11,1},{22,2},{33,3},{44,4}},ports) && waits==std::vector<unsigned>{200,120,40},"deadline exhausted before fourth hide"))return false;
    now=1000;ports.hide=[](const auto&,unsigned){return false;};
    if(!check(!hideFeedbackBatch({{11,1}},ports),"failed hide rejects capture readiness"))return false;
    std::cout<<"Pointer feedback: owner/context/frame isolation, retired leases, bounded layers, title/process constraints, multiworker capture barrier, callback exceptions and shared200ms hide budget OK (recorded ports; no native overlay/input)\n";
    return true;
}
