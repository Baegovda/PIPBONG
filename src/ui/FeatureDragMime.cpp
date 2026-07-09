#include "ui/FeatureDragMime.h"

#include <QMimeData>

#include <nlohmann/json.hpp>

#include <optional>

namespace FeatureDragMime {

namespace {

QString sourceToString(Source source) {
    switch (source) {
    case Source::Profile:
        return QStringLiteral("profile");
    case Source::Library:
        return QStringLiteral("library");
    }
    return {};
}

std::optional<Source> sourceFromString(const QString& value) {
    if (value == QStringLiteral("profile")) {
        return Source::Profile;
    }
    if (value == QStringLiteral("library")) {
        return Source::Library;
    }
    return std::nullopt;
}

} // namespace

QMimeData* createMimeData(const Payload& payload) {
    if (!payload.isValid()) {
        return nullptr;
    }

    nlohmann::json json;
    json["source"] = sourceToString(payload.source).toStdString();
    json["id"] = payload.id.toStdString();
    if (payload.source == Source::Profile) {
        json["profileId"] = payload.profileId.toStdString();
    }

    auto* mime = new QMimeData();
    mime->setData(kMimeType, QByteArray::fromStdString(json.dump()));
    return mime;
}

bool accepts(const QMimeData* mime) {
    return mime && mime->hasFormat(kMimeType);
}

Payload parse(const QMimeData* mime) {
    Payload payload;
    if (!accepts(mime)) {
        return payload;
    }

    const QByteArray raw = mime->data(kMimeType);
    if (raw.isEmpty()) {
        return payload;
    }

    try {
        const nlohmann::json json = nlohmann::json::parse(raw.constData());
        const auto source = sourceFromString(QString::fromStdString(json.value("source", std::string{})));
        if (!source) {
            return {};
        }
        payload.source = *source;
        payload.id = QString::fromStdString(json.value("id", std::string{}));
        payload.profileId = QString::fromStdString(json.value("profileId", std::string{}));
    } catch (...) {
        return {};
    }

    if (!payload.isValid()) {
        return {};
    }
    return payload;
}

} // namespace FeatureDragMime
