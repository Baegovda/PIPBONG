#include "ForegroundRunGate.h"

#include "core/capture/ScreenCapture.h"

#include <QtGlobal>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ForegroundRunGate {

#ifdef _WIN32
HWND findVisibleTopLevelWindowHwnd(const QString& binding, const QString& processPath) {
    if (binding.isEmpty()) {
        return nullptr;
    }
    return ScreenCapture::findVisibleWindowMatchingTitle(binding.toStdWString(),
                                                         processPath.toStdWString());
}

bool foregroundHwndMatchesLinkedProcess(HWND hwnd,
                                        const QString& mainProcessPath,
                                        const QString& subProcessPath) {
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }
    ScreenCapture::TargetWindowInfo info;
    if (!ScreenCapture::queryWindowInfo(hwnd, info) || info.processPath.empty()) {
        return false;
    }
    const QString processPath = QString::fromStdWString(info.processPath);
    if (!mainProcessPath.isEmpty()
        && processPath.compare(mainProcessPath, Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (!subProcessPath.isEmpty()
        && processPath.compare(subProcessPath, Qt::CaseInsensitive) == 0) {
        return true;
    }
    return false;
}

bool foregroundMatchesScopedSubTarget(const QString& foregroundTitle,
                                      HWND foregroundHwnd,
                                      const QString& mainBinding,
                                      const QString& subBinding,
                                      const QString& mainProcessPath,
                                      const QString& subProcessPath) {
    if (subBinding.isEmpty() || !foregroundHwnd) {
        return false;
    }
    if (foregroundHwndMatchesLinkedProcess(foregroundHwnd, {}, subProcessPath)) {
        return true;
    }
    if (foregroundTitle.isEmpty()) {
        return false;
    }
    if (!foregroundTitle.contains(subBinding, Qt::CaseInsensitive)) {
        return false;
    }
    if (mainBinding.isEmpty()) {
        return true;
    }
    if (!foregroundTitle.contains(mainBinding, Qt::CaseInsensitive)) {
        return true;
    }

    const HWND mainHwnd = findVisibleTopLevelWindowHwnd(mainBinding, mainProcessPath);
    const HWND subHwnd = findVisibleTopLevelWindowHwnd(subBinding, subProcessPath);
    if (mainHwnd && subHwnd && mainHwnd == subHwnd) {
        return true;
    }
    if (mainHwnd && foregroundHwnd == mainHwnd && mainHwnd != subHwnd) {
        return false;
    }
    if (subHwnd && foregroundHwnd == subHwnd) {
        return true;
    }
    if (mainHwnd && foregroundHwnd == mainHwnd && mainHwnd != subHwnd) {
        return false;
    }
    if (subBinding.length() > mainBinding.length()) {
        return true;
    }
    return mainHwnd == nullptr || foregroundHwnd != mainHwnd;
}

bool foregroundMatchesScopedMainTarget(const QString& foregroundTitle,
                                       HWND foregroundHwnd,
                                       const QString& mainBinding,
                                       const QString& subBinding,
                                       const QString& mainProcessPath,
                                       const QString& subProcessPath) {
    if (mainBinding.isEmpty() || !foregroundHwnd) {
        return false;
    }
    if (foregroundHwndMatchesLinkedProcess(foregroundHwnd, mainProcessPath, {})) {
        return true;
    }
    if (foregroundTitle.isEmpty()) {
        return false;
    }
    if (!foregroundTitle.contains(mainBinding, Qt::CaseInsensitive)) {
        return false;
    }
    if (subBinding.isEmpty()) {
        const HWND mainHwnd = findVisibleTopLevelWindowHwnd(mainBinding, mainProcessPath);
        return mainHwnd != nullptr && foregroundHwnd == mainHwnd;
    }
    if (!foregroundTitle.contains(subBinding, Qt::CaseInsensitive)) {
        return true;
    }

    const HWND mainHwnd = findVisibleTopLevelWindowHwnd(mainBinding, mainProcessPath);
    const HWND subHwnd = findVisibleTopLevelWindowHwnd(subBinding, subProcessPath);
    if (mainHwnd && subHwnd && mainHwnd == subHwnd) {
        return true;
    }
    if (subHwnd && foregroundHwnd == subHwnd && subHwnd != mainHwnd) {
        return false;
    }
    if (mainHwnd && foregroundHwnd == mainHwnd) {
        return true;
    }
    if (subHwnd && foregroundHwnd == subHwnd && subHwnd != mainHwnd) {
        return false;
    }
    if (mainBinding.length() > subBinding.length()) {
        return true;
    }
    return subHwnd == nullptr || foregroundHwnd != subHwnd;
}
#endif

} // namespace ForegroundRunGate
