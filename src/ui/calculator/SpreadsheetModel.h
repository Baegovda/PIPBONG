#pragma once

#include "core/poeninja/PoeNinjaTypes.h"
#include "ui/calculator/FormulaEvaluator.h"

#include <nlohmann/json.hpp>

#include <QAbstractTableModel>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVariant>
#include <optional>
#include <vector>

enum class SpreadsheetCellKind {
    Empty,
    Number,
    ApiRef,
    Formula
};

struct SpreadsheetCell {
    SpreadsheetCellKind kind = SpreadsheetCellKind::Empty;
    QString raw;
    QString currencyId;
    QString currencyName;
};

struct SpreadsheetCellState {
    SpreadsheetCell input;
    QString displayText;
    std::optional<double> numericValue;
    FormulaError error = FormulaError::None;
    bool hasError = false;
};

class SpreadsheetModel : public QAbstractTableModel {
    Q_OBJECT
public:
    static constexpr int kDefaultRowCount = 20;
    static constexpr int kDefaultColumnCount = 8;

    explicit SpreadsheetModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    void applyEconomySnapshot(const EconomySnapshot& snapshot);
    const EconomySnapshot* economySnapshot() const;

    QString baseCurrencyId() const;
    QString baseCurrencyName() const;
    void setBaseCurrencyId(const QString& currencyId);

    void setCellInput(int row, int col, SpreadsheetCell cell);
    SpreadsheetCell cellInput(int row, int col) const;
    SpreadsheetCellState cellState(int row, int col) const;

    bool bindCurrencyToCell(int row, int col, const QString& currencyId, const QString& currencyName);
    QList<CurrencyRate> availableCurrencies() const;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& document);

signals:
    void sheetModified();
    void baseCurrencyChanged();

private:
    using CellKey = QPair<int, int>;

    CellKey makeKey(int row, int col) const;
    void ensureDimensions(int row, int col);
    void recalculateAll();
    void recalculateCell(int row, int col, std::unordered_set<std::string>& visiting);
    std::optional<double> resolveNumericValue(int row, int col, std::unordered_set<std::string>& visiting);
    QString errorText(FormulaError error) const;
    static QString formatNumber(double value);
    std::optional<double> rateValueInBase(const QString& currencyId) const;

    int m_rowCount = kDefaultRowCount;
    int m_columnCount = kDefaultColumnCount;
    QMap<CellKey, SpreadsheetCell> m_inputs;
    QMap<CellKey, SpreadsheetCellState> m_states;
    std::optional<EconomySnapshot> m_snapshot;
    QString m_baseCurrencyId = QStringLiteral("currency:divine");
};
