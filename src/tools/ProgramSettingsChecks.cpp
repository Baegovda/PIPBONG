#include "app/ProgramSettingsController.h"
#include "app/ProgramSettings.h"
#include "app/ShutdownCoordinator.h"
#include "app/FeatureHotkeyGate.h"
#include "app/MainWindow.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include "ui/ProgramSettingsDialog.h"
#include "ui/LogPanelWidget.h"
#include <QApplication>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QCheckBox>
#include <QScopeGuard>
#include <QSettings>
#include <QFile>
#include <QSaveFile>
#include <QTimer>
#include <iostream>
#include <vector>

namespace {
using Controller=ProgramSettingsController;
struct Fixture {
    QWidget window;std::unique_ptr<Controller> controller;quint64 generation=1;
    bool allowed=true,accept=true,profile=true,disk=true,elevated=true,confirm=false,launch=true,guarded=false;
    int warnings=0,edits=0,quits=0,starts=0;std::vector<std::string> calls;
    std::function<void()> editAction,applyAction;
    Fixture(){
        Controller::Host h;h.allowed=[this]{return allowed;};h.source=[this]{return Controller::Source{this,"owner",generation};};
        h.saveProfile=[this]{calls.push_back("profile");return profile;};
        h.worker=[this]{calls.push_back("worker");if(applyAction)applyAction();};
        h.closePolicy=[this]{calls.push_back("close");};h.updatePolicy=[this]{calls.push_back("update");};h.logLimit=[this]{calls.push_back("log");};
        h.handoff=[this](const auto& start){calls.push_back("handoff");return start && start();};
        h.lastError=[]{return QStringLiteral("handoff failed");};h.quit=[this]{++quits;};
        Controller::Ports p;p.edit=[this](auto current){++edits;guarded=FeatureHotkeyGate::isFeatureHotkeysBlocked() && current();if(editAction)editAction();return accept;};
        p.sync=[this]{calls.push_back("disk");return disk;};p.elevated=[this]{return elevated;};p.confirmRestart=[this]{return confirm;};
        p.launchElevated=[this]{++starts;return launch;};p.warning=[this](const auto&){++warnings;};
        controller=std::make_unique<Controller>(window,std::move(h),std::move(p));
    }
};
bool handoffChecks() {
    for(int failure=0;failure<5;++failure){
        std::vector<int> calls;int launches=0,finalized=0;bool allow=false,nested=false;
        ShutdownCoordinator* active=nullptr;ShutdownCoordinator::HostCallbacks h;
        h.canBegin=[] {return true;};h.quiesce=[&]{calls.push_back(0);};
        h.saveProject=[&]{calls.push_back(1);return failure!=1 || allow;};
        h.saveSettings=[&]{calls.push_back(2);return failure!=2 || allow;};
        h.saveMemo=[&]{calls.push_back(3);return failure!=3 || allow;};h.finalize=[&]{calls.push_back(5);++finalized;};
        ShutdownCoordinator c(h);active=&c;
        const auto start=[&]{calls.push_back(4);++launches;nested=active->busy() && !active->prepare() && !active->handoff([]{return true;});return failure!=4 || allow;};
        const bool ok=c.handoff(start);
        if(failure==0){if(!ok || finalized!=1 || launches!=1 || !nested || !c.ready() || calls!=std::vector<int>{0,1,2,3,4,5})return false;
            if(c.handoff(start) || !c.prepare() || !c.checkpoint() || launches!=1)return false;
        } else {
            if(ok || c.ready() || c.busy() || finalized || c.lastError().isEmpty() || calls.back()!=failure || (failure<4 && launches))return false;
            calls.clear();allow=true;if(!c.handoff(start) || calls!=std::vector<int>{0,1,2,3,4,5} || finalized!=1 || !c.ready() || !nested)return false;
        }
    }
    for(int stage=0;stage<6;++stage){
        std::unique_ptr<ShutdownCoordinator> dying;ShutdownCoordinator::HostCallbacks h;
        h.canBegin=[&]{if(stage==0)dying.reset();return true;};h.quiesce=[&]{if(stage==1)dying.reset();};
        h.saveProject=[&]{if(stage==2)dying.reset();return true;};h.saveSettings=[&]{if(stage==3)dying.reset();return true;};
        h.saveMemo=[&]{if(stage==4)dying.reset();return true;};h.finalize=[&]{if(stage==5)dying.reset();};
        dying=std::make_unique<ShutdownCoordinator>(h);auto* raw=dying.get();
        if(raw->handoff([]{return true;}) || dying)return false;
    }
    std::unique_ptr<ShutdownCoordinator> dying;ShutdownCoordinator::HostCallbacks h;
    h.saveProject=h.saveSettings=h.saveMemo=[] {return true;};h.finalize=[]{};
    dying=std::make_unique<ShutdownCoordinator>(h);auto* raw=dying.get();
    if(raw->handoff([&]{dying.reset();return true;}) || dying)return false;
    return true;
}
}
class ProgramSettingsChecks {
public:
static bool run() {
    const auto fail=[](const char* s){std::cerr<<"Program settings FAILED: "<<s<<'\n';return false;};
    if(QApplication::platformName()!="offscreen")return fail("offscreen required");
    const QString key=QStringLiteral("program/runAsAdministrator"),logKey=QStringLiteral("program/logMaxLines");
    const QString selectKey=QStringLiteral("program/autoSelectRunningFeature");
    const auto old=QSettings().value(key),oldLog=QSettings().value(logKey),oldSelect=QSettings().value(selectKey);
    const auto restore=qScopeGuard([=]{QSettings settings;if(old.isValid())settings.setValue(key,old);else settings.remove(key);if(oldLog.isValid())settings.setValue(logKey,oldLog);else settings.remove(logKey);if(oldSelect.isValid())settings.setValue(selectKey,oldSelect);else settings.remove(selectKey);settings.sync();});
    QSettings().setValue(key,false);
    {
        Fixture f;f.allowed=false;f.controller->open();if(f.edits)return fail("blocked open");
        f.allowed=true;f.accept=false;f.controller->open();if(f.edits!=1 || !f.guarded || !f.calls.empty() || f.controller->busy())return fail("cancel and hotkey scope");
        f.accept=true;f.editAction=[&]{f.controller->open();};f.controller->open();
        if(f.edits!=2 || f.calls!=std::vector<std::string>{"profile","disk","worker","close","update","log"} || f.controller->busy())return fail("coherent apply order / reentry");
    }
    {
        Fixture f;f.editAction=[&]{++f.generation;};f.controller->open();
        if(!f.calls.empty() || f.warnings!=1)return fail("stale modal owner");
    }
    for(bool profileFailure:{false,true}){
        Fixture f;f.profile=!profileFailure;f.disk=profileFailure;f.controller->open();
        if(f.warnings!=1 || f.calls.size()!=6 || f.quits || f.starts)return fail("profile/global persistence failure still publishes cached values but cannot restart");
    }
    {
        Fixture f;f.applyAction=[&]{++f.generation;};f.controller->open();
        if(f.calls!=std::vector<std::string>{"profile","disk","worker"})return fail("owner replacement during application");
    }
    {
        Fixture f;f.applyAction=[&]{f.controller.reset();};auto* raw=f.controller.get();raw->open();
        if(f.controller || f.calls!=std::vector<std::string>{"profile","disk","worker"})return fail("deletion during runtime application");
    }
    {
        Fixture f;f.editAction=[&]{f.controller.reset();};auto* raw=f.controller.get();raw->open();
        if(f.controller || !f.calls.empty())return fail("deletion during modal");
    }
    QSettings().setValue(key,true); // Local fixture INI only; no registry helper.
    {
        Fixture f;f.elevated=false;f.controller->open();if(f.starts || f.quits)return fail("declined elevation remains usable");
        f.confirm=true;f.launch=false;f.controller->open();if(f.starts!=1 || f.quits || f.warnings!=1)return fail("cancelled launcher no quit");
        f.launch=true;f.controller->open();if(f.starts!=2 || f.quits!=1)return fail("successful handoff quits once");
    }
    if(!handoffChecks())return fail("save-once shutdown handoff and lifetime");
    // Exercise the real dialog's pre-commit guard. This rejection occurs before
    // ProgramSettings setters, startup registration or elevation helpers.
    {
        QWidget owner;Controller::Host h;quint64 generation=1;int warnings=0,saves=0;
        h.source=[&]{return Controller::Source{&owner,"one",generation};};h.saveProfile=[&]{++saves;return true;};
        Controller::Ports ports;ports.warning=[&](const auto&){++warnings;};
        Controller c(owner,h,ports);bool observed=false;
        QTimer::singleShot(0,&c,[&]{
            auto* dialog=qobject_cast<ProgramSettingsDialog*>(QApplication::activeModalWidget());if(!dialog)return;
            observed=true;++generation;dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        });
        c.open();if(!observed || saves || warnings!=1)return fail("actual modal stale-source commit guard");
    }
    if(!shell())return false;
    std::cout<<"Program settings: gated modal/owner commit, profile/global save failures, cached runtime publication, reentry/lifetime, real stale-dialog rejection, save-once handoff/failure/retry and actual MainWindow persistence/log/launch wiring OK (offscreen; recorded elevation/quit, no registry or process launch)\n";
    return true;
}
private:
static bool shell() {
    const auto fail=[](const char* s){std::cerr<<"Program settings shell FAILED: "<<s<<'\n';return false;};
    auto owner=std::make_unique<MainWindow>();auto& w=*owner;w.m_foregroundMonitor->stop();w.m_triggerMonitor->stop();
    const QString originalPath=w.m_profileManager->profileDirectory(w.m_profileManager->activeProfileId())+QStringLiteral("/profile-settings.json");
    QFile original(originalPath);if(!original.open(QIODevice::ReadOnly))return fail("original profile settings snapshot");
    const QByteArray bytes=original.readAll();original.close();bool pendingRestore=true;
    const auto restoreOriginal=[&]{
        if(!pendingRestore)return true;pendingRestore=false;owner.reset();
        QSaveFile file(originalPath);return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit();
    };
    const auto recover=qScopeGuard([&]{restoreOriginal();});
    auto& p=w.m_programSettings->m_ports;int starts=0,quits=0,warnings=0,limit=173;
    const bool selected=QSettings().value("program/autoSelectRunningFeature",true).toBool();
    p.edit=[&](auto current){if(!current())return false;QSettings settings;settings.setValue("program/logMaxLines",limit);settings.setValue("program/autoSelectRunningFeature",!selected);settings.setValue("program/runAsAdministrator",true);return true;};
    p.elevated=[] {return false;};p.confirmRestart=[] {return true;};p.launchElevated=[&]{++starts;return false;};p.warning=[&](const auto&){++warnings;};
    w.m_programSettings->m_host.quit=[&]{++quits;};
#ifdef Q_OS_WIN
    if(!w.saveActiveProfileSettings())return fail("fixture profile settings");
    {
        const QString path=w.m_profileManager->profileDirectory(w.m_profileManager->activeProfileId())+QStringLiteral("/profile-settings.json");
        HANDLE lock=CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(lock==INVALID_HANDLE_VALUE)return fail("actual profile-settings lock");
        const auto unlock=qScopeGuard([&]{CloseHandle(lock);});
        w.onProgramSettings();if(starts || quits || warnings!=1 || w.m_logPanel->maxLines()!=173)return fail("real profile-save failure cannot launch but applies cached values");
    }
    {
        QSettings settings;settings.sync();const auto path=settings.fileName();
        HANDLE lock=CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(lock==INVALID_HANDLE_VALUE)return fail("actual global INI lock");
        const auto unlock=qScopeGuard([&]{CloseHandle(lock);});limit=174;
        w.onProgramSettings();if(starts || quits || warnings!=2 || w.m_logPanel->maxLines()!=174)return fail("real global-save failure cannot launch but applies cached values");
    }
#endif
    warnings=0;
    w.onProgramSettings();
    if(starts!=1 || quits || warnings!=1 || w.m_shutdownCoordinator->ready() || w.m_shutdownCoordinator->busy() || w.m_logPanel->maxLines()!=limit || !w.m_profileManager->storageReady())return fail("failed actual handoff preserves usable controller and applies log limit");
    p.launchElevated=[&]{++starts;return true;};w.onProgramSettings();
    if(starts!=2 || quits!=1 || !w.m_shutdownCoordinator->ready() || !w.prepareForShutdown())return fail("actual save-once finalization/checkpoint");
    const auto before=starts;w.onProgramSettings();if(starts!=before)return fail("prepared shutdown blocks settings reopen");
    if(!restoreOriginal())return fail("restore original profile-settings bytes after shell destruction");
    return true;
}
};
bool runProgramSettingsChecks(){return ProgramSettingsChecks::run();}
