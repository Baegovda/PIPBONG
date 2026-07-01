#pragma once

#include <QStringList>

class EconomyFavoritesStore {
public:
    static EconomyFavoritesStore& instance();

    QStringList favorites() const;
    bool contains(const QString& compositeId) const;
    bool add(const QString& compositeId);
    bool remove(const QString& compositeId);
    bool toggle(const QString& compositeId);

private:
    EconomyFavoritesStore();
    void load();
    void save();

    QStringList m_ids;
};

inline QString economyFavoritesCategoryId() {
    return QStringLiteral("__favorites__");
}
