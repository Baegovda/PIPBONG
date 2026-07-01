#include "ui/calculator/FormulaBuilderDialog.h"

#include "ui/calculator/FormulaEvaluator.h"
#include "ui/calculator/SpreadsheetModel.h"
#include "ui/UiStrings.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {

QString manualHtml() {
    return QStringLiteral(
        "<h3>수식 · 사칙연산 안내</h3>"
        "<p>이 계산기는 엑셀처럼 셀에 숫자, 시세 연동, 수식을 넣을 수 있습니다.</p>"
        "<table cellpadding='4'>"
        "<tr><td><b>+</b></td><td>더하기</td><td><code>=A1+B1</code></td></tr>"
        "<tr><td><b>−</b></td><td>빼기</td><td><code>=A1-B1</code></td></tr>"
        "<tr><td><b>×</b></td><td>곱하기</td><td><code>=A1*B1</code></td></tr>"
        "<tr><td><b>÷</b></td><td>나누기</td><td><code>=A1/B1</code></td></tr>"
        "</table>"
        "<p><b>셀 주소</b>: 열 문자 + 행 번호 (예: <code>A1</code>, <code>B3</code>)</p>"
        "<p><b>괄호</b>: <code>=(A1+B1)/C1</code> 처럼 우선순위를 바꿀 수 있습니다.</p>"
        "<p><b>표에서 고르기</b>: 아래 <i>표에서 고르기</i> 버튼을 누른 뒤 표에서 셀을 클릭하거나 "
        "드래그해 여러 셀을 선택하세요. 선택한 셀은 현재 연산자로 자동 연결됩니다.</p>"
        "<p><b>직접 입력</b>: 값 칸에 숫자(<code>100</code>)나 셀 주소를 타이핑해도 됩니다.</p>"
        "<p>시세 연동 셀도 수식에 사용할 수 있으며, 기준 화폐로 환산된 값이 계산에 쓰입니다.</p>");
}

QPushButton* makeOperatorButton(const QString& label, QWidget* parent) {
    auto* button = new QPushButton(label, parent);
    button->setCheckable(true);
    button->setFixedSize(44, 32);
    return button;
}

} // namespace

FormulaBuilderDialog::FormulaBuilderDialog(SpreadsheetModel* model, QWidget* parent)
    : QDialog(parent)
    , m_model(model) {
    setWindowTitle(tr("수식 만들기"));
    setMinimumWidth(480);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(8);

    m_manualToggle = new QPushButton(tr("▶ 수식 사용법 보기"), this);
    m_manualToggle->setCheckable(true);
    m_manualToggle->setFlat(true);
    m_manualToggle->setStyleSheet(QStringLiteral("text-align: left;"));
    rootLayout->addWidget(m_manualToggle);

    m_manualBrowser = new QTextBrowser(this);
    m_manualBrowser->setHtml(manualHtml());
    m_manualBrowser->setOpenExternalLinks(false);
    m_manualBrowser->setVisible(false);
    m_manualBrowser->setMaximumHeight(220);
    rootLayout->addWidget(m_manualBrowser);

    auto* builderGroup = new QGroupBox(tr("수식 구성"), this);
    auto* builderLayout = new QGridLayout(builderGroup);
    builderLayout->setHorizontalSpacing(8);
    builderLayout->setVerticalSpacing(6);

    m_targetCellEdit = new QLineEdit(this);
    m_targetCellEdit->setReadOnly(true);
    m_pickTargetButton = new QPushButton(tr("표에서 고르기"), this);

    auto* targetRow = new QHBoxLayout();
    targetRow->addWidget(m_targetCellEdit, 1);
    targetRow->addWidget(m_pickTargetButton);

    builderLayout->addWidget(new QLabel(tr("결과 셀"), this), 0, 0);
    builderLayout->addLayout(targetRow, 0, 1);

    auto* opRow = new QHBoxLayout();
    m_opAddButton = makeOperatorButton(QStringLiteral("+"), this);
    m_opSubButton = makeOperatorButton(QStringLiteral("−"), this);
    m_opMulButton = makeOperatorButton(QStringLiteral("×"), this);
    m_opDivButton = makeOperatorButton(QStringLiteral("÷"), this);
    m_opAddButton->setChecked(true);
    opRow->addWidget(m_opAddButton);
    opRow->addWidget(m_opSubButton);
    opRow->addWidget(m_opMulButton);
    opRow->addWidget(m_opDivButton);
    opRow->addStretch(1);

    builderLayout->addWidget(new QLabel(tr("연산"), this), 1, 0);
    builderLayout->addLayout(opRow, 1, 1);

    m_operandLeft = new QLineEdit(this);
    m_operandLeft->setPlaceholderText(tr("첫 번째 값 (셀 또는 숫자)"));
    m_pickLeftButton = new QPushButton(tr("표에서 고르기"), this);
    auto* leftRow = new QHBoxLayout();
    leftRow->addWidget(m_operandLeft, 1);
    leftRow->addWidget(m_pickLeftButton);

    builderLayout->addWidget(new QLabel(tr("값 1"), this), 2, 0);
    builderLayout->addLayout(leftRow, 2, 1);

    m_operandRight = new QLineEdit(this);
    m_operandRight->setPlaceholderText(tr("두 번째 값 (셀 또는 숫자)"));
    m_pickRightButton = new QPushButton(tr("표에서 고르기"), this);
    auto* rightRow = new QHBoxLayout();
    rightRow->addWidget(m_operandRight, 1);
    rightRow->addWidget(m_pickRightButton);

    builderLayout->addWidget(new QLabel(tr("값 2"), this), 3, 0);
    builderLayout->addLayout(rightRow, 3, 1);

    m_formulaPreview = new QLineEdit(this);
    m_formulaPreview->setReadOnly(true);
    m_formulaPreview->setPlaceholderText(tr("=A1+B1"));
    builderLayout->addWidget(new QLabel(tr("수식"), this), 4, 0);
    builderLayout->addWidget(m_formulaPreview, 4, 1);

    m_resultPreviewLabel = new QLabel(this);
    m_resultPreviewLabel->setWordWrap(true);
    builderLayout->addWidget(m_resultPreviewLabel, 5, 0, 1, 2);

    rootLayout->addWidget(builderGroup);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close, this);
    localizeDialogButtons(buttons);
    m_applyButton = buttons->button(QDialogButtonBox::Apply);
    if (m_applyButton) {
        m_applyButton->setText(tr("셀에 적용"));
    }
    rootLayout->addWidget(buttons);

    connect(m_manualToggle, &QPushButton::toggled, this, &FormulaBuilderDialog::onManualToggled);
    connect(m_opAddButton, &QPushButton::clicked, this, &FormulaBuilderDialog::onOperatorClicked);
    connect(m_opSubButton, &QPushButton::clicked, this, &FormulaBuilderDialog::onOperatorClicked);
    connect(m_opMulButton, &QPushButton::clicked, this, &FormulaBuilderDialog::onOperatorClicked);
    connect(m_opDivButton, &QPushButton::clicked, this, &FormulaBuilderDialog::onOperatorClicked);
    connect(m_operandLeft, &QLineEdit::textChanged, this, &FormulaBuilderDialog::onOperandEdited);
    connect(m_operandRight, &QLineEdit::textChanged, this, &FormulaBuilderDialog::onOperandEdited);
    connect(m_pickTargetButton, &QPushButton::clicked, this, &FormulaBuilderDialog::onPickTargetClicked);
    connect(m_pickLeftButton, &QPushButton::clicked, this, &FormulaBuilderDialog::onPickLeftClicked);
    connect(m_pickRightButton, &QPushButton::clicked, this, &FormulaBuilderDialog::onPickRightClicked);
    if (m_applyButton) {
        connect(m_applyButton, &QPushButton::clicked, this, &FormulaBuilderDialog::onApplyClicked);
    }
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (m_model) {
        connect(m_model, &SpreadsheetModel::decimalPlacesChanged, this,
                &FormulaBuilderDialog::updatePreviewResult);
    }

    setTargetCell(0, 0);
    rebuildFormulaPreview();
}

void FormulaBuilderDialog::setTargetCell(int row, int col) {
    m_targetRow = row;
    m_targetCol = col;
    if (m_targetCellEdit) {
        m_targetCellEdit->setText(FormulaEvaluator::cellReference(row, col));
    }
}

QString FormulaBuilderDialog::joinCellRefs(const QStringList& refs, QChar op) {
    if (refs.isEmpty()) {
        return {};
    }
    if (refs.size() == 1) {
        return refs.front();
    }

    const QChar mathOp = op == QLatin1Char('×') ? QLatin1Char('*')
                        : op == QLatin1Char('÷') ? QLatin1Char('/')
                        : op == QLatin1Char('−') ? QLatin1Char('-')
                                                 : op;
    QString joined = refs.front();
    for (int i = 1; i < refs.size(); ++i) {
        joined += mathOp;
        joined += refs.at(i);
    }
    return QStringLiteral("(%1)").arg(joined);
}

void FormulaBuilderDialog::applyPickedCellRefs(const QStringList& cellRefs) {
    if (cellRefs.isEmpty()) {
        return;
    }

    if (m_activePickSlot == FormulaPickSlot::Target) {
        int row = 0;
        int col = 0;
        if (FormulaEvaluator::cellAddressFromReference(cellRefs.front(), row, col)) {
            setTargetCell(row, col);
        }
        endPickMode();
        return;
    }

    const QString expression = joinCellRefs(cellRefs, m_currentOperator);
    if (m_activePickSlot == FormulaPickSlot::LeftOperand && m_operandLeft) {
        m_operandLeft->setText(expression);
    } else if (m_activePickSlot == FormulaPickSlot::RightOperand && m_operandRight) {
        m_operandRight->setText(expression);
    }
    endPickMode();
}

void FormulaBuilderDialog::endPickMode() {
    if (m_activePickSlot == FormulaPickSlot::None) {
        return;
    }
    m_activePickSlot = FormulaPickSlot::None;
    updatePickButtonStyles();
    emit pickModeEnded();
}

QString FormulaBuilderDialog::wrapOperand(const QString& text) const {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return trimmed;
    }
    if (trimmed.startsWith(QLatin1Char('(')) && trimmed.endsWith(QLatin1Char(')'))) {
        return trimmed;
    }
    if (trimmed.contains(QLatin1Char('+')) || trimmed.contains(QLatin1Char('-'))
        || trimmed.contains(QLatin1Char('*')) || trimmed.contains(QLatin1Char('/'))) {
        return QStringLiteral("(%1)").arg(trimmed);
    }
    return trimmed;
}

void FormulaBuilderDialog::rebuildFormulaPreview() {
    if (!m_formulaPreview) {
        return;
    }

    const QString left = m_operandLeft ? m_operandLeft->text().trimmed() : QString();
    const QString right = m_operandRight ? m_operandRight->text().trimmed() : QString();
    if (left.isEmpty() || right.isEmpty() || m_currentOperator.isNull()) {
        m_formulaPreview->clear();
        if (m_applyButton) {
            m_applyButton->setEnabled(false);
        }
        if (m_resultPreviewLabel) {
            m_resultPreviewLabel->clear();
        }
        return;
    }

    const QChar mathOp = m_currentOperator == QLatin1Char('×') ? QLatin1Char('*')
                        : m_currentOperator == QLatin1Char('÷') ? QLatin1Char('/')
                        : m_currentOperator == QLatin1Char('−') ? QLatin1Char('-')
                                                                 : m_currentOperator;
    const QString formula =
        QStringLiteral("=%1%2%3").arg(wrapOperand(left), mathOp, wrapOperand(right));
    m_formulaPreview->setText(formula);
    if (m_applyButton) {
        m_applyButton->setEnabled(true);
    }
    updatePreviewResult();
}

void FormulaBuilderDialog::updatePreviewResult() {
    if (!m_resultPreviewLabel || !m_model || !m_formulaPreview) {
        return;
    }

    const QString formula = m_formulaPreview->text().trimmed();
    if (formula.isEmpty()) {
        m_resultPreviewLabel->clear();
        return;
    }

    const auto resolver = [this](int row, int col) -> std::optional<double> {
        const SpreadsheetCellState state = m_model->cellState(row, col);
        if (state.hasError || !state.numericValue.has_value()) {
            return std::nullopt;
        }
        return state.numericValue;
    };

    const FormulaResult result = FormulaEvaluator::evaluate(formula, resolver);
    if (result.ok) {
        m_resultPreviewLabel->setText(
            tr("미리보기 결과: %1").arg(m_model->formatNumber(result.value)));
    } else {
        m_resultPreviewLabel->setText(tr("미리보기: 수식을 확인하세요 (빈 셀·오류 참조)."));
    }
}

void FormulaBuilderDialog::updatePickButtonStyles() {
    const auto styleIdle = QStringLiteral("");
    const auto styleActive = QStringLiteral("font-weight: bold; border: 2px solid palette(highlight);");

    auto apply = [&](QPushButton* button, FormulaPickSlot slot) {
        if (!button) {
            return;
        }
        button->setStyleSheet(m_activePickSlot == slot ? styleActive : styleIdle);
    };

    apply(m_pickTargetButton, FormulaPickSlot::Target);
    apply(m_pickLeftButton, FormulaPickSlot::LeftOperand);
    apply(m_pickRightButton, FormulaPickSlot::RightOperand);
}

void FormulaBuilderDialog::requestPick(FormulaPickSlot slot) {
    if (m_activePickSlot == slot) {
        endPickMode();
        return;
    }
    m_activePickSlot = slot;
    updatePickButtonStyles();
    emit pickModeRequested(slot);
}

void FormulaBuilderDialog::onOperatorClicked() {
    auto* button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }

    for (QPushButton* op : {m_opAddButton, m_opSubButton, m_opMulButton, m_opDivButton}) {
        if (op) {
            op->setChecked(op == button);
        }
    }

    const QString label = button->text();
    if (label == QStringLiteral("+")) {
        m_currentOperator = QLatin1Char('+');
    } else if (label == QStringLiteral("−")) {
        m_currentOperator = QLatin1Char('−');
    } else if (label == QStringLiteral("×")) {
        m_currentOperator = QLatin1Char('×');
    } else if (label == QStringLiteral("÷")) {
        m_currentOperator = QLatin1Char('÷');
    }
    rebuildFormulaPreview();
}

void FormulaBuilderDialog::onOperandEdited() {
    rebuildFormulaPreview();
}

void FormulaBuilderDialog::onPickTargetClicked() {
    requestPick(FormulaPickSlot::Target);
}

void FormulaBuilderDialog::onPickLeftClicked() {
    requestPick(FormulaPickSlot::LeftOperand);
}

void FormulaBuilderDialog::onPickRightClicked() {
    requestPick(FormulaPickSlot::RightOperand);
}

void FormulaBuilderDialog::onApplyClicked() {
    const QString formula = m_formulaPreview ? m_formulaPreview->text().trimmed() : QString();
    if (formula.isEmpty()) {
        return;
    }
    emit formulaApplied(m_targetRow, m_targetCol, formula);
}

void FormulaBuilderDialog::onManualToggled(bool checked) {
    if (m_manualBrowser) {
        m_manualBrowser->setVisible(checked);
    }
    if (m_manualToggle) {
        m_manualToggle->setText(checked ? tr("▼ 수식 사용법 숨기기") : tr("▶ 수식 사용법 보기"));
    }
}
