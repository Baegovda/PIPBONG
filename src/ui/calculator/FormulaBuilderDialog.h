#pragma once

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QPushButton;
class QTextBrowser;
class SpreadsheetModel;

enum class FormulaPickSlot {
    None,
    Target,
    LeftOperand,
    RightOperand
};

class FormulaBuilderDialog : public QDialog {
    Q_OBJECT
public:
    explicit FormulaBuilderDialog(SpreadsheetModel* model, QWidget* parent = nullptr);

    void setTargetCell(int row, int col);
    int targetRow() const { return m_targetRow; }
    int targetCol() const { return m_targetCol; }

    FormulaPickSlot activePickSlot() const { return m_activePickSlot; }
    QChar currentOperator() const { return m_currentOperator; }

public slots:
    void applyPickedCellRefs(const QStringList& cellRefs);
    void endPickMode();

signals:
    void pickModeRequested(FormulaPickSlot slot);
    void pickModeEnded();
    void formulaApplied(int row, int col, const QString& formula);

private slots:
    void onOperatorClicked();
    void onOperandEdited();
    void onPickTargetClicked();
    void onPickLeftClicked();
    void onPickRightClicked();
    void onApplyClicked();
    void onManualToggled(bool checked);

private:
    void rebuildFormulaPreview();
    void updatePreviewResult();
    void updatePickButtonStyles();
    void requestPick(FormulaPickSlot slot);
    QString wrapOperand(const QString& text) const;
    static QString joinCellRefs(const QStringList& refs, QChar op);

    SpreadsheetModel* m_model = nullptr;
    int m_targetRow = 0;
    int m_targetCol = 0;
    QChar m_currentOperator = QLatin1Char('+');
    FormulaPickSlot m_activePickSlot = FormulaPickSlot::None;

    QTextBrowser* m_manualBrowser = nullptr;
    QPushButton* m_manualToggle = nullptr;
    QLineEdit* m_targetCellEdit = nullptr;
    QLineEdit* m_formulaPreview = nullptr;
    QLineEdit* m_operandLeft = nullptr;
    QLineEdit* m_operandRight = nullptr;
    QPushButton* m_pickTargetButton = nullptr;
    QPushButton* m_pickLeftButton = nullptr;
    QPushButton* m_pickRightButton = nullptr;
    QPushButton* m_opAddButton = nullptr;
    QPushButton* m_opSubButton = nullptr;
    QPushButton* m_opMulButton = nullptr;
    QPushButton* m_opDivButton = nullptr;
    QPushButton* m_applyButton = nullptr;
    class QLabel* m_resultPreviewLabel = nullptr;
};
