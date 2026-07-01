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
    void populateCategoryCombo();
    void rebuildList(const QString& filter);

    QList<CurrencyRate> m_rates;
    CurrencyIconCache* m_icons = nullptr;
    class QListWidget* m_list = nullptr;
    class QComboBox* m_categoryCombo = nullptr;
    QString m_selectedId;
    QString m_selectedName;
};
