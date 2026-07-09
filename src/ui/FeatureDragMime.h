#pragma once

#include <QString>

class QMimeData;

namespace FeatureDragMime {

inline constexpr const char* kMimeType = "application/x-pipbong-feature-drag";

enum class Source {
    Profile,
    Library,
};

struct Payload {
    Source source = Source::Profile;
    QString id;
    QString profileId;

    bool isValid() const {
        if (id.isEmpty()) {
            return false;
        }
        if (source == Source::Profile) {
            return !profileId.isEmpty();
        }
        return true;
    }
};

QMimeData* createMimeData(const Payload& payload);
bool accepts(const QMimeData* mime);
Payload parse(const QMimeData* mime);

} // namespace FeatureDragMime
