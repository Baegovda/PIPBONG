#pragma once

#include "core/poeninja/PoeNinjaTypes.h"

#include <QString>

// Official Korean display names from PoE2DB (poe2db.tw/kr/Currency and per-item pages).
QString localizedCurrencyName(const QString& currencyId, const QString& englishFallback);

void localizeCurrencyRates(QMap<QString, CurrencyRate>& ratesById);
