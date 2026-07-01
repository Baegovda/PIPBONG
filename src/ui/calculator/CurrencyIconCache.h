#pragma once

#include "core/poeninja/PoeNinjaTypes.h"

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QPixmap>
#include <QSet>

class QNetworkAccessManager;

class CurrencyIconCache : public QObject {
    Q_OBJECT
public:
    explicit CurrencyIconCache(QObject* parent = nullptr);
    ~CurrencyIconCache() override;

    void setRates(const QList<CurrencyRate>& rates);
    QPixmap pixmap(const QString& currencyId) const;
    QPixmap pixmapOrPlaceholder(const QString& currencyId) const;
    QIcon icon(const QString& currencyId) const;
    QIcon iconOrPlaceholder(const QString& currencyId) const;
    QPixmap placeholderPixmap() const;
    QString imageUrl(const QString& currencyId) const;
    void ensureIcon(const QString& currencyId);

signals:
    void iconUpdated(const QString& currencyId);

private:
    void ensureFetched(const QString& currencyId);
    void loadFromDisk(const QString& currencyId, const QString& imageUrl);
    void fetchFromNetwork(const QString& currencyId, const QUrl& url);
    QString diskCachePath(const QString& currencyId) const;

    QNetworkAccessManager* m_network = nullptr;
    QHash<QString, QString> m_imageUrlById;
    QHash<QString, QPixmap> m_pixmaps;
    QSet<QString> m_pending;
};
