#include "ui/calculator/EconomyFavoritesStore.h"

#include <QSettings>

namespace {

constexpr auto kFavoritesSettingsKey = "calculator/economyFavorites_v1";

} // namespace

EconomyFavoritesStore& EconomyFavoritesStore::instance() {
    static EconomyFavoritesStore store;
    return store;
}

EconomyFavoritesStore::EconomyFavoritesStore() {
    load();
}

QStringList EconomyFavoritesStore::favorites() const {
    return m_ids;
}

bool EconomyFavoritesStore::contains(const QString& compositeId) const {
    return m_ids.contains(compositeId);
}

bool EconomyFavoritesStore::add(const QString& compositeId) {
    if (compositeId.isEmpty() || m_ids.contains(compositeId)) {
        return false;
    }
    m_ids.prepend(compositeId);
    save();
    return true;
}

bool EconomyFavoritesStore::remove(const QString& compositeId) {
    if (!m_ids.removeAll(compositeId)) {
        return false;
    }
    save();
    return true;
}

bool EconomyFavoritesStore::toggle(const QString& compositeId) {
    if (compositeId.isEmpty()) {
        return false;
    }
    if (m_ids.contains(compositeId)) {
        remove(compositeId);
        return false;
    }
    add(compositeId);
    return true;
}

void EconomyFavoritesStore::load() {
    const QSettings settings;
    m_ids = settings.value(QLatin1String(kFavoritesSettingsKey)).toStringList();
    m_ids.removeDuplicates();
}

void EconomyFavoritesStore::save() {
    QSettings settings;
    settings.setValue(QLatin1String(kFavoritesSettingsKey), m_ids);
}
