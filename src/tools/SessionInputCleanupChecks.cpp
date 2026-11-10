#include "app/SessionInputCleanupController.h"
#include "app/FeatureRunSession.h"
#include "model/Feature.h"
#include "core/input/InputSimulator.h"
#include "core/capture/ScreenCapture.h"
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
namespace {
using Controller=SessionInputCleanupController;
bool check(bool value,const char* message){if(!value)std::cerr<<"Session input cleanup FAILED: "<<message<<'\n';return value;}
bool routing() {
    int finds=0,releases=0,cleanups=0,lastKey=0,moves=0;Controller::Binding searched;QPoint moved;
    std::optional<Controller::Window> candidate=Controller::Window{reinterpret_cast<HWND>(quintptr(1)),44,L"renamed owner",L"C:\\Apps\\Owner.exe",true};
    Controller controller({[&](const auto& binding){++finds;searched=binding;return candidate;},
        [&](const auto&,int key){++releases;lastKey=key;},[&](int key){++cleanups;lastKey=key;},
        []()->std::optional<QPoint>{return QPoint(-2010,250);},[&](QPoint p){++moves;moved=p;}});
    Feature feature("owned");feature.setRunMode(FeatureRunMode::Hold);HotkeyBinding hotkey;hotkey.virtualKey=VK_XBUTTON1;feature.setHotkey(hotkey);
    const auto make=[&] {
        FeatureRunSession s;s.runningMode=FeatureRunMode::Hold;s.sessionContext=std::make_shared<ExecutionContext>();
        Controller::bind(s,feature);s.lockedCaptureTargetTitle=L"Owner";s.lockedCaptureTargetProcessPath=L"c:/apps/owner.EXE";
        s.sessionContext->setTargetWindowBindingForWorker(L"wrong active",L"wrong.exe");
        s.sessionContext->noteSyntheticMouseDown(MouseButton::Back);s.sessionContext->noteSyntheticMouseUp(MouseButton::Back);return s;
    };
    auto session=make();hotkey.virtualKey=VK_LBUTTON;feature.setHotkey(hotkey);
    if(!check(controller.releaseHold(session)==Controller::Outcome::TargetAttempted && lastKey==VK_XBUTTON1 && releases==1
        && finds==1 && cleanups==0 && searched.title==L"Owner" && searched.path==L"c:/apps/owner.EXE",
        "started hotkey and locked title/process pair override edited model and foreign context pair"))return false;
    if(!check(controller.releaseHold(session)==Controller::Outcome::Duplicate && releases==1 && finds==1,"duplicate teardown does not repeat release"))return false;
    hotkey.virtualKey=VK_XBUTTON1;feature.setHotkey(hotkey);
    auto mismatch=make();candidate->path=L"other.exe";
    if(!check(controller.releaseHold(mismatch)==Controller::Outcome::CleanupAttempted && releases==1 && cleanups==1,"same-title different-executable candidate falls back to injected-state cleanup"))return false;
    auto absent=make();candidate.reset();controller.releaseHold(absent);
    if(!check(releases==1 && cleanups==2,"missing owner target never inherits active global target"))return false;
    auto legacy=make();legacy.lockedCaptureTargetTitle.clear();legacy.lockedCaptureTargetProcessPath.clear();
    legacy.sessionContext->setTargetWindowBindingForWorker(L"Owner",L"C:/Apps/Owner.exe");
    candidate=Controller::Window{reinterpret_cast<HWND>(quintptr(1)),44,L"Owner",L"C:/Apps/Owner.exe",true};controller.releaseHold(legacy);
    if(!check(releases==2 && searched.title==L"Owner","older session can use its own prepared context binding"))return false;
    auto constrained=make();constrained.lockedCaptureTargetProcessPath.clear();
    constrained.sessionContext->setTargetWindowBindingForWorker(L"Owner",L"C:/Apps/Owner.exe");candidate->path=L"foreign.exe";
    const int constrainedReleases=releases;controller.releaseHold(constrained);
    if(!check(releases==constrainedReleases && searched.path==L"C:/Apps/Owner.exe", "title-only lock retains a known executable from the same prepared title"))return false;
    auto unowned=make();unowned.sessionContext=std::make_shared<ExecutionContext>();const int old=finds+releases+cleanups;
    if(!check(controller.releaseHold(unowned)==Controller::Outcome::NotInjected && finds+releases+cleanups==old,"physical-only hotkey creates no injected-UP cleanup"))return false;
    auto key=make();key.holdReleaseVirtualKey='A';key.sessionContext->noteSyntheticKeyDown('A');key.sessionContext->noteSyntheticKeyUp('A');
    const int oldFinds=finds;controller.releaseHold(key);
    if(!check(finds==oldFinds && lastKey=='A' && cleanups==4,"keyboard injected-state cleanup does not resolve or post to a client window"))return false;
    auto empty=make();empty.holdReleaseBindingCaptured=false;
    if(!check(controller.releaseHold(empty)==Controller::Outcome::Unavailable && !empty.holdHotkeyReleasedToTarget,"missing start snapshot never guesses another model's hotkey"))return false;
    FeatureRunSession cursor;cursor.hasRunStartCursorPosition=true;controller.captureCursor(cursor);
    if(!check(!cursor.hasRunStartCursorPosition,"disabled cursor restore clears stale point"))return false;
    cursor.restoreMousePositionOnEnd=true;controller.captureCursor(cursor);controller.restoreCursor(cursor);
    return check(moves==1 && moved==QPoint(-2010,250),"cursor restore uses captured physical point through injected ports");
}
bool nativeBackend() {
    std::vector<INPUT> inputs;int posts=0,validations=0,held=0;bool valid=true,point=true,posted=true,replaceOnPoint=false;
    HWND receiver=nullptr;UINT message=0;WPARAM keys=0;LPARAM position=0;
    InputSimulator::KeyboardBackendForTest keyboard{[&](int vk){return held && vk==held;},[&](const INPUT* p,unsigned n){inputs.insert(inputs.end(),p,p+n);}};
    InputSimulator::ScopedKeyboardBackendForTest record(keyboard);
    InputSimulator::HoldReleaseBackendForTest native;
    native.validate=[&](HWND,unsigned long pid,const std::wstring& path){++validations;return valid && pid==44 && path==L"owner.exe";};
    native.clientPoint=[&](HWND,int& x,int& y){x=-23;y=45;if(replaceOnPoint)valid=false;return point;};
    native.mouseState=[]()->WPARAM{return MK_CONTROL|MK_SHIFT|MK_LBUTTON|MK_RBUTTON|MK_MBUTTON|MK_XBUTTON1|MK_XBUTTON2;};
    native.post=[&](HWND hwnd,UINT msg,WPARAM state,LPARAM value){++posts;receiver=hwnd;message=msg;keys=state;position=value;return posted;};
    InputSimulator::ScopedHoldReleaseBackendForTest intercept(native);
    const HWND target=reinterpret_cast<HWND>(quintptr(77));
    for(const int key:{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2}) {
        inputs.clear();posts=0;validations=0;InputSimulator::releaseHoldHotkeyToTarget(target,key,44,L"owner.exe");
        const UINT expected=key==VK_LBUTTON?WM_LBUTTONUP:key==VK_RBUTTON?WM_RBUTTONUP:key==VK_MBUTTON?WM_MBUTTONUP:WM_XBUTTONUP;
        const WPARAM mask=key==VK_LBUTTON?MK_LBUTTON:key==VK_RBUTTON?MK_RBUTTON:key==VK_MBUTTON?MK_MBUTTON:key==VK_XBUTTON1?MK_XBUTTON1:MK_XBUTTON2;
        if(!check(posts==1 && receiver==target && message==expected && validations==2 && inputs.empty()
            && !(LOWORD(keys)&mask) && (LOWORD(keys)&MK_CONTROL) && (LOWORD(keys)&MK_SHIFT)
            && static_cast<short>(LOWORD(position))==-23 && static_cast<short>(HIWORD(position))==45,
            "production mouse-UP path validates PID/exe twice and posts signed client coordinates with released-button state cleared"))return false;
        if(key==VK_XBUTTON1 || key==VK_XBUTTON2)if(!check(HIWORD(keys)==(key==VK_XBUTTON1?XBUTTON1:XBUTTON2),"side-button identity occupies high word"))return false;
    }
    for(int scenario=0;scenario<4;++scenario) {
        valid=scenario!=0;point=scenario!=1;replaceOnPoint=scenario==2;posted=scenario!=3;inputs.clear();posts=0;
        InputSimulator::releaseHoldHotkeyToTarget(target,VK_XBUTTON1,44,L"owner.exe");
        if(!check(inputs.size()==1 && inputs.front().type==INPUT_MOUSE && inputs.front().mi.dwFlags==MOUSEEVENTF_XUP
            && inputs.front().mi.mouseData==XBUTTON1 && posts==(scenario==3?1:0),"stale/point-failed/reused/queue-failed mouse target retains global injected-UP cleanup without foreign posting"))return false;
    }
    inputs.clear();posts=0;validations=0;InputSimulator::releaseHoldHotkeyToTarget(target,'A',44,L"owner.exe");
    if(!check(inputs.size()==1 && (inputs.front().ki.dwFlags&KEYEVENTF_KEYUP) && posts==0 && validations==0,"keyboard cleanup remains an untracked synthetic-UP"))return false;
    for(const int side:{VK_LSHIFT,VK_RSHIFT,VK_LCONTROL,VK_RCONTROL,VK_LMENU,VK_RMENU}) {
        held=side;inputs.clear();InputSimulator::releaseHoldHotkeyToTarget(target,side,44,L"owner.exe");
        if(!check(inputs.empty(),"physically held modifiers remain protected in cleanup"))return false;
    }
    return true;
}
bool tracking() {
    ExecutionContext context;std::vector<INPUT> inputs;bool late=true,pausedDuringEmission=false;
    InputSimulator::KeyboardBackendForTest keyboard{[](int){return false;},[&](const INPUT* p,unsigned n){
        inputs.insert(inputs.end(),p,p+n);pausedDuringEmission=context.isPaused();
        context.pipbongEverInjectedVirtualKey('A'); // Callback reentry must not deadlock tracking.
        if(late){late=false;context.noteSyntheticKeyDown('B');}
    }};
    InputSimulator::ScopedKeyboardBackendForTest record(keyboard);
    context.beginRunKeyboardSessionIfNeeded();context.noteSyntheticKeyDown('A');context.setPaused(true);
    if(!check(pausedDuringEmission && inputs.size()==1 && context.hasPipbongSyntheticKeyDown('B'),"pause is published before cleanup and an in-flight tracking update survives detached restore"))return false;
    context.restoreRunHeldInput();context.endRunInputSession();
    if(!check(inputs.size()==2 && !context.hasPipbongSyntheticKeyDown('B') && !context.pipbongEverInjectedVirtualKey('A'),"repeat/end drains once and clears history without locking across emission"))return false;
    ExecutionContext shared;std::atomic<bool> start{false};std::atomic<int> writers{3},reads{0};std::vector<std::thread> threads;
    for(int w=0;w<3;++w)threads.emplace_back([&,w]{while(!start.load())std::this_thread::yield();for(int n=0;n<10000;++n){
        const int key='A'+w;shared.noteSyntheticKeyDown(key);shared.noteSyntheticMouseDown(MouseButton::Back);
        shared.noteSyntheticKeyUp(key);shared.noteSyntheticMouseUp(MouseButton::Back);
    }--writers;});
    for(int r=0;r<3;++r)threads.emplace_back([&]{while(!start.load())std::this_thread::yield();int n=0;do{
        shared.pipbongEverInjectedVirtualKey('A');shared.hasPipbongSyntheticKeyDown('B');shared.pipbongEverInjectedMouseButton(MouseButton::Back);
        shared.shouldSuppressUserInputInterrupt(VK_XBUTTON1);if((++n%47)==0)shared.endRunInputSession();++reads;
    }while(writers.load()>0 || n<1000);});
    start=true;for(auto& thread:threads)thread.join();shared.endRunInputSession();
    return check(reads>=3000 && !shared.hasPipbongSyntheticKeyDown('A') && !shared.pipbongEverInjectedMouseButton(MouseButton::Back),
        "concurrent worker tracking, GUI reads/suppression and inactive teardown finish coherently without native input");
}
bool nativeRead() {
    const HWND hwnd=GetForegroundWindow();ScreenCapture::TargetWindowInfo info;DWORD pid=0;
    if(!hwnd || !GetWindowThreadProcessId(hwnd,&pid) || !ScreenCapture::queryWindowInfo(hwnd,info) || info.processPath.empty()) {
        std::cout<<"Hold release native read: existing foreground metadata unavailable; native identity control skipped\n";return true;
    }
    if(!check(InputSimulator::holdReleaseTargetMatches(hwnd,pid,info.processPath)
        && !InputSimulator::holdReleaseTargetMatches(hwnd,pid^0x40000000UL,info.processPath)
        && !InputSimulator::holdReleaseTargetMatches(hwnd,pid,L"C:/__pipbong_absent__/wrong.exe"),
        "native read-only identity control accepts actual process and rejects wrong PID/executable"))return false;
    std::cout<<"Hold release native read: existing foreground PID/executable accepted, wrong identity rejected OK (no PostMessage/SendInput)\n";return true;
}
}
bool runSessionInputCleanupChecks() {
    if(!routing() || !nativeBackend() || !tracking() || !nativeRead())return false;
    std::cout<<"Session input cleanup: frozen hotkey/owner binding, no active-target lookup, strict candidate and native release guards, five mouse-UP messages, fallback cleanup/modifier protection, cursor ports and synchronized tracking/callback reentry OK (recorded input only)\n";
    return true;
}
