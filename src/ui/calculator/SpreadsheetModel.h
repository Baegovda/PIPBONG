#pragma once

#include "core/poeninja/PoeNinjaTypes.h"
#include "ui/calculator/FormulaEvaluator.h"
#include "ui/calculator/SpreadsheetBorders.h"

#include <QColor>

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
    Text,
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

    static constexpr int kDefaultDecimalPlaces = 4;
    static constexpr int kMinDecimalPlaces = 0;
    static constexpr int kMaxDecimalPlaces = 8;

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

    QString effectiveBaseCurrencyId(int row, int col) const;
    QString effectiveBaseCurrencyName(int row, int col) const;
    bool hasCellBaseCurrencyOverride(int row, int col) const;
    void applyCellBaseCurrency(int minRow, int minCol, int maxRow, int maxCol, const QString& currencyId);
    void clearCellBaseCurrency(int minRow, int minCol, int maxRow, int maxCol);

    void setCellInput(int row, int col, SpreadsheetCell cell);
    SpreadsheetCell cellInput(int row, int col) const;
    SpreadsheetCellState cellState(int row, int col) const;

    bool bindCurrencyToCell(int row, int col, const QString& currencyId, const QString& currencyName);
    QList<CurrencyRate> availableCurrencies() const;

    int decimalPlaces() const;
    void setDecimalPlaces(int places);
    QString formatNumber(double value) const;

    CellBorderMask cellBorders(int row, int col) const;
    void applyBorderPreset(int minRow, int minCol, int maxRow, int maxCol, SpreadsheetBorderPreset preset);

    std::optional<QColor> cellBackgroundColor(int row, int col) const;
    std::optional<QColor> cellForegroundColor(int row, int col) const;
    void applyBackgroundColor(int minRow, int minCol, int maxRow, int maxCol, const QColor& color);
    void applyForegroundColor(int minRow, int minCol, int maxRow, int maxCol, const QColor& color);
    void clearCellBackgroundColors(int minRow, int minCol, int maxRow, int maxCol);
    void clearCellForegroundColors(int minRow, int minCol, int maxRow, int maxCol);
    void clearCellColors(int minRow, int minCol, int maxRow, int maxCol);

    bool moveCellRange(int srcMinRow,
                       int srcMinCol,
                       int srcMaxRow,
                       int srcMaxCol,
                       int dstMinRow,
                       int dstMinCol);

    bool insertRow(int atRow);
    bool insertColumn(int atCol);
    bool deleteRows(int minRow, int maxRow);
    bool deleteColumns(int minCol, int maxCol);

    FormulaResult evaluateFormulaAtCell(const QString& expression, int evalRow, int evalCol);

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& document);

signals:
    void sheetModified();
    void baseCurrencyChanged();
    void decimalPlacesChanged();

private:
    using CellKey = QPair<int, int>;

    CellKey makeKey(int row, int col) const;
    void ensureDimensions(int row, int col);
    void rebuildCellStates();
    void refreshCellStates();
    void recalculateAll();
    void recalculateCell(int row, int col, std::unordered_set<std::string>& visiting);
    std::optional<double> resolveNumericValue(int row, int col, std::unordered_set<std::string>& visiting);
    std::optional<double> resolveNumericValueInBase(int row,
                                                    int col,
                                                    std::unordered_set<std::string>& visiting,
                                                    const QString& targetBaseId);
    QString errorText(FormulaError error) const;
    std::optional<double> rateValueInBase(const QString& currencyId,
                                          const QString& baseCurrencyId = QString()) const;
    std::optional<double> convertValueBetweenBases(double value,
                                                   const QString& fromBaseId,
                                                   const QString& toBaseId) const;
    QString baseCurrencyNameForId(const QString& currencyId) const;
    void setCellBorderMask(int row, int col, CellBorderMask mask);
    void addCellBorder(int row, int col, CellBorder edge);
    void removeCellBorder(int row, int col, CellBorder edge);
    void emitBorderRangeChanged(int minRow, int minCol, int maxRow, int maxCol);
    void emitColorRangeChanged(int minRow, int minCol, int maxRow, int maxCol);
    static QString adjustFormulaForMove(const QString& raw, int srcMinRow, int srcMinCol, int srcMaxRow, int srcMaxCol, int deltaRow, int deltaCol);
    static QString adjustFormulaForRowInsert(const QString& raw, int insertRow);
    static QString adjustFormulaForColumnInsert(const QString& raw, int insertCol);
    static QString adjustFormulaForRowDelete(const QString& raw, int deleteMinRow, int deleteMaxRow);
    static QString adjustFormulaForColumnDelete(const QString& raw, int deleteMinCol, int deleteMaxCol);
    void shiftAllMapsForRowInsert(int atRow);
    void shiftAllMapsForColumnInsert(int atCol);
    void shiftAllMapsForRowDelete(int minRow, int maxRow);
    void shiftAllMapsForColumnDelete(int minCol, int maxCol);
    void adjustAllFormulasForRowInsert(int insertRow);
    void adjustAllFormulasForColumnInsert(int insertCol);
    void adjustAllFormulasForRowDelete(int minRow, int maxRow);
    void adjustAllFormulasForColumnDelete(int minCol, int maxCol);
    static void clearCellMapsAt(QMap<CellKey, SpreadsheetCell>& inputs,
                                QMap<CellKey, CellBorderMask>& borders,
                                QMap<CellKey, QColor>& backgroundColors,
                                QMap<CellKey, QColor>& foregroundColors,
                                QMap<CellKey, QString>& cellBaseCurrencyIds,
                                int row,
                                int col);

    int m_rowCount = kDefaultRowCount;
    int m_columnCount = kDefaultColumnCount;
    QMap<CellKey, SpreadsheetCell> m_inputs;
    QMap<CellKey, CellBorderMask> m_borders;
    QMap<CellKey, QColor> m_backgroundColors;
    QMap<CellKey, QColor> m_foregroundColors;
    QMap<CellKey, QString> m_cellBaseCurrencyIds;
    QMap<CellKey, SpreadsheetCellState> m_states;
    std::optional<EconomySnapshot> m_snapshot;
    QString m_baseCurrencyId = QStringLiteral("currency:exalted");
    int m_decimalPlaces = kDefaultDecimalPlaces;
};
