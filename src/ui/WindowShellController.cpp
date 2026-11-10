#include "ui/WindowShellController.h"
#include "ui/CustomTitleBar.h"
#include "app/ProgramSettings.h"
#include <QApplication>
#include <QCheckBox>
#include <QFileInfo>
#include <QMainWindow>
#include <QMenu>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalBlocker>
#include <QSystemTrayIcon>
#include <QTimer>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace {
const QString topmostKey=QStringLiteral("ui/state/mainWindow/alwaysOnTop");
void nativeTopmost(QMainWindow* window,bool enabled){
    if(!window)return;
#ifdef Q_OS_WIN
    if(!window->internalWinId())return;
    const HWND hwnd=reinterpret_cast<HWND>(window->internalWinId());
    if(hwnd && IsWindow(hwnd))SetWindowPos(hwnd,enabled?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
#else
    const bool visible=window->isVisible();window->setWindowFlag(Qt::WindowStaysOnTopHint,enabled);if(visible)window->show();
#endif
}
}
WindowShellController::WindowShellController(QMainWindow& window,CustomTitleBar* title,QCheckBox* topmost,Host host)
    :WindowShellController(window,title,topmost,std::move(host),{}){}
WindowShellController::WindowShellController(QMainWindow& window,CustomTitleBar* title,QCheckBox* topmost,Host host,Ports ports)
    :QObject(&window),m_window(&window),m_titleBar(title),m_topmost(topmost),m_statusTimer(new QTimer(this)),m_host(std::move(host)),m_ports(std::move(ports)),
      m_baseTitle(window.windowTitle()),m_previousQuitPolicy(QApplication::quitOnLastWindowClosed()){
    if(!m_ports.trayAvailable)m_ports.trayAvailable=[] {return QSystemTrayIcon::isSystemTrayAvailable();};
    if(!m_ports.trayVisible)m_ports.trayVisible=[this](bool visible){if(visible)ensureNativeTray();if(m_tray)m_tray->setVisible(visible);};
    if(!m_ports.notifyTray)m_ports.notifyTray=[this]{if(m_tray)m_tray->showMessage(tr("PIPBONG"),tr("알림 영역에서 계속 실행 중입니다."),QSystemTrayIcon::Information,3000);};
    const QPointer<QMainWindow> owner(&window);
    if(!m_ports.topmost)m_ports.topmost=[owner](bool enabled){nativeTopmost(owner,enabled);};
    if(!m_ports.present)m_ports.present=[owner]{if(owner){owner->showNormal();owner->raise();owner->activateWindow();}};
    m_statusTimer->setSingleShot(true);connect(m_statusTimer,&QTimer::timeout,this,&WindowShellController::clearTransient);
}
WindowShellController::~WindowShellController(){
    delete m_tray.data();delete m_menu.data();QApplication::setQuitOnLastWindowClosed(m_previousQuitPolicy);
}
void WindowShellController::ensureNativeTray(){
    if(m_tray || !m_window || !m_ports.trayAvailable())return;
    m_tray=new QSystemTrayIcon(m_window->windowIcon(),this);m_tray->setToolTip(tr("PIPBONG"));
    m_menu=new QMenu(m_window);
    m_menu->addAction(tr("열기"),this,&WindowShellController::restoreFromTray);m_menu->addSeparator();
    m_menu->addAction(tr("종료"),this,&WindowShellController::requestQuit);m_tray->setContextMenu(m_menu);
    connect(m_tray,&QSystemTrayIcon::activated,this,[this](QSystemTrayIcon::ActivationReason reason){
        if(reason==QSystemTrayIcon::Trigger || reason==QSystemTrayIcon::DoubleClick)restoreFromTray();
    });
}
void WindowShellController::applyClosePolicy(){
    const bool tray=!m_exitCommitted && ProgramSettings::closeToTray() && m_ports.trayAvailable();
    QApplication::setQuitOnLastWindowClosed(!tray);m_ports.trayVisible(tray);
}
bool WindowShellController::handleClose(){
    if(m_closing)return false;
    m_closing=true;const QPointer<WindowShellController> life(this);
    const auto release=qScopeGuard([life]{if(life)life->m_closing=false;});
    const auto host=m_host;const auto ports=m_ports;
    if(!m_exitCommitted && !m_explicitQuit && ProgramSettings::closeToTray() && ports.trayAvailable()){
        // Ensure the restore surface exists before hiding the main window.
        ports.trayVisible(true);if(!life)return false;
        QApplication::setQuitOnLastWindowClosed(false);
        if(host.hideAuxiliary)host.hideAuxiliary();if(!life)return false;
        if(host.saveUiState)host.saveUiState();if(!life)return false;
        if(m_window)m_window->hide();if(!life)return false;
        if(!m_notified){m_notified=true;ports.notifyTray();}
        return false;
    }
    if(!host.checkpoint || !host.checkpoint()){
        if(!life)return false;
        const QString error=host.lastError?host.lastError():QString();
        if(life && !error.isEmpty() && host.warning)host.warning(error+tr("\n현재 내용은 화면에 유지돼요. 저장 위치와 여유 공간을 확인한 뒤 다시 시도해 주세요."));
        return false;
    }
    if(!life)return false;
    commitExit();return true;
}
void WindowShellController::requestQuit(){
    if(m_explicitQuit || !m_window)return;
    const QPointer<WindowShellController> life(this);m_explicitQuit=true;
    m_window->close();
    if(life)m_explicitQuit=false; // A failed checkpoint must not poison later X-close policy.
}
void WindowShellController::restoreFromTray(){const auto present=m_ports.present;if(!m_exitCommitted)present();}
void WindowShellController::commitExit(){
    m_exitCommitted=true;m_statusTimer->stop();QApplication::setQuitOnLastWindowClosed(true);m_ports.trayVisible(false);
}
void WindowShellController::restoreTopmost(){
    const bool enabled=QSettings().value(topmostKey,false).toBool();
    if(m_topmost){QSignalBlocker guard(m_topmost);m_topmost->setChecked(enabled);}m_ports.topmost(enabled);
}
void WindowShellController::toggleTopmost(bool enabled){m_ports.topmost(enabled);QSettings().setValue(topmostKey,enabled);}
void WindowShellController::shown(){if(m_topmost)m_ports.topmost(m_topmost->isChecked());}
void WindowShellController::setDocument(const QString& path,bool modified){
    m_baseTitle=QStringLiteral("PIPBONG %1 - %2%3").arg(QCoreApplication::applicationVersion(),path.isEmpty()?tr("제목 없음"):QFileInfo(path).fileName(),modified?QStringLiteral(" *"):QString());refreshTitle();
}
void WindowShellController::refreshTitle(){
    const QString status=m_transient.isEmpty()?m_persistent:m_transient;
    const QPointer<WindowShellController> life(this);
    if(m_window)m_window->setWindowTitle(m_baseTitle+(status.isEmpty()?QString():QStringLiteral(" - ")+status));
    if(life && m_titleBar)m_titleBar->syncFromWindowTitle();
}
void WindowShellController::transient(const QString& message,int timeoutMs){
    const QPointer<WindowShellController> life(this);
    m_statusTimer->stop();m_transient=message;refreshTitle();if(life && !message.isEmpty() && timeoutMs>0)m_statusTimer->start(timeoutMs);
}
void WindowShellController::persistent(const QString& message){const auto text=message.trimmed();if(m_persistent==text)return;m_persistent=text;refreshTitle();}
void WindowShellController::clearTransient(){m_statusTimer->stop();m_transient.clear();refreshTitle();}
