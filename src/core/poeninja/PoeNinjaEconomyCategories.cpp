#include "core/poeninja/PoeNinjaEconomyCategories.h"

QList<EconomyCategoryDef> poeNinjaEconomyCategories() {
    return {
        {QStringLiteral("currency"), QStringLiteral("Currency")},
        {QStringLiteral("fragments"), QStringLiteral("Fragments")},
        {QStringLiteral("abyssal-bones"), QStringLiteral("Abyss")},
        {QStringLiteral("uncut-gems"), QStringLiteral("UncutGems")},
        {QStringLiteral("lineage-support-gems"), QStringLiteral("LineageSupportGems")},
        {QStringLiteral("essences"), QStringLiteral("Essences")},
        {QStringLiteral("soul-cores"), QStringLiteral("SoulCores")},
        {QStringLiteral("idols"), QStringLiteral("Idols")},
        {QStringLiteral("runes"), QStringLiteral("Runes")},
        {QStringLiteral("omens"), QStringLiteral("Ritual")},
        {QStringLiteral("expedition"), QStringLiteral("Expedition")},
        {QStringLiteral("liquid-emotions"), QStringLiteral("Delirium")},
        {QStringLiteral("breach-catalyst"), QStringLiteral("Breach")},
        {QStringLiteral("verisium"), QStringLiteral("Verisium")},
    };
}

QString economyCategoryLabel(const QString& categoryId) {
    if (categoryId == QLatin1String("currency")) {
        return QStringLiteral("화폐");
    }
    if (categoryId == QLatin1String("fragments")) {
        return QStringLiteral("파편");
    }
    if (categoryId == QLatin1String("abyssal-bones")) {
        return QStringLiteral("심연의 뼈");
    }
    if (categoryId == QLatin1String("uncut-gems")) {
        return QStringLiteral("미가공 젬");
    }
    if (categoryId == QLatin1String("lineage-support-gems")) {
        return QStringLiteral("혈통 보조 젬");
    }
    if (categoryId == QLatin1String("essences")) {
        return QStringLiteral("에센스");
    }
    if (categoryId == QLatin1String("soul-cores")) {
        return QStringLiteral("영혼 핵");
    }
    if (categoryId == QLatin1String("idols")) {
        return QStringLiteral("우상");
    }
    if (categoryId == QLatin1String("runes")) {
        return QStringLiteral("룬");
    }
    if (categoryId == QLatin1String("omens")) {
        return QStringLiteral("징조");
    }
    if (categoryId == QLatin1String("expedition")) {
        return QStringLiteral("탐험");
    }
    if (categoryId == QLatin1String("liquid-emotions")) {
        return QStringLiteral("액체 감정");
    }
    if (categoryId == QLatin1String("breach-catalyst")) {
        return QStringLiteral("균열 촉매");
    }
    if (categoryId == QLatin1String("verisium")) {
        return QStringLiteral("베리시움");
    }
    return categoryId;
}

QString economyItemCompositeId(const QString& categoryId, const QString& apiItemId) {
    return categoryId + QLatin1Char(':') + apiItemId;
}

QString economyCategoryFromCompositeId(const QString& compositeId) {
    const int separator = compositeId.indexOf(QLatin1Char(':'));
    if (separator < 0) {
        return QStringLiteral("currency");
    }
    return compositeId.left(separator);
}

QString economyApiItemIdFromCompositeId(const QString& compositeId) {
    const int separator = compositeId.indexOf(QLatin1Char(':'));
    if (separator < 0) {
        return compositeId;
    }
    return compositeId.mid(separator + 1);
}

QString normalizeEconomyItemCompositeId(const QString& storedId) {
    if (storedId.contains(QLatin1Char(':'))) {
        return storedId;
    }
    return economyItemCompositeId(QStringLiteral("currency"), storedId);
}

bool economyItemIdIsCurrencyCategory(const QString& compositeOrLegacyId) {
    return economyCategoryFromCompositeId(compositeOrLegacyId) == QLatin1String("currency");
}
