#pragma once

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <optional>

struct PoeNinjaLeagueInfo {
    QString displayName;
    QString url;
    bool indexed = false;
    bool hardcore = false;
};

struct CurrencyRate {
    QString id;
    QString categoryId;
    QString apiItemId;
    QString name;
    QString imageUrl;
    double primaryValue = 0.0;
    double volumePrimaryValue = 0.0;
    double changePercent = 0.0;
};

struct EconomySnapshot {
    QString leagueDisplayName;
    QString version;
    QMap<QString, CurrencyRate> ratesById;
    QDateTime fetchedAt;
    bool valid = false;
};

struct IndexStateResult {
    bool success = false;
    QString errorMessage;
    QList<PoeNinjaLeagueInfo> economyLeagues;
    QMap<QString, QString> versionByLeagueUrl;
};

inline QString poeNinjaAssetUrl(const QString& imagePath) {
    if (imagePath.isEmpty()) {
        return {};
    }
    if (imagePath.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || imagePath.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        return imagePath;
    }
    // PoE2 exchange API returns /gen/image/... paths served from GGG CDN, not poe.ninja.
    if (imagePath.startsWith(QLatin1Char('/'))) {
        return QStringLiteral("https://web.poecdn.com") + imagePath;
    }
    return QStringLiteral("https://web.poecdn.com/") + imagePath;
}
