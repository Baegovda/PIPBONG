#include "app/ForegroundRunGate.h"

#ifdef _WIN32
namespace ForegroundRunGate {

HWND findVisibleTopLevelWindowHwnd(const QString& binding, const QString& processPath) {
    Q_UNUSED(binding);
    Q_UNUSED(processPath);
    return nullptr;
}

bool foregroundHwndMatchesLinkedProcess(HWND hwnd,
                                        const QString& mainProcessPath,
                                        const QString& subProcessPath) {
    Q_UNUSED(hwnd);
    Q_UNUSED(mainProcessPath);
    Q_UNUSED(subProcessPath);
    return false;
}

bool foregroundMatchesScopedSubTarget(const QString& foregroundTitle,
                                      HWND foregroundHwnd,
                                      const QString& mainBinding,
                                      const QString& subBinding,
                                      const QString& mainProcessPath,
                                      const QString& subProcessPath) {
    Q_UNUSED(foregroundTitle);
    Q_UNUSED(foregroundHwnd);
    Q_UNUSED(mainBinding);
    Q_UNUSED(subBinding);
    Q_UNUSED(mainProcessPath);
    Q_UNUSED(subProcessPath);
    return false;
}

bool foregroundMatchesScopedMainTarget(const QString& foregroundTitle,
                                       HWND foregroundHwnd,
                                       const QString& mainBinding,
                                       const QString& subBinding,
                                       const QString& mainProcessPath,
                                       const QString& subProcessPath) {
    Q_UNUSED(foregroundTitle);
    Q_UNUSED(foregroundHwnd);
    Q_UNUSED(mainBinding);
    Q_UNUSED(subBinding);
    Q_UNUSED(mainProcessPath);
    Q_UNUSED(subProcessPath);
    return false;
}

} // namespace ForegroundRunGate
#endif
