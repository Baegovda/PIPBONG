#pragma once

#include "core/poeninja/PoeNinjaTypes.h"

#include <QString>

QString localizedEconomyName(const QString& compositeId, const QString& englishFallback);

void localizeEconomyRates(QMap<QString, CurrencyRate>& ratesById);
