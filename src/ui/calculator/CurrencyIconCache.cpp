#include "ui/calculator/CurrencyIconCache.h"

#include <QCoreApplication>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QSet>
#include <QStandardPaths>

namespace {

QNetworkRequest makeImageRequest(const QUrl& url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("PIPBONG/%1").arg(QCoreApplication::applicationVersion()));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QString safeFileToken(const QString& currencyId) {
    QString token = currencyId;
    for (QChar& ch : token) {
        if (!ch.isLetterOrNumber()) {
            ch = QLatin1Char('_');
        }
    }
    return token;
}

} // namespace

CurrencyIconCache::CurrencyIconCache(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this)) {}

CurrencyIconCache::~CurrencyIconCache() = default;

void CurrencyIconCache::setRates(const QList<CurrencyRate>& rates) {
    QSet<QString> activeIds;
    for (const CurrencyRate& rate : rates) {
        if (rate.id.isEmpty() || rate.imageUrl.isEmpty()) {
            continue;
        }
        activeIds.insert(rate.id);
        const QString previousUrl = m_imageUrlById.value(rate.id);
        m_imageUrlById.insert(rate.id, rate.imageUrl);
        if (previousUrl != rate.imageUrl) {
            m_pixmaps.remove(rate.id);
            m_pending.remove(rate.id);
        }
        ensureFetched(rate.id);
    }

    for (auto it = m_imageUrlById.begin(); it != m_imageUrlById.end();) {
        if (!activeIds.contains(it.key())) {
            m_pixmaps.remove(it.key());
            m_pending.remove(it.key());
            it = m_imageUrlById.erase(it);
        } else {
            ++it;
        }
    }
}

QString CurrencyIconCache::imageUrl(const QString& currencyId) const {
    return m_imageUrlById.value(currencyId);
}

void CurrencyIconCache::ensureIcon(const QString& currencyId) {
    ensureFetched(currencyId);
}

QPixmap CurrencyIconCache::pixmap(const QString& currencyId) const {
    return m_pixmaps.value(currencyId);
}

QPixmap CurrencyIconCache::placeholderPixmap() const {
    static const QPixmap placeholder = []() {
        constexpr int size = 24;
        QPixmap pix(size, size);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor(120, 130, 145), 1));
        painter.setBrush(QColor(55, 62, 72));
        painter.drawRoundedRect(QRectF(1, 1, size - 2, size - 2), 4, 4);
        return pix;
    }();
    return placeholder;
}

QPixmap CurrencyIconCache::pixmapOrPlaceholder(const QString& currencyId) const {
    const QPixmap pix = pixmap(currencyId);
    return pix.isNull() ? placeholderPixmap() : pix;
}

QIcon CurrencyIconCache::icon(const QString& currencyId) const {
    const QPixmap pix = pixmap(currencyId);
    if (pix.isNull()) {
        return {};
    }
    return QIcon(pix);
}

QIcon CurrencyIconCache::iconOrPlaceholder(const QString& currencyId) const {
    return QIcon(pixmapOrPlaceholder(currencyId));
}

QString CurrencyIconCache::diskCachePath(const QString& currencyId) const {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                         + QStringLiteral("/poeninja-icons");
    QDir().mkpath(base);
    return base + QLatin1Char('/') + safeFileToken(currencyId) + QStringLiteral(".png");
}

void CurrencyIconCache::ensureFetched(const QString& currencyId) {
    if (currencyId.isEmpty() || m_pixmaps.contains(currencyId) || m_pending.contains(currencyId)) {
        return;
    }
    const QString url = m_imageUrlById.value(currencyId);
    if (url.isEmpty()) {
        return;
    }

    loadFromDisk(currencyId, url);
    if (m_pixmaps.contains(currencyId)) {
        return;
    }

    m_pending.insert(currencyId);
    fetchFromNetwork(currencyId, QUrl(url));
}

void CurrencyIconCache::loadFromDisk(const QString& currencyId, const QString& imageUrl) {
    Q_UNUSED(imageUrl);
    const QString path = diskCachePath(currencyId);
    QPixmap pixmap;
    if (!pixmap.load(path)) {
        return;
    }
    m_pixmaps.insert(currencyId, pixmap);
}

void CurrencyIconCache::fetchFromNetwork(const QString& currencyId, const QUrl& url) {
    QNetworkReply* reply = m_network->get(makeImageRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, currencyId, reply]() {
        m_pending.remove(currencyId);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            return;
        }

        QPixmap pixmap;
        if (!pixmap.loadFromData(reply->readAll())) {
            return;
        }

        m_pixmaps.insert(currencyId, pixmap);
        pixmap.save(diskCachePath(currencyId), "PNG");
        emit iconUpdated(currencyId);
    });
}
