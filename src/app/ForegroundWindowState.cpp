#include "ForegroundWindowState.h"

bool ForegroundWindowState::isEquivalentTo(const ForegroundWindowState& other) const {
#ifdef _WIN32
    if (rootHwnd != other.rootHwnd) {
        return false;
    }
#endif
    return title == other.title && processPath == other.processPath && pipbong == other.pipbong
           && shellTransient == other.shellTransient;
}
