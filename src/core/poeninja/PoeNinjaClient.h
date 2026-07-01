#pragma once

#include "core/poeninja/PoeNinjaTypes.h"

#include <QObject>
#include <QSet>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

class PoeNinjaClient : public QObject {
    Q_OBJECT
public:
    explicit PoeNinjaClient(QObject* parent = nullptr);
    ~PoeNinjaClient() override;

    bool isBusy() const;
    std::optional<EconomySnapshot> cachedSnapshot() const;
    std::optional<IndexStateResult> cachedIndexState() const;

    void fetchIndexState(std::function<void(IndexStateResult)> onFinished);
    void fetchCurrencyExchange(const QString& leagueDisplayName,
                               std::function<void(EconomySnapshot)> onFinished);

signals:
    void busyChanged(bool busy);

private:
    void setBusy(bool busy);
    void abortActiveReplies();
    QNetworkReply* getJson(const QUrl& url, bool exclusive = true);

    void onIndexStateReplyFinished(std::function<void(IndexStateResult)> onFinished);
    void startEconomyExchangeFetch(const QString& leagueDisplayName,
                                   const IndexStateResult& indexState,
                                   std::function<void(EconomySnapshot)> onFinished);

    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_activeReply = nullptr;
    QSet<QNetworkReply*> m_activeReplies;

    std::optional<IndexStateResult> m_cachedIndexState;
    std::optional<EconomySnapshot> m_cachedSnapshot;
};
