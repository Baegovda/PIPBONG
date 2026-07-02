#pragma once

#include <QString>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>

enum class FormulaError {
    None,
    Ref,
    DivZero,
    Parse
};

struct FormulaResult {
    bool ok = false;
    double value = 0.0;
    FormulaError error = FormulaError::None;
};

class FormulaEvaluator {
public:
    using CellValueResolver = std::function<std::optional<double>(int row, int col)>;

    static bool cellAddressFromReference(const QString& reference, int& row, int& col);
    static QString cellReference(int row, int col);

    static FormulaResult evaluate(const QString& expression,
                                  const CellValueResolver& resolveCell,
                                  std::unordered_set<std::string>* visiting = nullptr);

    static QString shiftFormulaReferences(const QString& formula, int deltaRow, int deltaCol);
    static QString shiftFormulaReferencesAtOrAfter(const QString& formula,
                                                   int minRow,
                                                   int minCol,
                                                   int deltaRow,
                                                   int deltaCol);
    static QString shiftFormulaReferencesInRegion(const QString& formula,
                                                    int regionMinRow,
                                                    int regionMinCol,
                                                    int regionMaxRow,
                                                    int regionMaxCol,
                                                    int deltaRow,
                                                    int deltaCol);
    static QString formatCellRange(int minRow, int minCol, int maxRow, int maxCol);
};
