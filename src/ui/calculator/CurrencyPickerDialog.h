#pragma once

#include "core/poeninja/PoeNinjaTypes.h"

#include <QDialog>
#include <QListWidget>
#include <QString>

class CurrencyIconCache;
class QComboBox;
class QLineEdit;

class CurrencyPickerDialog : public QDialog {
    Q_OBJECT
public:
    CurrencyPickerDialog(const QList<CurrencyRate>& rates,
                         CurrencyIconCache* icons,
                         QWidget* parent = nullptr,
                         const QString& dialogTitle = QString(),
                         const QString& baseCurrencyId = QString(),
                         int decimalPlaces = 4);

    QString selectedCurrencyId() const;
    QString selectedCurrencyName() const;

private:
    void onFilterChanged(const QString& text);
    void onIconUpdated(const QString& currencyId);
    void onListContextMenu(const QPoint& pos);
    void onFavoritesContextMenu(const QPoint& pos);
    void showContextMenuForList(QListWidget* sourceList, const QPoint& pos);
    void populateCategoryCombo();
    void rebuildFavoritesList(const QString& filter);
    void rebuildList(const QString& filter);
    void acceptItem(QListWidgetItem* item);
    QListWidgetItem* addRateItem(QListWidget* listWidget, const CurrencyRate& rate, bool showRate);
    const CurrencyRate* findRate(const QString& compositeId) const;
    bool rateMatchesFilter(const CurrencyRate& rate, const QString& needle) const;
    void toggleFavoriteForItem(QListWidgetItem* item);
    QString formatRateValue(const CurrencyRate& rate) const;
    void clearOtherListSelection(QListWidget* activeList);

    QList<CurrencyRate> m_rates;
    CurrencyIconCache* m_icons = nullptr;
    QListWidget* m_favoritesList = nullptr;
    QListWidget* m_list = nullptr;
    QComboBox* m_categoryCombo = nullptr;
    QLineEdit* m_filterEdit = nullptr;
    QString m_baseCurrencyId;
    int m_decimalPlaces = 4;
    QString m_selectedId;
    QString m_selectedName;
};
