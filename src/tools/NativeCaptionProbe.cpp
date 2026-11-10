#include "tools/NativeCaptionProbe.h"
#include "app/MainWindow.h"
#include "ui/WindowChromeController.h"
#include "ui/CustomTitleBar.h"
#include <QApplication>
#include <QMenuBar>
#include <QPushButton>
#include <QCheckBox>
#include <QJsonArray>
#include <climits>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
QJsonObject NativeCaptionProbe::inspect(MainWindow& window) {
    QJsonObject report{{"supported",false},{"method","direct controller evaluation; no sent message, input, focus, movement or resizing"}};
#ifdef Q_OS_WIN
    if(QApplication::platformName()!="windows" || !window.m_windowChrome || !window.m_titleBar)return report;
    const auto hwnd=reinterpret_cast<HWND>(window.internalWinId());RECT bounds{};
    if(!hwnd || !GetWindowRect(hwnd,&bounds))return report;
    const qreal ratio=window.devicePixelRatioF();auto* title=window.m_titleBar;
    std::vector<std::pair<QString,QPointF>> points;
    auto add=[&](QWidget* widget,const QString& name){if(widget && widget->isVisible())points.push_back({name,widget->mapTo(&window,QPointF(widget->rect().center()))});};
    add(title->findChild<QWidget*>("titleBarDragRegion"),"drag-center");add(title->menuBar(),"menu");add(window.m_alwaysOnTopCheck,"topmost");
    for(auto* button:title->findChildren<QPushButton*>())add(button,button->objectName());
    for(int x=16;x<title->width()-16;x+=48)points.push_back({QStringLiteral("row-%1").arg(x),title->mapTo(&window,QPointF(x,title->height()/2.0))});
    QJsonArray samples;int mismatches=0,oldMismatches=0,positives=0,negatives=0;
    for(const auto& row:points){
        POINT global{qRound(row.second.x()*ratio),qRound(row.second.y()*ratio)};
        const QPoint physicalClient(global.x,global.y);
        if(!ClientToScreen(hwnd,&global) || global.x<SHRT_MIN || global.x>SHRT_MAX || global.y<SHRT_MIN || global.y>SHRT_MAX)continue;
        if(global.x<bounds.left || global.x>=bounds.right || global.y<bounds.top || global.y>=bounds.bottom)continue;
        const bool expected=title->hitTestCaptionAtWindowPoint(QPointF(physicalClient)/ratio);
        MSG msg{};msg.hwnd=hwnd;msg.message=WM_NCHITTEST;msg.lParam=MAKELPARAM(global.x,global.y);qintptr result=0;
        const bool actual=window.m_windowChrome->nativeEvent("windows_generic_MSG",&msg,&result) && result==HTCAPTION;
        const bool legacy=title->hitTestCaptionDrag(QPoint(global.x,global.y));
        mismatches+=actual!=expected;oldMismatches+=legacy!=expected;positives+=expected;negatives+=!expected;
        samples.append(QJsonObject{{"name",row.first},{"nativeX",int(global.x)},{"nativeY",int(global.y)},{"expectedCaption",expected},{"actualCaption",actual},{"legacyRawCaption",legacy}});
    }
    report.insert("supported",true);report.insert("devicePixelRatio",ratio);report.insert("samples",samples);
    report.insert("mismatches",mismatches);report.insert("legacyRawMismatches",oldMismatches);
    report.insert("captionPositive",positives);report.insert("controlNegative",negatives);
    report.insert("passed",mismatches==0 && positives>0 && negatives>0);
#else
    Q_UNUSED(window);
#endif
    return report;
}
