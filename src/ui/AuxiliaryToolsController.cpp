#include "ui/AuxiliaryToolsController.h"
#include "app/ProfileManager.h"
#include "ui/calculator/CalculatorDialog.h"
#include "ui/diagnostics/SpikeWatchDialog.h"
#include "ui/memo/MemoDialog.h"
#include <QEvent>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>

AuxiliaryToolsController::AuxiliaryToolsController(ProfileManager& profiles,QWidget& owner,Host host)
    :QObject(&owner),m_profiles(profiles),m_owner(&owner),m_host(std::move(host)){}

AuxiliaryToolsController::~AuxiliaryToolsController(){
    // Stop worker-backed dialogs while the host's run registry is still alive.
    for(const auto& dialog:m_owned)if(dialog){dialog->removeEventFilter(this);disconnect(dialog,nullptr,this,nullptr);}
    const auto owned=m_owned;
    for(const auto& dialog:owned)delete dialog.data();
}

void AuxiliaryToolsController::setButtons(QPushButton* calculator,QPushButton* spike,QPushButton* memo){
    m_calculatorButton=calculator;m_spikeButton=spike;m_memoButton=memo;updateButtons();
}
bool AuxiliaryToolsController::calculatorVisible()const{return m_calculator && m_calculator->isVisible();}
bool AuxiliaryToolsController::spikeWatchVisible()const{return m_spike && m_spike->isVisible();}
bool AuxiliaryToolsController::memoVisible()const{return m_memo && m_memo->isVisible();}
MemoDialog* AuxiliaryToolsController::memo()const{return m_memo;}
bool AuxiliaryToolsController::saveMemo(){return !m_memo || m_memo->saveNow();}

void AuxiliaryToolsController::updateButtons(){
    const auto set=[](QPushButton* button,bool visible){if(button){QSignalBlocker guard(button);button->setChecked(visible);}};
    set(m_calculatorButton,calculatorVisible());set(m_spikeButton,spikeWatchVisible());set(m_memoButton,memoVisible());
}
bool AuxiliaryToolsController::eventFilter(QObject* watched,QEvent* event){
    if(event->type()==QEvent::Show || event->type()==QEvent::Hide)updateButtons();
    return QObject::eventFilter(watched,event);
}
void AuxiliaryToolsController::wire(QDialog* dialog,Kind kind){
    dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->installEventFilter(this);m_owned.append(dialog);
    const auto retire=[this,dialog,kind]{
        // An accepted close can queue deletion. A synchronous reopen must create
        // a fresh dialog, and the old destroyed callback must not clear it.
        if(kind==Kind::Calculator && m_calculator==dialog)m_calculator=nullptr;
        if(kind==Kind::Spike && m_spike==dialog)m_spike=nullptr;
        if(kind==Kind::Memo && m_memo==dialog)m_memo=nullptr;
        updateButtons();
    };
    connect(dialog,&QDialog::finished,this,retire);
    connect(dialog,&QObject::destroyed,this,[this,retire]{
        retire();
        for(auto it=m_owned.begin();it!=m_owned.end();)if(!*it)it=m_owned.erase(it);else ++it;
    });
}
void AuxiliaryToolsController::present(QDialog* dialog){
    dialog->show();dialog->raise();dialog->activateWindow();
}
void AuxiliaryToolsController::toggleCalculator(){
    if(m_calculator && m_calculator->isVisible()){m_calculator->close();updateButtons();return;}
    if(!m_owner)return;
    if(!m_calculator){m_calculator=new CalculatorDialog(m_owner);wire(m_calculator,Kind::Calculator);}
    present(m_calculator);updateButtons();
}
void AuxiliaryToolsController::toggleSpikeWatch(){
    if(m_spike && m_spike->isVisible()){m_spike->close();updateButtons();return;}
    if(!m_owner)return;
    if(!m_spike){m_spike=new SpikeWatchDialog(m_owner);wire(m_spike,Kind::Spike);}
    QPointer<AuxiliaryToolsController> self(this);
    m_spike->setFeatureRunningCallback([self]{return self && self->m_host.featureRunning && self->m_host.featureRunning();});
    present(m_spike);m_spike->startMonitoringIfIdle();updateButtons();
}
MemoDialog* AuxiliaryToolsController::ensureMemo(){
    if(!m_memo && m_owner){m_memo=new MemoDialog(m_owner);wire(m_memo,Kind::Memo);}
    return m_memo;
}
void AuxiliaryToolsController::toggleMemo(){
    if(m_memo && m_memo->isVisible()){m_memo->close();updateButtons();return;}
    if(!ensureMemo() || !syncMemoProfile())return;
    present(m_memo);updateButtons();
}
bool AuxiliaryToolsController::syncMemoProfile(){
    const QString id=m_profiles.activeProfileId();
    const bool open=!id.isEmpty() && QSettings().value(QStringLiteral("memo/open/%1").arg(id),false).toBool();
    if(!open && !m_memo)return true;
    if(!ensureMemo())return false;
    const auto* profile=m_profiles.activeProfile();
    const QString name=profile?(m_profiles.isDefaultProfile(id)?tr("기본"):profile->name):QString();
    if(!m_memo->setProfile(id,m_profiles.activeProjectDirectory(),name)){
        const auto warning=m_host.warning;
        if(warning)warning(tr("메모 저장 또는 읽기에 실패해 이전 프로필의 메모를 유지해요."));
        return false;
    }
    if(open && !m_memo->isVisible())m_memo->show();
    else if(!open && m_memo->isVisible())m_memo->hide();
    updateButtons();return true;
}
void AuxiliaryToolsController::hideForTray(){
    if(m_calculator)m_calculator->hide();if(m_spike)m_spike->hide();updateButtons();
}
void AuxiliaryToolsController::closeForShutdown(){
    if(m_calculator)m_calculator->close();if(m_spike)m_spike->close();
    if(m_memo){m_memo->prepareForApplicationShutdown();m_memo->close();}updateButtons();
}
