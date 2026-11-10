#include "app/MouseCenterLock.h"
#include "app/MouseLockClaims.h"
#include <atomic>
#include <limits>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
namespace {
std::atomic<unsigned> g_syntheticDepth{0};
#ifdef _WIN32
HHOOK g_mouseHook=nullptr;POINT g_lockPoint{};bool g_appliedOurClip=false,g_savedClip=false;RECT g_preLockClip{};
LRESULT CALLBACK mouseHookProc(int code,WPARAM kind,LPARAM data){
    if(code!=HC_ACTION || MouseCenterLock::syntheticPointerOperationActive())return CallNextHookEx(g_mouseHook,code,kind,data);
    if(kind==WM_MOUSEMOVE){const auto* info=reinterpret_cast<MSLLHOOKSTRUCT*>(data);
        if(info && !(info->flags&LLMHF_INJECTED)){
            MouseCenterLock::claims()->refresh();
            if(g_appliedOurClip){SetCursorPos(g_lockPoint.x,g_lockPoint.y);return 1;}
        }}
    return CallNextHookEx(g_mouseHook,code,kind,data);
}
bool enabled(){return qEnvironmentVariableIntValue("PIPBONG_SELF_TEST_UI")!=1
    && qEnvironmentVariableIntValue("PIPBONG_SMOKE_TEST")!=1 && qEnvironmentVariableIntValue("PIPBONG_WORKFLOW_DRY_RUN")!=1;}
void apply(QPoint point){
    if(!enabled() || MouseCenterLock::syntheticPointerOperationActive() || point.x()==std::numeric_limits<int>::max() || point.y()==std::numeric_limits<int>::max())return;
    if(!g_savedClip){RECT before{};if(GetClipCursor(&before)){g_preLockClip=before;g_savedClip=true;}}
    g_lockPoint={point.x(),point.y()};const RECT clip{point.x(),point.y(),point.x()+1,point.y()+1};
    if(ClipCursor(&clip)){g_appliedOurClip=true;SetCursorPos(point.x(),point.y());}
    if(!g_mouseHook)g_mouseHook=SetWindowsHookExW(WH_MOUSE_LL,mouseHookProc,GetModuleHandleW(nullptr),0);
}
void release(){
    if(g_mouseHook){UnhookWindowsHookEx(g_mouseHook);g_mouseHook=nullptr;}
    if(g_appliedOurClip){if(g_savedClip)ClipCursor(&g_preLockClip);else ClipCursor(nullptr);}
    g_appliedOurClip=false;g_savedClip=false;
}
#endif
}
std::shared_ptr<MouseLockClaims> MouseCenterLock::claims(){
#ifdef _WIN32
    static auto book=std::make_shared<MouseLockClaims>(MouseLockClaims::Backend{apply,release});
#else
    static auto book=std::make_shared<MouseLockClaims>(MouseLockClaims::Backend{});
#endif
    return book;
}
void MouseCenterLock::releaseAll(){claims()->clear();}
bool MouseCenterLock::isActive(){return claims()->winner().has_value();}
void MouseCenterLock::beginSyntheticPointerOperation(){g_syntheticDepth.fetch_add(1,std::memory_order_relaxed);}
void MouseCenterLock::endSyntheticPointerOperation(){auto depth=g_syntheticDepth.load(std::memory_order_relaxed);
    while(depth && !g_syntheticDepth.compare_exchange_weak(depth,depth-1,std::memory_order_relaxed)){} }
bool MouseCenterLock::syntheticPointerOperationActive(){return g_syntheticDepth.load(std::memory_order_relaxed)>0;}
