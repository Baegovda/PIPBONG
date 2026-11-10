#include "app/ProgramSettingsController.h"
#include "app/FeatureHotkeyGate.h"
#include "app/ProgramSettings.h"
#include "app/WindowsRunAsAdmin.h"
#include "ui/ProgramSettingsDialog.h"
#include <QMessageBox>
#include <QPointer>
#include <QScopeGuard>
#include <QSettings>

using Controller=ProgramSettingsController;
Controller::ProgramSettingsController(QWidget& window,Host host,Ports ports,QObject* parent)
    :QObject(parent?parent:&window),m_host(std::move(host)),m_ports(std::move(ports)) {
    const QPointer<QWidget> owner(&window);
    if(!m_ports.edit)m_ports.edit=[owner](Current current){
        if(!owner || !current())return false;
        QPointer<ProgramSettingsDialog> dialog=new ProgramSettingsDialog(owner);
        dialog->setCommitAllowed(std::move(current));const int result=dialog->exec();
        if(!dialog)return false;delete dialog.data();return result==QDialog::Accepted;
    };
    if(!m_ports.sync)m_ports.sync=[]{QSettings settings;settings.sync();return settings.status()==QSettings::NoError;};
    if(!m_ports.elevated)m_ports.elevated=WindowsRunAsAdmin::isProcessElevated;
    if(!m_ports.launchElevated)m_ports.launchElevated=WindowsRunAsAdmin::relaunchElevated;
    if(!m_ports.confirmRestart)m_ports.confirmRestart=[owner]{return owner && QMessageBox::question(owner,tr("관리자 권한"),tr("관리자 권한을 적용하려면 다시 시작해야 해요.\n지금 다시 시작할까요?"),QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes)==QMessageBox::Yes;};
    if(!m_ports.warning)m_ports.warning=[owner](const QString& text){if(owner)QMessageBox::warning(owner,tr("프로그램 설정"),text);};
}
void Controller::open() {
    if(m_busy)return;
    const auto host=m_host;const auto ports=m_ports;const QPointer<Controller> life(this);
    if(host.allowed && !host.allowed())return;
    m_busy=true;const auto reset=qScopeGuard([life]{if(life)life->m_busy=false;});
    FeatureHotkeyGateScope hotkeys;
    const auto source=host.source?host.source():Source{};
    const Current current=[life,host,source]{
        if(!life)return false;const auto value=host.source?host.source():Source{};
        return life && source.project==value.project && source.owner==value.owner && source.generation==value.generation;
    };
    const bool accepted=ports.edit && ports.edit(current);if(!life)return;
    if(!current()){if(ports.warning)ports.warning(tr("프로필이 바뀌었어요. 설정 창을 다시 열어 현재 설정을 확인해 주세요."));return;}
    if(!accepted)return;
    const bool profileSaved=host.saveProfile && host.saveProfile();if(!current())return;
    const bool settingsSaved=ports.sync && ports.sync();if(!current())return;
    // The dialog has applied cached values. Publish those values even if disk
    // persistence failed, report that failure and prohibit a restart.
    for(const auto& apply:{host.worker,host.closePolicy,host.updatePolicy,host.logLimit}){
        if(apply)apply();if(!current())return;
    }
    if(!profileSaved || !settingsSaved){if(ports.warning)ports.warning(tr("현재 적용된 설정을 저장하지 못했어요. 저장 위치와 여유 공간을 확인한 뒤 다시 시도해 주세요."));return;}
    if(!ProgramSettings::runAsAdministrator() || !ports.elevated || ports.elevated())return;
    if(!current() || !ports.confirmRestart || !ports.confirmRestart() || !current())return;
    // Save exactly once under the shutdown write gate before spawning. Once
    // launch succeeds there is no second fallible persistence checkpoint.
    const bool launched=host.handoff && host.handoff(ports.launchElevated);if(!life)return;
    if(!launched){const auto error=host.lastError?host.lastError():QString();if(life && ports.warning)ports.warning(error.isEmpty()?tr("관리자 권한으로 다시 시작하지 못했어요."):error);return;}
    if(host.quit)host.quit();
}
