#pragma once

#include <QDialog>
#include <QStringList>
#include <QVector>

class QComboBox;
class QLineEdit;
class QPushButton;
class QTextBrowser;
class QVBoxLayout;
class QWidget;
class SpreadsheetModel;

enum class FormulaPickSlot {
    None,
    Target,
    Operand,
    ArrayRange
};

enum class FormulaApplyMode {
    SingleCell,
    ArrayFill
};

class FormulaBuilderDialog : public QDialog {
    Q_OBJECT
public:
    explicit FormulaBuilderDialog(SpreadsheetModel* model, QWidget* parent = nullptr);

    void setTargetCell(int row, int col);
    int targetRow() const { return m_targetRow; }
    int targetCol() const { return m_targetCol; }

    FormulaPickSlot activePickSlot() const { return m_activePickSlot; }
    int activeOperandIndex() const { return m_activeOperandIndex; }
    QChar currentOperator() const;

public slots:
    void applyPickedCellRefs(const QStringList& cellRefs);
    void endPickMode();

signals:
    void pickModeRequested(FormulaPickSlot slot);
    void pickModeEnded();
    void formulaApplied(int row, int col, const QString& formula);

private slots:
    void onSegmentOperatorClicked();
    void onOperandEdited();
    void onFormulaPreviewEdited();
    void onParenToggled();
    void onApplyModeChanged(int index);
    void onPickTargetClicked();
    void onPickOperandClicked();
    void onPickArrayRangeClicked();
    void onAddOperandClicked();
    void onRemoveOperandClicked();
    void onApplyClicked();
    void onManualToggled(bool checked);

private:
    struct OperandRow {
        QWidget* rowWidget = nullptr;
        class QLabel* label = nullptr;
        QLineEdit* edit = nullptr;
        QPushButton* pickButton = nullptr;
        QPushButton* parenButton = nullptr;
        QPushButton* deleteButton = nullptr;
        bool wrapInParens = false;
    };

    struct OperatorSegment {
        QWidget* segmentWidget = nullptr;
        QPushButton* addButton = nullptr;
        QPushButton* subButton = nullptr;
        QPushButton* mulButton = nullptr;
        QPushButton* divButton = nullptr;
        QChar op = QLatin1Char('+');
    };

    void rebuildFormulaPreview();
    void updatePreviewResult();
    void updatePickButtonStyles();
    void updateApplyModeUi();
    void updateOperandDeleteButtons();
    void requestPick(FormulaPickSlot slot);
    QString wrapOperand(const QString& text) const;
    QString formatOperandTerm(int index) const;
    static QString joinCellRefs(const QStringList& refs, QChar op);
    void setArrayRange(int minRow, int minCol, int maxRow, int maxCol);
    bool arrayRangeValid() const;
    int arrayCellCount() const;
    int operandIndexForWidget(const QWidget* widget) const;
    int segmentIndexForWidget(const QWidget* widget) const;
    QChar operatorForOperandPick(int operandIndex) const;
    void addOperandRow(const QString& initialText = QString());
    void removeOperandRow(int index);
    void addOperatorSegment(QChar initialOp = QLatin1Char('+'));
    void removeOperatorSegment(int index);
    void rebuildOperandLayout();
    void renumberOperandLabels();
    void setSegmentOperator(int segmentIndex, QChar op);

    SpreadsheetModel* m_model = nullptr;
    int m_targetRow = 0;
    int m_targetCol = 0;
    int m_rangeMinRow = -1;
    int m_rangeMinCol = -1;
    int m_rangeMaxRow = -1;
    int m_rangeMaxCol = -1;
    int m_activeOperandIndex = -1;
    FormulaPickSlot m_activePickSlot = FormulaPickSlot::None;
    FormulaApplyMode m_applyMode = FormulaApplyMode::SingleCell;

    QTextBrowser* m_manualBrowser = nullptr;
    QPushButton* m_manualToggle = nullptr;
    QComboBox* m_applyModeCombo = nullptr;
    QLineEdit* m_targetCellEdit = nullptr;
    QLineEdit* m_arrayRangeEdit = nullptr;
    QLineEdit* m_formulaPreview = nullptr;
    QPushButton* m_pickTargetButton = nullptr;
    QPushButton* m_pickArrayRangeButton = nullptr;
    QPushButton* m_addOperandButton = nullptr;
    QPushButton* m_applyButton = nullptr;
    class QLabel* m_targetLabel = nullptr;
    class QLabel* m_arrayRangeLabel = nullptr;
    class QLabel* m_resultPreviewLabel = nullptr;
    QWidget* m_arrayRangeRow = nullptr;
    QWidget* m_operandListHost = nullptr;
    QVBoxLayout* m_operandRowsLayout = nullptr;
    QVector<OperandRow> m_operandRows;
    QVector<OperatorSegment> m_operatorSegments;

    static constexpr int kMinOperandRows = 1;
};
