#include "core/poeninja/CurrencyNameKo.h"
#include "core/poeninja/PoeNinjaEconomyCategories.h"

#include <QHash>

namespace {

const QHash<QString, QString>& koreanNameById() {
    static const QHash<QString, QString> map = {
        {QStringLiteral("ancient-infuser"), QStringLiteral("고대 주입기")},
        {QStringLiteral("etcher"), QStringLiteral("신비학자의 식각기")},
        {QStringLiteral("architects-orb"), QStringLiteral("건축가의 오브")},
        {QStringLiteral("scrap"), QStringLiteral("방어구 장인의 고철")},
        {QStringLiteral("artificers"), QStringLiteral("숙련공의 오브")},
        {QStringLiteral("artificers-shard"), QStringLiteral("숙련공의 파편")},
        {QStringLiteral("whetstone"), QStringLiteral("대장장이의 숫돌")},
        {QStringLiteral("chance-shard"), QStringLiteral("기회의 파편")},
        {QStringLiteral("chaos"), QStringLiteral("카오스 오브")},
        {QStringLiteral("core-destabiliser"), QStringLiteral("핵 불안정화기")},
        {QStringLiteral("cryptic-key"), QStringLiteral("난해한 열쇠")},
        {QStringLiteral("crystallised-corruption"), QStringLiteral("결정화된 타락")},
        {QStringLiteral("divine"), QStringLiteral("신성한 오브")},
        {QStringLiteral("exalted"), QStringLiteral("엑잘티드 오브")},
        {QStringLiteral("fracturing-orb"), QStringLiteral("분열의 오브")},
        {QStringLiteral("gcp"), QStringLiteral("세공사의 프리즘")},
        {QStringLiteral("bauble"), QStringLiteral("유리직공의 방울")},
        {QStringLiteral("greater-chaos-orb"), QStringLiteral("상위 카오스 오브")},
        {QStringLiteral("greater-exalted-orb"), QStringLiteral("상위 엑잘티드 오브")},
        {QStringLiteral("greater-jewellers-orb"), QStringLiteral("상위 쥬얼러 오브")},
        {QStringLiteral("greater-orb-of-augmentation"), QStringLiteral("상위 확장의 오브")},
        {QStringLiteral("greater-orb-of-transmutation"), QStringLiteral("상위 진화의 오브")},
        {QStringLiteral("greater-regal-orb"), QStringLiteral("상위 제왕의 오브")},
        {QStringLiteral("hinekoras-lock"), QStringLiteral("히네코라의 머리카락")},
        {QStringLiteral("lesser-jewellers-orb"), QStringLiteral("하위 쥬얼러 오브")},
        {QStringLiteral("mirror"), QStringLiteral("칼란드라의 거울")},
        {QStringLiteral("alch"), QStringLiteral("연금술의 오브")},
        {QStringLiteral("annul"), QStringLiteral("소멸의 오브")},
        {QStringLiteral("aug"), QStringLiteral("확장의 오브")},
        {QStringLiteral("chance"), QStringLiteral("기회의 오브")},
        {QStringLiteral("orb-of-extraction"), QStringLiteral("추출의 오브")},
        {QStringLiteral("transmute"), QStringLiteral("진화의 오브")},
        {QStringLiteral("perfect-chaos-orb"), QStringLiteral("완벽한 카오스 오브")},
        {QStringLiteral("perfect-exalted-orb"), QStringLiteral("완벽한 엑잘티드 오브")},
        {QStringLiteral("perfect-jewellers-orb"), QStringLiteral("완벽한 쥬얼러 오브")},
        {QStringLiteral("perfect-orb-of-augmentation"), QStringLiteral("완벽한 확장의 오브")},
        {QStringLiteral("perfect-orb-of-transmutation"), QStringLiteral("완벽한 진화의 오브")},
        {QStringLiteral("perfect-regal-orb"), QStringLiteral("완벽한 제왕의 오브")},
        {QStringLiteral("regal"), QStringLiteral("제왕의 오브")},
        {QStringLiteral("regal-shard"), QStringLiteral("제왕의 파편")},
        {QStringLiteral("wisdom"), QStringLiteral("감정 주문서")},
        {QStringLiteral("transmutation-shard"), QStringLiteral("진화의 파편")},
        {QStringLiteral("vaal-arcanists-infuser"), QStringLiteral("바알 신비학자의 주입기")},
        {QStringLiteral("vaal-armourers-infuser"), QStringLiteral("바알 방어구 장인의 주입기")},
        {QStringLiteral("vaal-blacksmiths-infuser"), QStringLiteral("바알 대장장이의 주입기")},
        {QStringLiteral("vaal-catalysing-infuser"), QStringLiteral("바알 촉진시키는 주입기")},
        {QStringLiteral("vaal-cultivation-orb"), QStringLiteral("바알 함양 오브")},
        {QStringLiteral("vaal"), QStringLiteral("바알 오브")},
        {QStringLiteral("vaal-siphoner"), QStringLiteral("바알 착취기")},
    };
    return map;
}

} // namespace

QString localizedCurrencyName(const QString& currencyId, const QString& englishFallback) {
    const auto it = koreanNameById().constFind(currencyId);
    if (it != koreanNameById().constEnd()) {
        return *it;
    }
    return englishFallback;
}

void localizeCurrencyRates(QMap<QString, CurrencyRate>& ratesById) {
    for (auto it = ratesById.begin(); it != ratesById.end(); ++it) {
        CurrencyRate& rate = it.value();
        if (rate.categoryId != QStringLiteral("currency")) {
            continue;
        }
        const QString apiId = rate.apiItemId.isEmpty()
                                  ? economyApiItemIdFromCompositeId(rate.id)
                                  : rate.apiItemId;
        rate.name = localizedCurrencyName(apiId, rate.name);
    }
}
