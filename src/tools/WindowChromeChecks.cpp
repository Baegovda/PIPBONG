#include "ui/WindowChromeController.h"
#include "ui/CustomTitleBar.h"
#include "app/MainWindow.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QMenuBar>
#include <QCheckBox>
#include <climits>
#include <cmath>
#include <limits>
#include <iostream>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

class WindowChromeChecks {
    using Controller=WindowChromeController;
    using Hit=Controller::Hit;
public:
static bool run() {
    const auto fail=[](const char* s){std::cerr<<"Window chrome FAILED: "<<s<<'\n';return false;};
    const Controller::Bounds r{-1800,-200,-800,600};const Controller::State resizable{};
    struct Case {QPoint p;Hit hit;};
    for(const auto& row:{Case{{-1800,-200},Hit::TopLeft},Case{{-801,-200},Hit::TopRight},
        Case{{-1800,599},Hit::BottomLeft},Case{{-801,599},Hit::BottomRight},
        Case{{-1800,200},Hit::Left},Case{{-801,200},Hit::Right},
        Case{{-1300,-200},Hit::Top},Case{{-1300,599},Hit::Bottom}})
        if(Controller::resizeHit(r,row.p,resizable)!=row.hit)return fail("eight physical edges on negative monitor");
    for(const auto& p:{QPoint(-1801,0),QPoint(-800,0),QPoint(-1800,-201),QPoint(-1800,600),QPoint(-1300,0),QPoint(-1790,-190)})
        if(Controller::resizeHit(r,p,resizable))return fail("half-open exterior/interior/border endpoint");
    if(Controller::resizeHit(r,{-1800,-200},{true,true,true,10})
        || Controller::resizeHit(r,{-1800,200},{false,false,true,10})
        || Controller::resizeHit(r,{-1300,-200},{false,true,false,10})
        || Controller::resizeHit(r,{-1800,-200},{false,false,false,10})
        || Controller::resizeHit({0,0,0,0},{0,0},resizable)
        || Controller::resizeHit({4,4,0,0},{2,2},resizable))return fail("maximized/fixed axes/invalid rectangle");
    if(Controller::resizeHit(r,{-1800,-200},{false,false,true,10})!=Hit::Top
        || Controller::resizeHit(r,{-1800,-200},{false,true,false,10})!=Hit::Left
        || Controller::resizeHit({INT_MIN,INT_MIN,INT_MAX,INT_MAX},{INT_MAX-1,0},resizable)!=Hit::Right)
        return fail("fixed-axis corner fallback and widened arithmetic");
#ifdef Q_OS_WIN
    QMainWindow window;quintptr handle=7;int frames=0,rounds=0,reads=0,captions=0;bool valid=true;
    Controller::Ports ports;ports.handle=[&]{return handle;};ports.rectangle=[&](auto)->std::optional<Controller::Bounds>{++reads;if(!valid)return {};return r;};
    ports.frame=[&](auto){++frames;};ports.round=[&](auto){++rounds;};ports.caption=[&](QPointF p){++captions;return p.y()<50;};
    ports.clientPoint=[&](auto,QPoint p)->std::optional<QPoint>{return p-QPoint(int(r.left),int(r.top));};ports.ratio=[]{return 1.0;};
    Controller controller(window,nullptr,ports);controller.shown();
    if(frames!=1 || rounds!=1)return fail("show delegates native setup once per invocation");
    MSG msg{};msg.hwnd=reinterpret_cast<HWND>(handle);msg.message=WM_NCHITTEST;msg.lParam=MAKELPARAM(-1800,-200);
    qintptr result=99;
    if(!controller.nativeEvent("windows_generic_MSG",&msg,&result) || result!=HTTOPLEFT || captions)return fail("adapter edge priority / signed negative LPARAM");
    msg.lParam=MAKELPARAM(-1300,-175);if(!controller.nativeEvent("windows_dispatch_MSG",&msg,&result) || result!=HTCAPTION)return fail("caption fallback");
    msg.lParam=MAKELPARAM(-1800,-201);result=99;if(controller.nativeEvent("windows_generic_MSG",&msg,&result) || result!=99)return fail("exterior caption/edge rejection");
    const int before=reads;msg.message=WM_MOUSEMOVE;
    if(controller.nativeEvent("windows_generic_MSG",&msg,&result) || controller.nativeEvent("unrelated",&msg,&result) || reads!=before)return fail("unrelated messages are read-free");
    msg.message=WM_NCHITTEST;msg.hwnd=reinterpret_cast<HWND>(99);
    if(controller.nativeEvent("windows_generic_MSG",&msg,&result) || reads!=before)return fail("foreign HWND is read-free");
    msg.hwnd=reinterpret_cast<HWND>(handle);valid=false;result=99;
    if(controller.nativeEvent("windows_generic_MSG",&msg,&result) || result!=99
        || controller.nativeEvent("windows_generic_MSG",nullptr,&result) || controller.nativeEvent("windows_generic_MSG",&msg,nullptr))return fail("failed rectangle/null input preserves output");
    controller.m_ports.frame=[&](auto){++handle;};controller.shown();if(rounds!=1)return fail("recreated handle skips stale corner setup");
    auto dying=std::make_unique<Controller>(window,nullptr,ports);
    dying->m_ports.frame=[&](auto){dying.reset();};auto* raw=dying.get();raw->shown();if(dying)return fail("deletion during frame setup");
    dying=std::make_unique<Controller>(window,nullptr,ports);dying->m_ports.rectangle=[&](auto)->std::optional<Controller::Bounds>{dying.reset();return r;};
    msg.hwnd=reinterpret_cast<HWND>(handle);raw=dying.get();if(raw->nativeEvent("windows_generic_MSG",&msg,&result) || dying)return fail("deletion during rectangle read");
    {
        QMainWindow scaled;Controller::Ports p;quintptr id=9;const QPoint origin(-2400,700);qreal dpr=1.25;int conversions=0,calls=0;
        p.handle=[&]{return id;};p.rectangle=[&](auto)->std::optional<Controller::Bounds>{return Controller::Bounds{origin.x(),origin.y(),origin.x()+1500,origin.y()+1000};};
        p.clientPoint=[&](auto,QPoint point)->std::optional<QPoint>{++conversions;return point-origin;};p.ratio=[&]{return dpr;};
        QPointF observed;p.caption=[&](QPointF point){++calls;observed=point;return point.x()>=100 && point.x()<200 && point.y()>=20 && point.y()<40;};
        Controller c(scaled,nullptr,p);MSG event{};event.hwnd=reinterpret_cast<HWND>(id);event.message=WM_NCHITTEST;
        const auto at=[&](int x,int y){event.lParam=MAKELPARAM(origin.x()+x,origin.y()+y);result=99;return c.nativeEvent("windows_generic_MSG",&event,&result);};
        for(const qreal ratio:{1.0,1.25,1.5,2.0}){
            dpr=ratio;if(!at(qRound(100*dpr),qRound(30*dpr)) || result!=HTCAPTION || std::abs(observed.x()-100)>0.001)return fail("native-screen to local logical caption across DPRs");
            if(at(qRound(100*dpr)-1,qRound(30*dpr)))return fail("fractional client coordinates must not round across caption edge");
        }
        const int converted=conversions;if(!at(0,0) || result!=HTTOPLEFT || conversions!=converted)return fail("resize edges stay physical and skip caption conversion");
        for(const qreal invalid:{0.0,-1.0,std::numeric_limits<qreal>::infinity(),std::numeric_limits<qreal>::quiet_NaN()}){
            dpr=invalid;const int before=calls;if(at(150,30) || result!=99 || calls!=before)return fail("invalid DPR fails closed");
        }
        dpr=1.25;c.m_ports.clientPoint=[](auto,QPoint)->std::optional<QPoint>{return {};};if(at(150,30) || result!=99)return fail("failed client conversion");
        c.m_ports.clientPoint=[&](auto,QPoint point)->std::optional<QPoint>{++id;return point-origin;};if(at(150,30) || result!=99)return fail("recreated HWND during conversion");
    }
#endif
    if(QApplication::platformName()!="offscreen")return fail("offscreen required for fixture");
    {
        MainWindow w;w.m_foregroundMonitor->stop();w.m_triggerMonitor->stop();
        int frame=0,round=0;w.m_windowChrome->m_ports.frame=[&](auto){++frame;};w.m_windowChrome->m_ports.round=[&](auto){++round;};
        w.m_windowChrome->m_ports.handle=[]{return quintptr(123);};
        // Qt show/geometry only on the offscreen fixture, never a desktop window.
        w.show();QApplication::processEvents();
        if(frame!=1 || round!=1 || !w.m_titleBar)return fail("actual shell show dispatch");
        auto* title=w.m_titleBar;const auto buttons=title->findChildren<QPushButton*>();
        for(auto* button:buttons)if(button->isVisible() && title->hitTestCaptionDrag(button->mapToGlobal(button->rect().center())))return fail("caption excludes title buttons");
        if(title->hitTestCaptionDrag(title->menuBar()->mapToGlobal(title->menuBar()->rect().center()))
            || title->hitTestCaptionDrag(w.m_alwaysOnTopCheck->mapToGlobal(w.m_alwaysOnTopCheck->rect().center())))return fail("caption excludes menu/topmost controls");
        bool drag=false;for(int x=0;x<title->width();x+=8)drag=drag || title->hitTestCaptionDrag(title->mapToGlobal(QPoint(x,title->height()/2)));
        if(!drag || !w.prepareForShutdown())return fail("caption positive control and shutdown");
    }
    std::cout<<"Window chrome: eight negative-coordinate edges, physical half-open bounds, fixed/maximized axes, failed/foreign reads, signed native-message routing, callback lifetime and actual offscreen shell/title controls OK (recorded frame/corner; no desktop movement/input)\n";
    std::cout<<"Window chrome DPI: native-screen to physical-client to fractional logical-client mapping,100/125/150/200%, negative monitor origin, border priority, failed conversion and handle replacement OK (recorded conversion; native drag unverified)\n";
    return true;
}
};
bool runWindowChromeChecks(){return WindowChromeChecks::run();}
