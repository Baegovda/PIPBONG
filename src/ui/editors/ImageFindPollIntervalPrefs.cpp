#include "ui/editors/ImageFindPollIntervalPrefs.h"

#include "core/workflow/blocks/ImageFindBlock.h"

#include <QSettings>

namespace {

constexpr const char* kLastPollIntervalMsKey = "ui/state/imageFindEditor/lastPollIntervalMs";

} // namespace

int lastImageFindPollIntervalMs() {
    QSettings settings;
    return snapImageFindPollIntervalMs(
        settings.value(kLastPollIntervalMsKey, kDefaultImageFindPollIntervalMs).toInt());
}

void saveLastImageFindPollIntervalMs(int ms) {
    QSettings settings;
    settings.setValue(kLastPollIntervalMsKey, snapImageFindPollIntervalMs(ms));
}
