#pragma once

#include "core/poeninja/PoeNinjaTypes.h"

#include <QDialog>
#include <QString>

class CurrencyIconCache;

class CurrencyPickerDialog : public QDialog {
    Q_OBJECT
public:
    CurrencyPickerDialog(const QList<CurrencyRate>& rates,
                         CurrencyIconCache* icons,
                         QWidget* parent = nullptr);

    QString selectedCurrencyId() const;
    QString selectedCurrencyName() const;

private:
    void onFilterChanged(const QString& text);
    void onIconUpdated(const QString& currencyId);
    void onListContextMenu(const QPoint& pos);
    void populateCategoryCombo();
    void rebuildList(const QString& filter);
    void acceptItem(QListWidgetItem* item);
    QListWidgetItem* addRateItem(const CurrencyRate& rate);
    const CurrencyRate* findRate(const QString& compositeId) const;
    bool rateMatchesFilter(const CurrencyRate& rate, const QString& needle) const;
    void toggleFavoriteForItem(QListWidgetItem* item);

    QList<CurrencyRate> m_rates;
    CurrencyIconCache* m_icons = nullptr;
    class QListWidget* m_list = nullptr;
    class QComboBox* m_categoryCombo = nullptr;
    class QLineEdit* m_filterEdit = nullptr;
    QString m_selectedId;
    QString m_selectedName;
};
