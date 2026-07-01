#include "core/poeninja/PoeNinjaClient.h"
#include "core/poeninja/EconomyNameKo.h"
#include "core/poeninja/PoeNinjaEconomyCategories.h"
#include "core/poeninja/PoeNinjaTypes.h"

#include "PipbongVersion.h"

#include <nlohmann/json.hpp>

#include <QCoreApplication>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#include <memory>

namespace {

constexpr auto kPoe2ApiBase = "https://poe.ninja/poe2/api";

QNetworkRequest makeRequest(const QUrl& url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("PIPBONG/%1").arg(QCoreApplication::applicationVersion()));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QString leagueUrlForDisplayName(const IndexStateResult& indexState, const QString& displayName) {
    for (const PoeNinjaLeagueInfo& league : indexState.economyLeagues) {
        if (league.displayName.compare(displayName, Qt::CaseInsensitive) == 0) {
            return league.url;
        }
    }
    return {};
}

QString versionForLeagueUrl(const IndexStateResult& indexState, const QString& leagueUrl) {
    return indexState.versionByLeagueUrl.value(leagueUrl);
}

QString jsonIdAsString(const nlohmann::json& value) {
    if (value.is_string()) {
        return QString::fromStdString(value.get<std::string>());
    }
    if (value.is_number_integer()) {
        return QString::number(value.get<int>());
    }
    if (value.is_number_unsigned()) {
        return QString::number(static_cast<qulonglong>(value.get<unsigned int>()));
    }
    return {};
}

void mergeExchangeOverview(const nlohmann::json& document,
                           const EconomyCategoryDef& category,
                           EconomySnapshot& snapshot) {
    QMap<QString, QString> namesById;
    QMap<QString, QString> imageUrlById;
    if (document.contains("items") && document["items"].is_array()) {
        for (const auto& item : document["items"]) {
            const QString apiItemId = jsonIdAsString(item["id"]);
            const QString name = QString::fromStdString(item.value("name", std::string()));
            const QString imagePath = QString::fromStdString(item.value("image", std::string()));
            if (!apiItemId.isEmpty() && !name.isEmpty()) {
                namesById.insert(apiItemId, name);
            }
            if (!apiItemId.isEmpty() && !imagePath.isEmpty()) {
                imageUrlById.insert(apiItemId, poeNinjaAssetUrl(imagePath));
            }
        }
    }

    if (!document.contains("lines") || !document["lines"].is_array()) {
        return;
    }

    for (const auto& line : document["lines"]) {
        const QString apiItemId = jsonIdAsString(line["id"]);
        if (apiItemId.isEmpty()) {
            continue;
        }

        CurrencyRate rate;
        rate.categoryId = category.id;
        rate.apiItemId = apiItemId;
        rate.id = economyItemCompositeId(category.id, apiItemId);
        rate.name = namesById.value(apiItemId);
        rate.imageUrl = imageUrlById.value(apiItemId);
        if (rate.name.isEmpty() && line.contains("name")) {
            rate.name = QString::fromStdString(line.value("name", std::string()));
        }
        rate.primaryValue = line.value("primaryValue", 0.0);
        rate.volumePrimaryValue = line.value("volumePrimaryValue", 0.0);
        if (line.contains("sparkline") && line["sparkline"].is_object()) {
            rate.changePercent = line["sparkline"].value("totalChange", 0.0);
        }
        snapshot.ratesById.insert(rate.id, rate);
    }
}

IndexStateResult parseIndexState(const QByteArray& body) {
    IndexStateResult result;
    try {
        const auto document = nlohmann::json::parse(body.constData());

        if (document.contains("economyLeagues") && document["economyLeagues"].is_array()) {
            for (const auto& entry : document["economyLeagues"]) {
                PoeNinjaLeagueInfo league;
                league.displayName = QString::fromStdString(entry.value("displayName", std::string()));
                if (league.displayName.isEmpty()) {
                    league.displayName = QString::fromStdString(entry.value("name", std::string()));
                }
                league.url = QString::fromStdString(entry.value("url", std::string()));
                league.indexed = entry.value("indexed", false);
                league.hardcore = entry.value("hardcore", false);
                if (!league.displayName.isEmpty()) {
                    result.economyLeagues.append(league);
                }
            }
        }

        if (document.contains("snapshotVersions") && document["snapshotVersions"].is_array()) {
            for (const auto& entry : document["snapshotVersions"]) {
                const QString url = QString::fromStdString(entry.value("url", std::string()));
                const QString version = QString::fromStdString(entry.value("version", std::string()));
                if (!url.isEmpty() && !version.isEmpty()) {
                    result.versionByLeagueUrl.insert(url, version);
                }
            }
        }

        result.success = !result.economyLeagues.isEmpty();
        if (!result.success) {
            result.errorMessage = QStringLiteral("index-state에 리그 정보가 없습니다.");
        }
    } catch (const std::exception& ex) {
        result.success = false;
        result.errorMessage = QString::fromUtf8(ex.what());
    }
    return result;
}

struct EconomyBatchFetch {
    EconomySnapshot snapshot;
    int remaining = 0;
    std::function<void(EconomySnapshot)> onFinished;
};

} // namespace

PoeNinjaClient::PoeNinjaClient(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this)) {}

PoeNinjaClient::~PoeNinjaClient() {
    abortActiveReplies();
}

bool PoeNinjaClient::isBusy() const {
    return !m_activeReplies.isEmpty() || m_activeReply != nullptr;
}

std::optional<EconomySnapshot> PoeNinjaClient::cachedSnapshot() const {
    return m_cachedSnapshot;
}

std::optional<IndexStateResult> PoeNinjaClient::cachedIndexState() const {
    return m_cachedIndexState;
}

void PoeNinjaClient::setBusy(bool busy) {
    Q_UNUSED(busy);
    emit busyChanged(isBusy());
}

void PoeNinjaClient::abortActiveReplies() {
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
    for (QNetworkReply* reply : m_activeReplies) {
        reply->abort();
        reply->deleteLater();
    }
    m_activeReplies.clear();
    setBusy(false);
}

QNetworkReply* PoeNinjaClient::getJson(const QUrl& url, bool exclusive) {
    if (exclusive) {
        if (m_activeReply) {
            m_activeReply->abort();
            m_activeReply->deleteLater();
            m_activeReply = nullptr;
        }
    }
    QNetworkReply* reply = m_network->get(makeRequest(url));
    if (exclusive) {
        m_activeReply = reply;
    } else {
        m_activeReplies.insert(reply);
    }
    setBusy(true);
    return reply;
}

void PoeNinjaClient::fetchIndexState(std::function<void(IndexStateResult)> onFinished) {
    const QUrl url(QStringLiteral("%1/data/index-state").arg(QLatin1String(kPoe2ApiBase)));
    QNetworkReply* reply = getJson(url);
    connect(reply, &QNetworkReply::finished, this, [this, onFinished]() {
        onIndexStateReplyFinished(std::move(onFinished));
    });
}

void PoeNinjaClient::onIndexStateReplyFinished(std::function<void(IndexStateResult)> onFinished) {
    QNetworkReply* reply = m_activeReply;
    m_activeReply = nullptr;
    setBusy(false);

    IndexStateResult result;
    if (!reply) {
        result.errorMessage = QStringLiteral("응답이 없습니다.");
        if (onFinished) {
            onFinished(result);
        }
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        result.errorMessage = reply->errorString();
        if (onFinished) {
            onFinished(result);
        }
        return;
    }

    result = parseIndexState(reply->readAll());
    if (result.success) {
        m_cachedIndexState = result;
    }
    if (onFinished) {
        onFinished(result);
    }
}

void PoeNinjaClient::fetchCurrencyExchange(const QString& leagueDisplayName,
                                           std::function<void(EconomySnapshot)> onFinished) {
    auto finishWithSnapshot = [this, onFinished](EconomySnapshot snapshot) {
        if (snapshot.valid) {
            m_cachedSnapshot = snapshot;
        }
        if (onFinished) {
            onFinished(std::move(snapshot));
        }
    };

    if (m_cachedIndexState && m_cachedIndexState->success) {
        startEconomyExchangeFetch(leagueDisplayName, *m_cachedIndexState, std::move(finishWithSnapshot));
        return;
    }

    fetchIndexState([this, leagueDisplayName, finishWithSnapshot](IndexStateResult indexState) {
        if (!indexState.success) {
            EconomySnapshot snapshot;
            snapshot.leagueDisplayName = leagueDisplayName;
            if (m_cachedSnapshot) {
                snapshot = *m_cachedSnapshot;
            }
            finishWithSnapshot(std::move(snapshot));
            return;
        }
        startEconomyExchangeFetch(leagueDisplayName, indexState, std::move(finishWithSnapshot));
    });
}

void PoeNinjaClient::startEconomyExchangeFetch(const QString& leagueDisplayName,
                                               const IndexStateResult& indexState,
                                               std::function<void(EconomySnapshot)> onFinished) {
    const QString leagueUrl = leagueUrlForDisplayName(indexState, leagueDisplayName);
    const QString version = versionForLeagueUrl(indexState, leagueUrl);
    if (leagueUrl.isEmpty() || version.isEmpty()) {
        EconomySnapshot snapshot;
        snapshot.leagueDisplayName = leagueDisplayName;
        if (m_cachedSnapshot) {
            snapshot = *m_cachedSnapshot;
        }
        if (onFinished) {
            onFinished(std::move(snapshot));
        }
        return;
    }

    abortActiveReplies();

    const QList<EconomyCategoryDef> categories = poeNinjaEconomyCategories();
    auto batch = std::make_shared<EconomyBatchFetch>();
    batch->snapshot.leagueDisplayName = leagueDisplayName;
    batch->snapshot.version = version;
    batch->snapshot.fetchedAt = QDateTime::currentDateTimeUtc();
    batch->remaining = categories.size();
    batch->onFinished = std::move(onFinished);

    for (const EconomyCategoryDef& category : categories) {
        QUrl url(QStringLiteral("%1/economy/exchange/%2/overview")
                     .arg(QLatin1String(kPoe2ApiBase), version));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("league"), leagueDisplayName);
        query.addQueryItem(QStringLiteral("type"), category.apiType);
        url.setQuery(query);

        QNetworkReply* reply = getJson(url, false);
        connect(reply, &QNetworkReply::finished, this, [this, batch, category, reply]() {
            m_activeReplies.remove(reply);
            reply->deleteLater();

            if (reply->error() == QNetworkReply::NoError) {
                try {
                    const auto document = nlohmann::json::parse(reply->readAll().constData());
                    mergeExchangeOverview(document, category, batch->snapshot);
                } catch (const std::exception&) {
                }
            }

            --batch->remaining;
            if (batch->remaining > 0) {
                setBusy(true);
                return;
            }

            batch->snapshot.valid = !batch->snapshot.ratesById.isEmpty();
            if (batch->snapshot.valid) {
                localizeEconomyRates(batch->snapshot.ratesById);
                m_cachedSnapshot = batch->snapshot;
            } else if (m_cachedSnapshot) {
                batch->snapshot = *m_cachedSnapshot;
            }

            setBusy(false);
            if (batch->onFinished) {
                batch->onFinished(std::move(batch->snapshot));
            }
        });
    }
}
