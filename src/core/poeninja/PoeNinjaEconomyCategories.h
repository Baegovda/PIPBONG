#pragma once

#include <QList>
#include <QString>

struct EconomyCategoryDef {
    QString id;
    QString apiType;
};

// PoE2 currency exchange overview categories (poe.ninja exchange API `type` query).
QList<EconomyCategoryDef> poeNinjaEconomyCategories();

QString economyCategoryLabel(const QString& categoryId);

QString economyItemCompositeId(const QString& categoryId, const QString& apiItemId);
QString economyCategoryFromCompositeId(const QString& compositeId);
QString economyApiItemIdFromCompositeId(const QString& compositeId);
QString normalizeEconomyItemCompositeId(const QString& storedId);

bool economyItemIdIsCurrencyCategory(const QString& compositeOrLegacyId);
