#include "app/MainWindow.h"
#include "app/ForegroundWindowMonitor.h"
#include "ui/ProfileListPresenter.h"
#include "ui/ProfileListWidget.h"
#include "core/capture/ScreenCapture.h"
#include <QApplication>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QMap>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <iostream>

namespace {
QIcon solid(Qt::GlobalColor color) {
    QPixmap image(18,18); image.fill(color); return QIcon(image);
}
QMap<QString,QByteArray> files(const QString& directory) {
    QMap<QString,QByteArray> values;
    QDirIterator entries(directory,QDir::Files,QDirIterator::Subdirectories);
    while(entries.hasNext()) {
        QFile file(entries.next());
        if(!file.open(QIODevice::ReadOnly)) return {};
        values.insert(file.fileName(),file.readAll());
    }
    return values;
}
bool presenterChecks() {
    ProfileListWidget list; QPushButton remove;
    list.resize(150,130);
    const auto fallback=solid(Qt::gray), expected=solid(Qt::green), wrong=solid(Qt::red);
    int enumerations=0, selectionSignals=0;
    QObject::connect(&list,&QListWidget::currentItemChanged,[&]{++selectionSignals;});
    using P=ProfileListPresenter;
    P::IconAccess access;
    access.processIcon=[](const QString&){return QIcon{};};
    access.windows=[&] {
        ++enumerations;
        return QVector<P::WindowIcon>{
            {"Shared game","C:/wrong/game.exe",wrong},
            {"Shared game","C:/owner/game.exe",expected},
            {"Unique tool","C:/tool/app.exe",expected}};
    };
    P presenter(list,&remove,access);
    QVector<P::Row> rows{
        {"default","cannot rename",{},{},{},true},
        {"known","긴 프로필 이름을 끝까지 툴팁으로 읽을 수 있어요","Shared",R"(C:\OWNER\sub\..\game.exe)",{},false},
        {"missing","Unavailable program","Shared","C:/missing/game.exe",{},false},
        {"ambiguous","Ambiguous title","Shared",{},{},false},
        {"unique","Unique program","Unique",{},{},false}};
    presenter.refresh(rows,"known",fallback);
    if(enumerations!=1 || selectionSignals || list.currentRow()!=1 || !remove.isEnabled()
        || list.item(0)->text()!=QStringLiteral("기본") || (list.item(0)->flags() & Qt::ItemIsDragEnabled)
        || list.item(1)->icon().cacheKey()!=expected.cacheKey()
        || list.item(2)->icon().cacheKey()!=fallback.cacheKey()
        || list.item(3)->icon().cacheKey()!=fallback.cacheKey()
        || list.item(4)->icon().cacheKey()!=expected.cacheKey()
        || !list.item(1)->toolTip().startsWith(rows[1].name+QLatin1Char('\n'))
        || !list.item(1)->toolTip().contains("game.exe")) {
        std::cerr<<"Profile list: icon/tooltip/default/signal invariant failed; enumerations="<<enumerations
            <<" signals="<<selectionSignals<<" selected="<<list.currentRow()<<'\n';return false;
    }
    presenter.refresh(rows,"default",fallback);
    if(remove.isEnabled() || selectionSignals){std::cerr<<"Profile list: default deletion guard failed\n";return false;}
    presenter.syncSelection("unique");
    if(list.currentRow()!=4 || selectionSignals)return false;
    presenter.syncSelection("deleted-profile");
    if(list.currentItem() || selectionSignals)return false;
    for(int i=0;i<30;++i)rows.push_back({QString::number(i),"scroll row",{},{},fallback,false});
    presenter.refresh(rows,"unique",fallback);
    list.doItemsLayout();
    list.verticalScrollBar()->setValue(list.verticalScrollBar()->maximum());
    const int scroll=list.verticalScrollBar()->value();
    presenter.refresh(rows,"unique",fallback);
    if(list.verticalScrollBar()->value()!=scroll || scroll==0 || selectionSignals) {
        std::cerr<<"Profile list: scroll retention failed before="<<scroll<<" after="<<list.verticalScrollBar()->value()<<'\n';return false;
    }
    // A fully cached list must not enumerate desktop windows at all.
    for(auto& row:rows)row.cachedIcon=fallback;
    const int before=enumerations;
    presenter.refresh(rows,"unique",fallback);
    return enumerations==before;
}
}

class ProfileListUiChecks {
public:
static bool run() {
    if(QApplication::platformName()!=QStringLiteral("offscreen") || !presenterChecks()) {
        std::cerr<<"Profile list presenter invariant FAILED\n";return false;
    }
    MainWindow window;
#ifdef _WIN32
    window.m_foregroundMonitor->stop();
#endif
    auto& profiles=*window.m_profileManager;
    const QString original=profiles.activeProfileId();
    const QString known=profiles.addProfile("profile known",false);
    const QString unknown=profiles.addProfile("profile without executable",false);
    if(known.isEmpty() || unknown.isEmpty()
        || !profiles.updateProfileTargetBinding(known,"Shared","C:/owner/game.exe",true)
        || !profiles.updateProfileTargetBinding(unknown,"Shared",{},true))return false;
    const auto expected=solid(Qt::green),wrong=solid(Qt::red);
    int reads=0,changes=0;
    ProfileListPresenter::IconAccess access;
    access.processIcon=[](const QString&){return QIcon{};};
    access.windows=[&]{++reads;return QVector<ProfileListPresenter::WindowIcon>{
        {"Shared game","C:/wrong/game.exe",wrong},
        {"Shared game","C:/owner/game.exe",expected}};};
    window.m_profileListPresenter=std::make_unique<ProfileListPresenter>(*window.m_profileList,window.m_deleteProfileButton,access);
    QObject::connect(window.m_profileList,&QListWidget::currentItemChanged,[&]{++changes;});
    const auto before=files(profiles.profilesDirectory());
    if(before.isEmpty())return false;
    ScreenCapture::setTargetWindow(nullptr);
    ScreenCapture::setTargetWindowTitle(L"profile-render-main-sentinel");
    ScreenCapture::setSubTargetWindowTitle(L"profile-render-sub-sentinel");
    window.refreshProfileList();
    const auto* selected=window.m_profileList->currentItem();
    const bool passive=reads==1 && !changes && !window.m_refreshingProfileList
        && selected && selected->data(Qt::UserRole).toString()==original
        && profiles.activeProfileId()==original
        && profiles.linkedTargetProcessPath(unknown).isEmpty()
        && profiles.linkedTargetProcessPath(known)=="C:/owner/game.exe"
        && files(profiles.profilesDirectory())==before
        && ScreenCapture::targetWindowTitle()==L"profile-render-main-sentinel"
        && ScreenCapture::subTargetWindowTitle()==L"profile-render-sub-sentinel"
        && ScreenCapture::targetWindow()==nullptr;
    if(!passive) {
        std::cerr<<"Profile list MainWindow passive refresh FAILED: reads="<<reads<<" signals="<<changes<<'\n';return false;
    }
    if(!profiles.removeProfile(known) || !profiles.removeProfile(unknown) || !window.prepareForShutdown())return false;
    std::cout<<"Profile list MainWindow: snapshot-only refresh preserves manifest/settings bytes, owner bindings, capture sentinels and selection; strict/ambiguous icons, rebuild signals, cached reads and scroll retention OK (offscreen; scripted desktop icons; no native input)\n";
    return true;
}
};
bool runProfileListUiChecks(){return ProfileListUiChecks::run();}
