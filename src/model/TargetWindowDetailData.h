#pragma once
#include <QString>

struct TargetWindowDetailData {
    QString hwnd;
    QString title;
    QString className;
    QString frameBounds;
    QString clientSize;
    QString monitor;
    QString processName;
    QString processPath;
    QString stateText;
    bool subTarget = false;
    bool minimized = false;
    bool visible = true;
    int monitorDpi = 0;
};

