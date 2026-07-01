#include "ui/calculator/FormulaEvaluator.h"

#include <QChar>
#include <QRegularExpression>

#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class Parser {
public:
    Parser(std::string input, const FormulaEvaluator::CellValueResolver& resolveCell, std::unordered_set<std::string>* visiting)
        : m_input(std::move(input))
        , m_resolveCell(resolveCell)
        , m_visiting(visiting) {}

    double parseExpression() {
        double value = parseTerm();
        while (true) {
            skipWhitespace();
            if (match('+')) {
                value += parseTerm();
            } else if (match('-')) {
                value -= parseTerm();
            } else {
                break;
            }
        }
        return value;
    }

private:
    double parseTerm() {
        double value = parseFactor();
        while (true) {
            skipWhitespace();
            if (match('*')) {
                value *= parseFactor();
            } else if (match('/')) {
                const double rhs = parseFactor();
                if (std::abs(rhs) < 1e-15) {
                    throw FormulaError::DivZero;
                }
                value /= rhs;
            } else {
                break;
            }
        }
        return value;
    }

    double parseFactor() {
        skipWhitespace();
        if (match('+')) {
            return parseFactor();
        }
        if (match('-')) {
            return -parseFactor();
        }
        if (match('(')) {
            const double value = parseExpression();
            skipWhitespace();
            if (!match(')')) {
                throw FormulaError::Parse;
            }
            return value;
        }
        if (peek() && (std::isdigit(static_cast<unsigned char>(*peek())) || *peek() == '.')) {
            return parseNumber();
        }
        if (peek() && std::isalpha(static_cast<unsigned char>(*peek()))) {
            return parseCellReference();
        }
        throw FormulaError::Parse;
    }

    double parseNumber() {
        skipWhitespace();
        size_t start = m_pos;
        while (m_pos < m_input.size()
               && (std::isdigit(static_cast<unsigned char>(m_input[m_pos])) || m_input[m_pos] == '.')) {
            ++m_pos;
        }
        if (start == m_pos) {
            throw FormulaError::Parse;
        }
        return std::stod(m_input.substr(start, m_pos - start));
    }

    double parseCellReference() {
        const size_t start = m_pos;
        while (m_pos < m_input.size() && std::isalpha(static_cast<unsigned char>(m_input[m_pos]))) {
            ++m_pos;
        }
        const size_t colEnd = m_pos;
        while (m_pos < m_input.size() && std::isdigit(static_cast<unsigned char>(m_input[m_pos]))) {
            ++m_pos;
        }
        if (colEnd == start || m_pos == colEnd) {
            throw FormulaError::Parse;
        }

        const QString reference = QString::fromStdString(m_input.substr(start, m_pos - start)).toUpper();
        int row = 0;
        int col = 0;
        if (!FormulaEvaluator::cellAddressFromReference(reference, row, col)) {
            throw FormulaError::Ref;
        }

        const std::string key = reference.toStdString();
        if (m_visiting && m_visiting->count(key) > 0) {
            throw FormulaError::Ref;
        }
        std::unordered_set<std::string> localVisiting;
        std::unordered_set<std::string>* activeVisiting = m_visiting ? m_visiting : &localVisiting;
        activeVisiting->insert(key);

        const std::optional<double> resolved = m_resolveCell(row, col);
        activeVisiting->erase(key);
        if (!resolved.has_value()) {
            throw FormulaError::Ref;
        }
        return *resolved;
    }

    void skipWhitespace() {
        while (m_pos < m_input.size() && std::isspace(static_cast<unsigned char>(m_input[m_pos]))) {
            ++m_pos;
        }
    }

    bool match(char expected) {
        skipWhitespace();
        if (m_pos < m_input.size() && m_input[m_pos] == expected) {
            ++m_pos;
            return true;
        }
        return false;
    }

    const char* peek() const {
        if (m_pos >= m_input.size()) {
            return nullptr;
        }
        return &m_input[m_pos];
    }

    std::string m_input;
    size_t m_pos = 0;
    const FormulaEvaluator::CellValueResolver& m_resolveCell;
    std::unordered_set<std::string>* m_visiting = nullptr;
};

} // namespace

bool FormulaEvaluator::cellAddressFromReference(const QString& reference, int& row, int& col) {
    static const QRegularExpression pattern(QStringLiteral("^([A-Z]+)([0-9]+)$"));
    const QRegularExpressionMatch match = pattern.match(reference.trimmed().toUpper());
    if (!match.hasMatch()) {
        return false;
    }

    const QString colLetters = match.captured(1);
    bool ok = false;
    const int rowNumber = match.captured(2).toInt(&ok);
    if (!ok || rowNumber <= 0) {
        return false;
    }

    int column = 0;
    for (const QChar ch : colLetters) {
        column = column * 26 + (ch.unicode() - QLatin1Char('A').unicode() + 1);
    }
    col = column - 1;
    row = rowNumber - 1;
    return col >= 0 && row >= 0;
}

QString FormulaEvaluator::cellReference(int row, int col) {
    if (row < 0 || col < 0) {
        return {};
    }

    int columnNumber = col + 1;
    QString letters;
    while (columnNumber > 0) {
        const int remainder = (columnNumber - 1) % 26;
        letters.prepend(QChar(QLatin1Char('A').unicode() + remainder));
        columnNumber = (columnNumber - 1) / 26;
    }
    return letters + QString::number(row + 1);
}

FormulaResult FormulaEvaluator::evaluate(const QString& expression,
                                         const CellValueResolver& resolveCell,
                                         std::unordered_set<std::string>* visiting) {
    FormulaResult result;
    QString trimmed = expression.trimmed();
    if (trimmed.startsWith(QLatin1Char('='))) {
        trimmed = trimmed.mid(1).trimmed();
    }
    if (trimmed.isEmpty()) {
        result.error = FormulaError::Parse;
        return result;
    }

    try {
        Parser parser(trimmed.toStdString(), resolveCell, visiting);
        result.value = parser.parseExpression();
        result.ok = true;
        result.error = FormulaError::None;
    } catch (FormulaError error) {
        result.error = error;
    } catch (...) {
        result.error = FormulaError::Parse;
    }
    return result;
}
