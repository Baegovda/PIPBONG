#include "app/SessionInputCleanupController.h"
#include "app/FeatureRunSession.h"
#include "model/Feature.h"
#include "core/capture/ScreenCapture.h"
#include "core/input/InputSimulator.h"
#include <QDir>
#include <QString>

namespace {
std::optional<MouseButton> mouseButton(int key) {
    switch(key) {case 1:return MouseButton::Left;case 2:return MouseButton::Right;case 4:return MouseButton::Middle;
    case 5:return MouseButton::Back;case 6:return MouseButton::Forward;default:return std::nullopt;}
}
SessionInputCleanupController::Ports nativePorts() {
    using Controller=SessionInputCleanupController;Controller::Ports ports;
    if(!qEnvironmentVariableIsEmpty("PIPBONG_DATA_DIR") && (qEnvironmentVariableIntValue("PIPBONG_SELF_TEST_UI")==1
        || qEnvironmentVariableIntValue("PIPBONG_SMOKE_TEST")==1))return ports;
#ifdef _WIN32
    ports.find=[](const Controller::Binding& value)->std::optional<Controller::Window> {
        if(value.title.empty() && value.path.empty())return std::nullopt;
        const HWND hwnd=ScreenCapture::findVisibleWindowMatchingTitle(value.title,value.path);
        DWORD before=0,after=0;ScreenCapture::TargetWindowInfo info;
        if(!hwnd || !GetWindowThreadProcessId(hwnd,&before) || !ScreenCapture::queryWindowInfo(hwnd,info)
            || !GetWindowThreadProcessId(hwnd,&after) || before!=after)return std::nullopt;
        return Controller::Window{hwnd,before,info.title,info.processPath,info.visible};
    };
    ports.release=[](const auto& window,int key){InputSimulator::releaseHoldHotkeyToTarget(window.hwnd,key,window.processId,window.path);};
    ports.cleanup=[](int key){if(mouseButton(key))InputSimulator::forceHotkeyMouseButtonUp(key);else InputSimulator::forceKeyUp(key);};
    ports.cursor=[]()->std::optional<QPoint>{int x=0,y=0;if(InputSimulator::getCursorScreenPosition(x,y))return QPoint(x,y);return std::nullopt;};
    ports.move=[](QPoint point){InputSimulator::moveMouse(point.x(),point.y());};
#endif
    return ports;
}
}
SessionInputCleanupController::SessionInputCleanupController():SessionInputCleanupController(nativePorts()){}
void SessionInputCleanupController::bind(FeatureRunSession& session,const Feature& feature) {
    session.holdReleaseBindingCaptured=true;session.holdReleaseVirtualKey=feature.hotkey().virtualKey;
}
void SessionInputCleanupController::captureCursor(FeatureRunSession& session) {
    session.hasRunStartCursorPosition=false;
    if(!session.restoreMousePositionOnEnd || !m_ports.cursor)return;
    if(const auto point=m_ports.cursor()) {session.runStartCursorScreenX=point->x();session.runStartCursorScreenY=point->y();session.hasRunStartCursorPosition=true;}
}
void SessionInputCleanupController::restoreCursor(const FeatureRunSession& session) const {
    if(session.hasRunStartCursorPosition && m_ports.move)m_ports.move(QPoint(session.runStartCursorScreenX,session.runStartCursorScreenY));
}
SessionInputCleanupController::Binding SessionInputCleanupController::binding(const FeatureRunSession& session) {
    Binding value{session.lockedCaptureTargetTitle,session.lockedCaptureTargetProcessPath};
    if(session.sessionContext) {
        const auto prepared=session.sessionContext->targetWindowBinding();
        if(value.title.empty() && value.path.empty())return {prepared.first,prepared.second};
        if(value.path.empty() && value.title==prepared.first)value.path=prepared.second;
    }
    return value;
}
bool SessionInputCleanupController::injected(const FeatureRunSession& session,int key) {
#ifdef _WIN32
    if(const auto mouse=mouseButton(key))return session.sessionContext->pipbongEverInjectedMouseButton(*mouse);
    return session.sessionContext->pipbongEverInjectedVirtualKey(key);
#else
    return false;
#endif
}
bool SessionInputCleanupController::matches(const Window& window,const Binding& value) {
    if(!window.hwnd || !window.processId || !window.visible)return false;
    if(!value.path.empty()) {
        const auto clean=[](const std::wstring& path){return QDir::cleanPath(QDir::fromNativeSeparators(QString::fromStdWString(path)));};
        return clean(window.path).compare(clean(value.path),Qt::CaseInsensitive)==0;
    }
    return !value.title.empty() && QString::fromStdWString(window.title).contains(QString::fromStdWString(value.title),Qt::CaseInsensitive);
}
SessionInputCleanupController::Outcome SessionInputCleanupController::releaseHold(FeatureRunSession& session) {
    if(session.runningMode!=FeatureRunMode::Hold)return Outcome::Unavailable;
    if(session.holdHotkeyReleasedToTarget)return Outcome::Duplicate;
    if(!session.holdReleaseBindingCaptured || !session.sessionContext)return Outcome::Unavailable;
    const int key=session.holdReleaseVirtualKey;
    session.holdHotkeyReleasedToTarget=true; // Native/scripted callbacks cannot cause a second release.
    if(key<=0 || !injected(session,key))return Outcome::NotInjected;
    const auto value=binding(session);
    if(mouseButton(key) && m_ports.find && (!value.title.empty() || !value.path.empty())) {
        const auto target=m_ports.find(value);
        if(target && matches(*target,value) && m_ports.release) {m_ports.release(*target,key);return Outcome::TargetAttempted;}
    }
    if(!m_ports.cleanup)return Outcome::Unavailable;
    m_ports.cleanup(key);
    return Outcome::CleanupAttempted;
}
