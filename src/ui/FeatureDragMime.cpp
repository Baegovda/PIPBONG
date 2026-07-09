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

    QStringList ids = payload.allIds();
    if (ids.isEmpty()) {
        return nullptr;
    }

    nlohmann::json json;
    json["source"] = sourceToString(payload.source).toStdString();
    json["id"] = ids.first().toStdString();
    nlohmann::json idsJson = nlohmann::json::array();
    for (const QString& entryId : ids) {
        idsJson.push_back(entryId.toStdString());
    }
    json["ids"] = idsJson;
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
        if (json.contains("ids") && json["ids"].is_array()) {
            for (const auto& entry : json["ids"]) {
                if (!entry.is_string()) {
                    continue;
                }
                const QString entryId = QString::fromStdString(entry.get<std::string>());
                if (!entryId.isEmpty() && !payload.ids.contains(entryId)) {
                    payload.ids.push_back(entryId);
                }
            }
        }
        if (payload.ids.isEmpty() && !payload.id.isEmpty()) {
            payload.ids.push_back(payload.id);
        } else if (!payload.ids.isEmpty() && payload.id.isEmpty()) {
            payload.id = payload.ids.first();
        }
    } catch (...) {
        return {};
    }

    if (!payload.isValid()) {
        return {};
    }
    return payload;
}

} // namespace FeatureDragMime
