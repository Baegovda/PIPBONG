#include "ui/WindowChromeController.h"
#include "ui/CustomTitleBar.h"
#include "ui/UiResizeHandle.h"
#include <QMainWindow>
#include <cmath>
#include <limits>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

using Controller=WindowChromeController;
std::optional<QPointF> Controller::logicalClientPoint(QPoint physical,qreal ratio) {
    if(!std::isfinite(ratio) || ratio<=0)return {};
    const QPointF p=QPointF(physical)/ratio;
    const auto max=std::numeric_limits<int>::max();
    if(!std::isfinite(p.x()) || !std::isfinite(p.y()) || std::abs(p.x())>max || std::abs(p.y())>max)return {};
    return p;
}
bool Controller::contains(Bounds r,QPoint p) {
    return r.left<r.right && r.top<r.bottom && p.x()>=r.left && p.x()<r.right && p.y()>=r.top && p.y()<r.bottom;
}
std::optional<Controller::Hit> Controller::resizeHit(Bounds r,QPoint p,State s) {
    if(s.maximized || s.border<=0 || !contains(r,p))return {};
    const bool left=p.x()-r.left<s.border,right=r.right-p.x()<=s.border;
    const bool top=p.y()-r.top<s.border,bottom=r.bottom-p.y()<=s.border;
    if(s.width && s.height){
        if(top && left)return Hit::TopLeft;if(top && right)return Hit::TopRight;
        if(bottom && left)return Hit::BottomLeft;if(bottom && right)return Hit::BottomRight;
    }
    if(s.width){if(left)return Hit::Left;if(right)return Hit::Right;}
    if(s.height){if(top)return Hit::Top;if(bottom)return Hit::Bottom;}
    return {};
}
Controller::WindowChromeController(QMainWindow& window,CustomTitleBar* title,Ports ports,QObject* parent)
    :QObject(parent?parent:&window),m_window(&window),m_ports(std::move(ports)) {
    const QPointer<QMainWindow> owner(&window);const QPointer<CustomTitleBar> caption(title);
    if(!m_ports.handle)m_ports.handle=[owner]{return owner?quintptr(owner->internalWinId()):0;};
    if(!m_ports.ratio)m_ports.ratio=[owner]{return owner?owner->devicePixelRatioF():0;};
    if(!m_ports.caption)m_ports.caption=[caption](QPointF p){return caption && caption->hitTestCaptionAtWindowPoint(p);};
#ifdef Q_OS_WIN
    if(!m_ports.clientPoint)m_ports.clientPoint=[](Handle handle,QPoint p)->std::optional<QPoint>{
        POINT client{p.x(),p.y()};if(!handle || !ScreenToClient(reinterpret_cast<HWND>(handle),&client))return {};
        return QPoint(client.x,client.y);
    };
    if(!m_ports.rectangle)m_ports.rectangle=[](Handle handle)->std::optional<Bounds>{
        RECT r{};if(!handle || !GetWindowRect(reinterpret_cast<HWND>(handle),&r))return {};
        return Bounds{r.left,r.top,r.right,r.bottom};
    };
    if(!m_ports.frame)m_ports.frame=[](Handle handle){
        const auto hwnd=reinterpret_cast<HWND>(handle);if(!hwnd || !IsWindow(hwnd))return;
        const LONG style=GetWindowLongW(hwnd,GWL_STYLE);if(style & WS_THICKFRAME)return;
        SetWindowLongW(hwnd,GWL_STYLE,style|WS_THICKFRAME);
        SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    };
    if(!m_ports.round)m_ports.round=[](Handle handle){
        const auto hwnd=reinterpret_cast<HWND>(handle);if(!hwnd || !IsWindow(hwnd))return;
        const DWM_WINDOW_CORNER_PREFERENCE preference=DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd,DWMWA_WINDOW_CORNER_PREFERENCE,&preference,sizeof(preference));
    };
#endif
}
void Controller::shown() {
    const auto ports=m_ports;const QPointer<Controller> life(this);if(!m_window || !ports.handle)return;
    const auto handle=ports.handle();if(!life || !handle)return;
    if(ports.frame)ports.frame(handle);
    if(!life || !m_window || ports.handle()!=handle)return;
    if(ports.round)ports.round(handle);
}
bool Controller::nativeEvent(const QByteArray& type,void* message,qintptr* result) {
#ifdef Q_OS_WIN
    if(!result || !message || !m_window || (type!="windows_generic_MSG" && type!="windows_dispatch_MSG"))return false;
    const auto* msg=static_cast<const MSG*>(message);if(msg->message!=WM_NCHITTEST)return false;
    const auto ports=m_ports;const QPointer<Controller> life(this);if(!ports.handle || !ports.rectangle)return false;
    const auto handle=ports.handle();if(!life || !handle || handle!=reinterpret_cast<Handle>(msg->hwnd))return false;
    const auto bounds=ports.rectangle(handle);if(!life || !m_window || !bounds || ports.handle()!=handle)return false;
    const QPoint point(GET_X_LPARAM(msg->lParam),GET_Y_LPARAM(msg->lParam));
    if(!contains(*bounds,point))return false;
    const State state{m_window->isMaximized(),m_window->minimumWidth()<m_window->maximumWidth(),m_window->minimumHeight()<m_window->maximumHeight(),UiResizeHandle::kWindowResizeBorderPx};
    auto hit=resizeHit(*bounds,point,state);
    if(!hit && ports.caption && ports.clientPoint && ports.ratio){
        const auto client=ports.clientPoint(handle,point);
        if(!life || !m_window || ports.handle()!=handle || !client)return false;
        const auto ratio=ports.ratio();if(!life || !m_window || ports.handle()!=handle)return false;
        const auto logical=logicalClientPoint(*client,ratio);if(!logical)return false;
        const bool drag=ports.caption(*logical);
        if(!life || !m_window || ports.handle()!=handle)return false;
        if(drag)hit=Hit::Caption;
    }
    if(!hit)return false;
    switch(*hit){
    case Hit::Left:*result=HTLEFT;break;case Hit::Right:*result=HTRIGHT;break;
    case Hit::Top:*result=HTTOP;break;case Hit::Bottom:*result=HTBOTTOM;break;
    case Hit::TopLeft:*result=HTTOPLEFT;break;case Hit::TopRight:*result=HTTOPRIGHT;break;
    case Hit::BottomLeft:*result=HTBOTTOMLEFT;break;case Hit::BottomRight:*result=HTBOTTOMRIGHT;break;
    case Hit::Caption:*result=HTCAPTION;break;
    }
    return true;
#else
    Q_UNUSED(type);Q_UNUSED(message);Q_UNUSED(result);return false;
#endif
}
