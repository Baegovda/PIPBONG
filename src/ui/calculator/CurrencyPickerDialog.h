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
                         const QString& dialogTitle = QString());

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
    QListWidget* m_list = nullptr;
    QComboBox* m_categoryCombo = nullptr;
    QLineEdit* m_filterEdit = nullptr;
    QString m_selectedId;
    QString m_selectedName;
};
