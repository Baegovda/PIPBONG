#include "core/poeninja/EconomyNameKo.h"
#include "core/poeninja/CurrencyNameKo.h"
#include "core/poeninja/PoeNinjaEconomyCategories.h"

#include <QHash>

namespace {

const QHash<QString, QString>& economyKoreanNameByCompositeId() {
    static const QHash<QString, QString> map = {
#include "EconomyNameKoData.inc"
    };
    return map;
}

} // namespace

QString localizedEconomyName(const QString& compositeId, const QString& englishFallback) {
    const QString normalizedId = normalizeEconomyItemCompositeId(compositeId);
    const auto it = economyKoreanNameByCompositeId().constFind(normalizedId);
    if (it != economyKoreanNameByCompositeId().constEnd()) {
        return *it;
    }
    if (economyCategoryFromCompositeId(normalizedId) == QLatin1String("currency")) {
        return localizedCurrencyName(economyApiItemIdFromCompositeId(normalizedId), englishFallback);
    }
    return englishFallback;
}

void localizeEconomyRates(QMap<QString, CurrencyRate>& ratesById) {
    for (auto it = ratesById.begin(); it != ratesById.end(); ++it) {
        CurrencyRate& rate = it.value();
        rate.name = localizedEconomyName(rate.id, rate.name);
    }
}
