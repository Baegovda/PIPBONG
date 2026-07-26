#pragma once

#include <QString>

#ifdef _WIN32
#    include <windows.h>
#endif

struct ForegroundWindowState {
#ifdef _WIN32
    HWND rootHwnd = nullptr;
#endif
    QString title;
    QString processPath;
    bool pipbong = false;
    bool shellTransient = false;
    quint64 monotonicSeq = 0;
    qint64 changedAtMs = 0;

    bool isEquivalentTo(const ForegroundWindowState& other) const;
};
