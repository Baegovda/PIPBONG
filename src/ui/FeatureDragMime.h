#pragma once

#include <QString>
#include <QStringList>

class QMimeData;

namespace FeatureDragMime {

inline constexpr const char* kMimeType = "application/x-pipbong-feature-drag";

enum class Source {
    Profile,
    Library,
};

struct Payload {
    Source source = Source::Profile;
    /// Primary / first selected id (kept for callers that only need one).
    QString id;
    /// All selected ids in list order (includes @a id as first when non-empty).
    QStringList ids;
    QString profileId;

    bool isValid() const {
        if (ids.isEmpty() && id.isEmpty()) {
            return false;
        }
        if (source == Source::Profile) {
            return !profileId.isEmpty();
        }
        return true;
    }

    QStringList allIds() const {
        if (!ids.isEmpty()) {
            return ids;
        }
        if (!id.isEmpty()) {
            return QStringList{id};
        }
        return {};
    }
};

QMimeData* createMimeData(const Payload& payload);
bool accepts(const QMimeData* mime);
Payload parse(const QMimeData* mime);

} // namespace FeatureDragMime
