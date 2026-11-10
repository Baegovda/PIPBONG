#include "ui/WindowShellController.h"
#include "app/MainWindow.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include "app/UpdateUiController.h"
#include "app/ShutdownCoordinator.h"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QMainWindow>
#include <QMessageBox>
#include <QSettings>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QTimer>
#include <iostream>

namespace {
class ShellWindow:public QMainWindow {
public:WindowShellController* shell=nullptr;
protected:void closeEvent(QCloseEvent* event)override{if(shell && shell->handleClose())event->accept();else event->ignore();}
};
}
class WindowShellUiChecks {
public:
static int exitProbe(){
    if(QApplication::platformName()!="offscreen")return 198;
    ProgramSettings::setCloseToTray(true);ShellWindow window;int checkpoints=0;bool timedOut=false;
    WindowShellController::Host host;host.checkpoint=[&]{++checkpoints;return true;};
    WindowShellController::Ports ports;ports.trayAvailable=[] {return true;};ports.trayVisible=[](bool){};ports.notifyTray=[]{};ports.topmost=[](bool){};ports.present=[]{};
    WindowShellController shell(window,nullptr,nullptr,host,ports);window.shell=&shell;shell.applyClosePolicy();window.show();
    QTimer::singleShot(0,&shell,&WindowShellController::requestQuit);
    QTimer::singleShot(2000,&window,[&]{timedOut=true;QApplication::exit(197);});
    const int result=QApplication::exec();
    if(result || timedOut || checkpoints!=1 || !shell.m_exitCommitted || window.isVisible())return 197;
    std::cout<<"Window shell exit probe: actual event loop quit after one checkpoint OK\n";return 0;
}
static bool run(){
    if(QApplication::platformName()!="offscreen")return false;
    const auto fail=[](const char* label){std::cerr<<"Window shell FAILED: "<<label<<'\n';return false;};
    const bool previousClose=ProgramSettings::closeToTray(),previousQuit=QApplication::quitOnLastWindowClosed();
    const QString key="ui/state/mainWindow/alwaysOnTop";QSettings settings;const auto previousTop=settings.value(key);
    struct Restore{bool close,quit;QString key;QVariant top;~Restore(){ProgramSettings::setCloseToTray(close);QApplication::setQuitOnLastWindowClosed(quit);QSettings s;if(top.isValid())s.setValue(key,top);else s.remove(key);}}restore{previousClose,previousQuit,key,previousTop};
    ProgramSettings::setCloseToTray(true);
    bool available=false,tray=false,saveOK=false,nested=false,nestedRejected=false;
    int notices=0,presents=0,saves=0,hiddenTools=0,checkpoints=0,warnings=0,topChanges=0;bool lastTop=false;
    ShellWindow window;QCheckBox top;
    WindowShellController::Ports ports;
    ports.trayAvailable=[&]{return available;};ports.trayVisible=[&](bool value){tray=value;};ports.notifyTray=[&]{++notices;};
    ports.present=[&]{++presents;window.show();};ports.topmost=[&](bool value){++topChanges;lastTop=value;};
    WindowShellController::Host host;host.hideAuxiliary=[&]{++hiddenTools;};host.saveUiState=[&]{++saves;};
    // QWidget can short-circuit recursive close() before another closeEvent;
    // call the controller entry to exercise its independent reentry guard.
    host.checkpoint=[&]{++checkpoints;if(nested)nestedRejected=!window.shell->handleClose();return saveOK;};host.lastError=[] {return QStringLiteral("locked");};host.warning=[&](const QString&){++warnings;};
    WindowShellController shell(window,nullptr,&top,host,ports);window.shell=&shell;
    shell.applyClosePolicy();window.show();
    if(tray || !QApplication::quitOnLastWindowClosed() || window.close() || !window.isVisible() || checkpoints!=1 || warnings!=1)return fail("unavailable tray cannot hide blocked close");
    available=true;shell.applyClosePolicy();
    if(!tray || QApplication::quitOnLastWindowClosed())return fail("available tray policy");
    shell.requestQuit();
    if(!window.isVisible() || checkpoints!=2 || warnings!=2 || shell.m_explicitQuit || shell.m_exitCommitted || !tray)return fail("failed explicit exit retains restore surface and clears intent");
    if(window.close() || window.isVisible() || checkpoints!=2 || saves!=1 || hiddenTools!=1 || notices!=1)return fail("X after failed exit returns to tray");
    shell.restoreFromTray();if(!window.isVisible() || presents!=1)return fail("restore port");
    window.close();shell.restoreFromTray();if(notices!=1 || saves!=2 || hiddenTools!=2)return fail("single tray notice");
    available=false;shell.applyClosePolicy();
    if(tray || !QApplication::quitOnLastWindowClosed() || window.close() || !window.isVisible() || checkpoints!=3)return fail("tray loss falls back to checkpoint");
    int toggles=0;QObject::connect(&top,&QCheckBox::toggled,[&]{++toggles;});
    settings.setValue(key,true);shell.restoreTopmost();
    if(!top.isChecked() || !lastTop || toggles || topChanges!=1)return fail("topmost restore blocks user signal");
    QObject::connect(&top,&QCheckBox::toggled,&shell,&WindowShellController::toggleTopmost);
    top.setChecked(false);shell.shown();
    if(lastTop || settings.value(key).toBool() || topChanges!=3 || toggles!=1)return fail("topmost persistence and shown reapply");
    shell.setDocument("C:/fixture/demo.json",true);shell.persistent("  running  ");shell.transient("temporary",10);
    if(!window.windowTitle().endsWith("demo.json * - temporary"))return fail("transient title precedence");
    QElapsedTimer elapsed;elapsed.start();while(!shell.transient().isEmpty() && elapsed.elapsed()<500){QApplication::processEvents();QThread::msleep(1);}
    if(!shell.transient().isEmpty() || !window.windowTitle().endsWith("demo.json * - running"))return fail("real timer restores persistent status");
    shell.transient("indefinite",0);shell.persistent("new persistent");shell.setDocument({},false);
    if(!window.windowTitle().endsWith(QStringLiteral("제목 없음 - indefinite")) || shell.m_statusTimer->isActive())return fail("title update preserves indefinite status");
    shell.clearTransient();if(!window.windowTitle().endsWith(QStringLiteral("제목 없음 - new persistent")))return fail("clear transient reveals current persistent");
    available=true;shell.applyClosePolicy();saveOK=true;nested=true;shell.requestQuit();
    if(window.isVisible() || tray || !QApplication::quitOnLastWindowClosed() || !nestedRejected || checkpoints!=4 || !shell.m_exitCommitted)return fail("accepted explicit exit commits quit policy after one checkpoint");
    shell.restoreFromTray();if(presents!=2)return fail("committed exit cannot reopen");
    {
        QMainWindow owner;auto* disposable=new WindowShellController(owner,nullptr,nullptr,{},ports);QPointer<WindowShellController> life(disposable);
        QObject::connect(&owner,&QWidget::windowTitleChanged,&owner,[&]{delete disposable;disposable=nullptr;});
        disposable->transient("delete during title publication",10);if(life)return fail("title callback lifetime");
    }
    // Real shell checkpoint: locked canonical project must keep the native
    // tray/UI policy intact; ports record all tray/topmost/presentation work.
    {
        MainWindow main;main.m_foregroundMonitor->stop();main.m_triggerMonitor->stop();
        bool nativeTray=false;int mainWarnings=0;
        auto* controller=main.m_windowShell.get();
        controller->m_ports.trayAvailable=[] {return true;};controller->m_ports.trayVisible=[&](bool value){nativeTray=value;};
        controller->m_ports.topmost=[](bool){};controller->m_ports.notifyTray=[]{};
        controller->m_ports.present=[&]{main.show();};controller->m_host.warning=[&](const QString&){++mainWarnings;};
        controller->applyClosePolicy();main.show();main.m_updateController->stop();
        const QString path=main.m_profileManager->activeProjectPath();QFile original(path);if(!original.open(QIODevice::ReadOnly))return fail("canonical project read");const auto before=original.readAll();original.close();
        HANDLE lock=CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(lock==INVALID_HANDLE_VALUE)return fail("canonical lock");main.m_modified=true;
        main.requestApplicationQuit();
        bool updateWarning=false;QTimer acknowledge;acknowledge.setInterval(1);
        QObject::connect(&acknowledge,&QTimer::timeout,&main,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* message=qobject_cast<QMessageBox*>(widget)){updateWarning=true;message->accept();acknowledge.stop();}});
        // prepareForShutdown processes events before opening the warning, so a
        // one-shot timer could be consumed before the modal dialog exists.
        acknowledge.start();
        main.onReadyToRestartForUpdate();
        acknowledge.stop();
        const bool retained=main.isVisible() && nativeTray && mainWarnings==1 && updateWarning && !controller->m_exitCommitted && !controller->m_explicitQuit && !QApplication::quitOnLastWindowClosed();
        const bool readback=original.open(QIODevice::ReadOnly);const bool preserved=readback && original.readAll()==before;original.close();
        const bool rendered=main.grab().save(QDir(qEnvironmentVariable("PIPBONG_DATA_DIR")).filePath("window-shell-detail.png"));
        const bool trayClose=!main.close() && !main.isVisible();controller->restoreFromTray();
        CloseHandle(lock);
        if(!retained || !preserved || !trayClose || !main.isVisible() || !rendered)return fail("real locked explicit/update exit preserves bytes and tray restore");
        main.requestApplicationQuit();
        if(main.isVisible() || nativeTray || !main.m_shutdownCoordinator->ready() || !QApplication::quitOnLastWindowClosed())return fail("real unlocked exit checkpoint commits");
    }
    QTemporaryDir probeData;QProcess child;auto environment=QProcessEnvironment::systemEnvironment();
    environment.insert("QT_QPA_PLATFORM","offscreen");environment.insert("PIPBONG_SELF_TEST_UI","1");environment.insert("PIPBONG_SELF_TEST_SHELL_EXIT","1");
    environment.insert("PIPBONG_DATA_DIR",probeData.path());environment.insert("PIPBONG_REPO_ROOT",probeData.path());
    child.setProcessEnvironment(environment);child.start(QCoreApplication::applicationFilePath(),QStringList{});
    if(!child.waitForStarted(2000))return fail("exit probe start");const qint64 pid=child.processId();
    if(!child.waitForFinished(5000)){child.kill();child.waitForFinished(2000);return fail("exit probe exceeded deadline");}
    const bool marker=child.readAllStandardOutput().contains("actual event loop quit after one checkpoint OK");
    QFile report(QDir(qEnvironmentVariable("PIPBONG_DATA_DIR")).filePath("window-shell-exit-probe.json"));
    if(!report.open(QIODevice::WriteOnly))return fail("exit probe report");
    report.write(QJsonDocument(QJsonObject{{"PID",pid},{"ExitCode",child.exitCode()},{"NormalExit",child.exitStatus()==QProcess::NormalExit},{"ActualEventLoopMarker",marker}}).toJson());report.close();
    if(child.exitStatus()!=QProcess::NormalExit || child.exitCode()!=0 || !marker)return fail("actual offscreen event loop did not exit cleanly");
    std::cout<<"Window shell exit subprocess: actual QApplication event loop ends on explicit close-to-tray exit, no fallback timer/kill OK (offscreen; recorded tray; no native input)\n";
    std::cout<<"Window shell: unavailable/lost tray, declined explicit/update exit, X retry/restore, one notice, nested checkpoint, successful quit policy, topmost restore/signals, actual title timer/lifetime and MainWindow locked project bytes/unlocked retry OK (offscreen; recorded tray/topmost/presentation; no native activation/input)\n";
    return true;
}
};
bool runWindowShellChecks(){return WindowShellUiChecks::run();}
int runWindowShellExitProbe(){return WindowShellUiChecks::exitProbe();}
