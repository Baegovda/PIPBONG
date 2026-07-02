#include "FormulaBuilderDialog.h"

#include "FormulaEvaluator.h"
#include "SpreadsheetModel.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <climits>

namespace {

QString formulaManualHtml() {
    return QStringLiteral(
        R"(<h3 style="margin-top:0;">수식 만들기 안내</h3>
<p>셀에 <b>계산식</b>을 넣습니다. 결과는 숫자로 표시되며, 참조 셀이 바뀌면 자동으로 다시 계산됩니다.</p>
<h4>기본 규칙</h4>
<ul>
<li>셀 주소는 <b>A1</b> 형식 (열 A~Z, 행 1부터).</li>
<li>값과 값 <b>사이</b>에서 <b>+ − × ÷</b> 를 골라 연결합니다.</li>
<li>각 값의 <b>( )</b> 버튼으로 그 항목을 괄호로 묶을 수 있습니다.</li>
<li><b>수식 미리보기</b> 칸에서 완성된 수식을 직접 수정할 수도 있습니다.</li>
</ul>
<h4>값 항목</h4>
<ul>
<li><b>값 추가</b> / <b>삭제</b>로 항목 개수를 늘리거나 줄일 수 있습니다.</li>
<li>항목마다 연산자를 다르게 지정할 수 있습니다. 예: <code>A1+B1*C1</code></li>
<li>각 항목 옆 <b>표에서 고르기</b>로 셀을 클릭·드래그해 넣을 수 있습니다.</li>
</ul>
<h4>한 셀 / 배열 수식</h4>
<p><b>한 셀</b> 또는 <b>배열 수식 (범위)</b> 방식으로 적용할 수 있습니다. 배열 수식은 범위마다 참조가 자동으로 밀립니다.</p>
<h4>표에서 고르기</h4>
<ul>
<li><b>적용 셀</b>의 <b>표에서 고르기</b>는 표에 현재 선택된 셀을 바로 지정합니다.</li>
<li>값 항목은 여러 셀을 고르면 해당 값 칸 앞 연산자로 이어 붙입니다 (예: <code>A1+B1</code>).</li>
<li>고르는 중 <b>Esc</b> 로 취소.</li>
</ul>)");
}

QPushButton* makeSegmentOperatorButton(const QString& label, QWidget* parent) {
    auto* button = new QPushButton(label, parent);
    button->setCheckable(true);
    button->setFixedWidth(36);
    button->setFixedHeight(28);
    return button;
}

} // namespace

FormulaBuilderDialog::FormulaBuilderDialog(SpreadsheetModel* model, QWidget* parent)
    : QDialog(parent)
    , m_model(model) {
    setWindowTitle(tr("수식 만들기"));
    setModal(false);
    resize(560, 640);

    auto* rootLayout = new QVBoxLayout(this);

    m_manualToggle = new QPushButton(tr("▶ 수식 설명 보기"), this);
    m_manualToggle->setCheckable(true);
    m_manualToggle->setFlat(true);
    m_manualToggle->setStyleSheet(QStringLiteral("text-align:left;"));
    connect(m_manualToggle, &QPushButton::toggled, this, &FormulaBuilderDialog::onManualToggled);
    rootLayout->addWidget(m_manualToggle);

    m_manualBrowser = new QTextBrowser(this);
    m_manualBrowser->setHtml(formulaManualHtml());
    m_manualBrowser->setMaximumHeight(200);
    m_manualBrowser->setVisible(false);
    rootLayout->addWidget(m_manualBrowser);

    auto* applyModeRow = new QHBoxLayout();
    applyModeRow->addWidget(new QLabel(tr("적용 방식:"), this));
    m_applyModeCombo = new QComboBox(this);
    m_applyModeCombo->addItem(tr("한 셀"), static_cast<int>(FormulaApplyMode::SingleCell));
    m_applyModeCombo->addItem(tr("배열 수식 (범위)"), static_cast<int>(FormulaApplyMode::ArrayFill));
    connect(m_applyModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormulaBuilderDialog::onApplyModeChanged);
    applyModeRow->addWidget(m_applyModeCombo, 1);
    rootLayout->addLayout(applyModeRow);

    auto* targetRow = new QHBoxLayout();
    m_targetLabel = new QLabel(tr("적용 셀:"), this);
    targetRow->addWidget(m_targetLabel);
    m_targetCellEdit = new QLineEdit(this);
    m_targetCellEdit->setReadOnly(true);
    m_targetCellEdit->setPlaceholderText(tr("A1"));
    targetRow->addWidget(m_targetCellEdit, 1);
    m_pickTargetButton = new QPushButton(tr("표에서 고르기"), this);
    m_pickTargetButton->setToolTip(tr("현재 표에서 선택한 셀을 적용 셀로 지정합니다."));
    connect(m_pickTargetButton, &QPushButton::clicked, this, &FormulaBuilderDialog::onPickTargetClicked);
    targetRow->addWidget(m_pickTargetButton);
    rootLayout->addLayout(targetRow);

    m_arrayRangeRow = new QWidget(this);
    auto* arrayRangeLayout = new QHBoxLayout(m_arrayRangeRow);
    arrayRangeLayout->setContentsMargins(0, 0, 0, 0);
    m_arrayRangeLabel = new QLabel(tr("적용 범위:"), m_arrayRangeRow);
    arrayRangeLayout->addWidget(m_arrayRangeLabel);
    m_arrayRangeEdit = new QLineEdit(m_arrayRangeRow);
    m_arrayRangeEdit->setReadOnly(true);
    m_arrayRangeEdit->setPlaceholderText(tr("C1:C5"));
    arrayRangeLayout->addWidget(m_arrayRangeEdit, 1);
    m_pickArrayRangeButton = new QPushButton(tr("표에서 고르기"), m_arrayRangeRow);
    connect(m_pickArrayRangeButton, &QPushButton::clicked, this, &FormulaBuilderDialog::onPickArrayRangeClicked);
    arrayRangeLayout->addWidget(m_pickArrayRangeButton);
    rootLayout->addWidget(m_arrayRangeRow);

    auto* operandGroup = new QGroupBox(tr("값"), this);
    auto* operandGroupLayout = new QVBoxLayout(operandGroup);

    auto* operandScroll = new QScrollArea(operandGroup);
    operandScroll->setWidgetResizable(true);
    operandScroll->setFrameShape(QFrame::NoFrame);
    operandScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    operandScroll->setMaximumHeight(280);

    m_operandListHost = new QWidget(operandScroll);
    m_operandRowsLayout = new QVBoxLayout(m_operandListHost);
    m_operandRowsLayout->setContentsMargins(0, 0, 0, 0);
    m_operandRowsLayout->setSpacing(4);
    operandScroll->setWidget(m_operandListHost);
    operandGroupLayout->addWidget(operandScroll);

    auto* operandToolbar = new QHBoxLayout();
    m_addOperandButton = new QPushButton(tr("값 추가"), operandGroup);
    connect(m_addOperandButton, &QPushButton::clicked, this, &FormulaBuilderDialog::onAddOperandClicked);
    operandToolbar->addWidget(m_addOperandButton);
    operandToolbar->addStretch(1);
    operandGroupLayout->addLayout(operandToolbar);
    rootLayout->addWidget(operandGroup);

    addOperandRow();
    addOperandRow();

    auto* previewForm = new QFormLayout();
    m_formulaPreview = new QLineEdit(this);
    m_formulaPreview->setPlaceholderText(tr("=A1+B1 또는 직접 입력"));
    connect(m_formulaPreview, &QLineEdit::textChanged, this, &FormulaBuilderDialog::onFormulaPreviewEdited);
    previewForm->addRow(tr("수식 미리보기:"), m_formulaPreview);
    m_resultPreviewLabel = new QLabel(tr("—"), this);
    previewForm->addRow(tr("계산 결과:"), m_resultPreviewLabel);
    rootLayout->addLayout(previewForm);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_applyButton = new QPushButton(tr("적용"), this);
    buttons->addButton(m_applyButton, QDialogButtonBox::ApplyRole);
    connect(m_applyButton, &QPushButton::clicked, this, &FormulaBuilderDialog::onApplyClicked);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);

    setTargetCell(0, 0);
    updateApplyModeUi();
    updatePickButtonStyles();
    updateOperandDeleteButtons();
    rebuildFormulaPreview();
}

QChar FormulaBuilderDialog::currentOperator() const {
    return operatorForOperandPick(m_activeOperandIndex >= 0 ? m_activeOperandIndex : 0);
}

QChar FormulaBuilderDialog::operatorForOperandPick(int operandIndex) const {
    if (operandIndex <= 0) {
        if (!m_operatorSegments.isEmpty()) {
            return m_operatorSegments.first().op;
        }
        return QLatin1Char('+');
    }
    const int segmentIndex = operandIndex - 1;
    if (segmentIndex >= 0 && segmentIndex < m_operatorSegments.size()) {
        return m_operatorSegments.at(segmentIndex).op;
    }
    return QLatin1Char('+');
}

void FormulaBuilderDialog::addOperatorSegment(QChar initialOp) {
    OperatorSegment segment;
    segment.op = initialOp;
    segment.segmentWidget = new QWidget(m_operandListHost);
    auto* layout = new QHBoxLayout(segment.segmentWidget);
    layout->setContentsMargins(48, 0, 0, 0);
    layout->addStretch(1);

    segment.addButton = makeSegmentOperatorButton(QStringLiteral("+"), segment.segmentWidget);
    segment.subButton = makeSegmentOperatorButton(QStringLiteral("−"), segment.segmentWidget);
    segment.mulButton = makeSegmentOperatorButton(QStringLiteral("×"), segment.segmentWidget);
    segment.divButton = makeSegmentOperatorButton(QStringLiteral("÷"), segment.segmentWidget);

    const int segmentIndex = m_operatorSegments.size();
    segment.addButton->setProperty("segmentIndex", segmentIndex);
    segment.subButton->setProperty("segmentIndex", segmentIndex);
    segment.mulButton->setProperty("segmentIndex", segmentIndex);
    segment.divButton->setProperty("segmentIndex", segmentIndex);

    for (QPushButton* button :
         {segment.addButton, segment.subButton, segment.mulButton, segment.divButton}) {
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, &FormulaBuilderDialog::onSegmentOperatorClicked);
    }

    layout->addStretch(1);
    m_operatorSegments.append(segment);
    setSegmentOperator(segmentIndex, initialOp);
}

void FormulaBuilderDialog::removeOperatorSegment(int index) {
    if (index < 0 || index >= m_operatorSegments.size()) {
        return;
    }
    OperatorSegment segment = m_operatorSegments.takeAt(index);
    m_operandRowsLayout->removeWidget(segment.segmentWidget);
    segment.segmentWidget->deleteLater();

    for (int i = index; i < m_operatorSegments.size(); ++i) {
        for (QPushButton* button : {m_operatorSegments[i].addButton,
                                    m_operatorSegments[i].subButton,
                                    m_operatorSegments[i].mulButton,
                                    m_operatorSegments[i].divButton}) {
            button->setProperty("segmentIndex", i);
        }
    }
}

void FormulaBuilderDialog::setSegmentOperator(int segmentIndex, QChar op) {
    if (segmentIndex < 0 || segmentIndex >= m_operatorSegments.size()) {
        return;
    }
    OperatorSegment& segment = m_operatorSegments[segmentIndex];
    segment.op = op;
    segment.addButton->setChecked(op == QLatin1Char('+'));
    segment.subButton->setChecked(op == QLatin1Char('-'));
    segment.mulButton->setChecked(op == QLatin1Char('*'));
    segment.divButton->setChecked(op == QLatin1Char('/'));
    const auto styleActive = [](bool active) {
        return active ? QStringLiteral("font-weight:bold;") : QString();
    };
    segment.addButton->setStyleSheet(styleActive(op == QLatin1Char('+')));
    segment.subButton->setStyleSheet(styleActive(op == QLatin1Char('-')));
    segment.mulButton->setStyleSheet(styleActive(op == QLatin1Char('*')));
    segment.divButton->setStyleSheet(styleActive(op == QLatin1Char('/')));
}

void FormulaBuilderDialog::rebuildOperandLayout() {
    while (QLayoutItem* item = m_operandRowsLayout->takeAt(0)) {
        delete item;
    }

    for (int i = 0; i < m_operandRows.size(); ++i) {
        if (i > 0 && i - 1 < m_operatorSegments.size()) {
            m_operandRowsLayout->addWidget(m_operatorSegments[i - 1].segmentWidget);
        }
        m_operandRowsLayout->addWidget(m_operandRows[i].rowWidget);
    }
}

void FormulaBuilderDialog::addOperandRow(const QString& initialText) {
    if (!m_operandRows.isEmpty()) {
        addOperatorSegment(QLatin1Char('+'));
    }

    OperandRow row;
    row.rowWidget = new QWidget(m_operandListHost);
    auto* rowLayout = new QHBoxLayout(row.rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);

    row.label = new QLabel(row.rowWidget);
    row.edit = new QLineEdit(row.rowWidget);
    row.edit->setPlaceholderText(tr("A1 또는 숫자"));
    row.edit->setText(initialText);
    connect(row.edit, &QLineEdit::textChanged, this, &FormulaBuilderDialog::onOperandEdited);

    row.pickButton = new QPushButton(tr("표에서 고르기"), row.rowWidget);
    connect(row.pickButton, &QPushButton::clicked, this, &FormulaBuilderDialog::onPickOperandClicked);

    row.parenButton = new QPushButton(QStringLiteral("( )"), row.rowWidget);
    row.parenButton->setCheckable(true);
    row.parenButton->setToolTip(tr("이 값을 괄호로 묶기"));
    row.parenButton->setFixedWidth(40);
    connect(row.parenButton, &QPushButton::toggled, this, &FormulaBuilderDialog::onParenToggled);

    row.deleteButton = new QPushButton(tr("삭제"), row.rowWidget);
    connect(row.deleteButton, &QPushButton::clicked, this, &FormulaBuilderDialog::onRemoveOperandClicked);

    rowLayout->addWidget(row.label);
    rowLayout->addWidget(row.edit, 1);
    rowLayout->addWidget(row.pickButton);
    rowLayout->addWidget(row.parenButton);
    rowLayout->addWidget(row.deleteButton);

    m_operandRows.append(row);
    rebuildOperandLayout();
    renumberOperandLabels();
    updateOperandDeleteButtons();
    rebuildFormulaPreview();
}

void FormulaBuilderDialog::removeOperandRow(int index) {
    if (index < 0 || index >= m_operandRows.size() || m_operandRows.size() <= kMinOperandRows) {
        return;
    }

    if (m_activePickSlot == FormulaPickSlot::Operand && m_activeOperandIndex == index) {
        endPickMode();
    }

    if (index > 0) {
        removeOperatorSegment(index - 1);
    } else if (!m_operatorSegments.isEmpty()) {
        removeOperatorSegment(0);
    }

    OperandRow row = m_operandRows.takeAt(index);
    row.rowWidget->deleteLater();

    if (m_activeOperandIndex > index) {
        --m_activeOperandIndex;
    } else if (m_activeOperandIndex == index) {
        m_activeOperandIndex = -1;
    }

    rebuildOperandLayout();
    renumberOperandLabels();
    updateOperandDeleteButtons();
    updatePickButtonStyles();
    rebuildFormulaPreview();
}

void FormulaBuilderDialog::renumberOperandLabels() {
    for (int i = 0; i < m_operandRows.size(); ++i) {
        m_operandRows[i].label->setText(tr("값 %1:").arg(i + 1));
    }
}

void FormulaBuilderDialog::updateOperandDeleteButtons() {
    const bool canDelete = m_operandRows.size() > kMinOperandRows;
    for (const OperandRow& row : m_operandRows) {
        row.deleteButton->setEnabled(canDelete);
    }
}

int FormulaBuilderDialog::operandIndexForWidget(const QWidget* widget) const {
    for (int i = 0; i < m_operandRows.size(); ++i) {
        const OperandRow& row = m_operandRows.at(i);
        if (widget == row.pickButton || widget == row.deleteButton || widget == row.edit
            || widget == row.parenButton) {
            return i;
        }
    }
    return -1;
}

int FormulaBuilderDialog::segmentIndexForWidget(const QWidget* widget) const {
    for (int i = 0; i < m_operatorSegments.size(); ++i) {
        const OperatorSegment& segment = m_operatorSegments.at(i);
        if (widget == segment.addButton || widget == segment.subButton || widget == segment.mulButton
            || widget == segment.divButton) {
            return i;
        }
    }
    return -1;
}

void FormulaBuilderDialog::setTargetCell(int row, int col) {
    m_targetRow = row;
    m_targetCol = col;
    m_targetCellEdit->setText(FormulaEvaluator::cellReference(row, col));
    rebuildFormulaPreview();
}

void FormulaBuilderDialog::onManualToggled(bool checked) {
    m_manualBrowser->setVisible(checked);
    m_manualToggle->setText(checked ? tr("▼ 수식 설명 숨기기") : tr("▶ 수식 설명 보기"));
}

void FormulaBuilderDialog::onApplyModeChanged(int index) {
    m_applyMode = static_cast<FormulaApplyMode>(m_applyModeCombo->itemData(index).toInt());
    updateApplyModeUi();
}

void FormulaBuilderDialog::updateApplyModeUi() {
    const bool arrayMode = (m_applyMode == FormulaApplyMode::ArrayFill);
    m_arrayRangeRow->setVisible(arrayMode);
    m_targetLabel->setVisible(!arrayMode);
    m_targetCellEdit->setVisible(!arrayMode);
    m_pickTargetButton->setVisible(!arrayMode);

    if (arrayMode && m_activePickSlot == FormulaPickSlot::Target) {
        endPickMode();
    }
    if (!arrayMode && m_activePickSlot == FormulaPickSlot::ArrayRange) {
        endPickMode();
    }
    updatePickButtonStyles();
}

void FormulaBuilderDialog::onSegmentOperatorClicked() {
    const int segmentIndex = segmentIndexForWidget(qobject_cast<QWidget*>(sender()));
    if (segmentIndex < 0) {
        return;
    }

    auto* senderButton = qobject_cast<QPushButton*>(sender());
    if (!senderButton) {
        return;
    }

    QChar op = QLatin1Char('+');
    if (senderButton == m_operatorSegments[segmentIndex].subButton) {
        op = QLatin1Char('-');
    } else if (senderButton == m_operatorSegments[segmentIndex].mulButton) {
        op = QLatin1Char('*');
    } else if (senderButton == m_operatorSegments[segmentIndex].divButton) {
        op = QLatin1Char('/');
    }

    setSegmentOperator(segmentIndex, op);
    rebuildFormulaPreview();
}

void FormulaBuilderDialog::onOperandEdited() {
    rebuildFormulaPreview();
}

void FormulaBuilderDialog::onFormulaPreviewEdited() {
    updatePreviewResult();
}

void FormulaBuilderDialog::onParenToggled() {
    const int index = operandIndexForWidget(qobject_cast<QWidget*>(sender()));
    if (index < 0 || index >= m_operandRows.size()) {
        return;
    }
    m_operandRows[index].wrapInParens = m_operandRows[index].parenButton->isChecked();
    rebuildFormulaPreview();
}

void FormulaBuilderDialog::onAddOperandClicked() {
    addOperandRow();
}

void FormulaBuilderDialog::onRemoveOperandClicked() {
    const int index = operandIndexForWidget(qobject_cast<QWidget*>(sender()));
    if (index >= 0) {
        removeOperandRow(index);
    }
}

void FormulaBuilderDialog::requestPick(FormulaPickSlot slot) {
    if (m_activePickSlot == slot && slot != FormulaPickSlot::Operand) {
        endPickMode();
        return;
    }
    m_activePickSlot = slot;
    updatePickButtonStyles();
    emit pickModeRequested(slot);
}

void FormulaBuilderDialog::onPickTargetClicked() {
    m_activeOperandIndex = -1;
    requestPick(FormulaPickSlot::Target);
}

void FormulaBuilderDialog::onPickOperandClicked() {
    const int index = operandIndexForWidget(qobject_cast<QWidget*>(sender()));
    if (index < 0) {
        return;
    }
    if (m_activePickSlot == FormulaPickSlot::Operand && m_activeOperandIndex == index) {
        endPickMode();
        return;
    }
    m_activeOperandIndex = index;
    requestPick(FormulaPickSlot::Operand);
}

void FormulaBuilderDialog::onPickArrayRangeClicked() {
    m_activeOperandIndex = -1;
    requestPick(FormulaPickSlot::ArrayRange);
}

void FormulaBuilderDialog::endPickMode() {
    if (m_activePickSlot == FormulaPickSlot::None) {
        return;
    }
    m_activePickSlot = FormulaPickSlot::None;
    m_activeOperandIndex = -1;
    updatePickButtonStyles();
    emit pickModeEnded();
}

void FormulaBuilderDialog::updatePickButtonStyles() {
    auto styleActive = [](bool active) {
        return active ? QStringLiteral("font-weight:bold;") : QString();
    };
    m_pickTargetButton->setStyleSheet(styleActive(m_activePickSlot == FormulaPickSlot::Target));
    m_pickArrayRangeButton->setStyleSheet(styleActive(m_activePickSlot == FormulaPickSlot::ArrayRange));

    for (int i = 0; i < m_operandRows.size(); ++i) {
        const bool active = m_activePickSlot == FormulaPickSlot::Operand && m_activeOperandIndex == i;
        m_operandRows[i].pickButton->setStyleSheet(styleActive(active));
    }
}

void FormulaBuilderDialog::setArrayRange(int minRow, int minCol, int maxRow, int maxCol) {
    m_rangeMinRow = minRow;
    m_rangeMinCol = minCol;
    m_rangeMaxRow = maxRow;
    m_rangeMaxCol = maxCol;
    m_arrayRangeEdit->setText(FormulaEvaluator::formatCellRange(minRow, minCol, maxRow, maxCol));
}

bool FormulaBuilderDialog::arrayRangeValid() const {
    return m_rangeMinRow >= 0 && m_rangeMinCol >= 0
           && m_rangeMaxRow >= m_rangeMinRow && m_rangeMaxCol >= m_rangeMinCol;
}

int FormulaBuilderDialog::arrayCellCount() const {
    if (!arrayRangeValid()) {
        return 0;
    }
    return (m_rangeMaxRow - m_rangeMinRow + 1) * (m_rangeMaxCol - m_rangeMinCol + 1);
}

QString FormulaBuilderDialog::wrapOperand(const QString& text) const {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return trimmed;
    }
    bool ok = false;
    trimmed.toDouble(&ok);
    if (ok) {
        return trimmed;
    }
    if (trimmed.startsWith(QLatin1Char('(')) && trimmed.endsWith(QLatin1Char(')'))) {
        return trimmed;
    }
    return QStringLiteral("(") + trimmed + QStringLiteral(")");
}

QString FormulaBuilderDialog::formatOperandTerm(int index) const {
    if (index < 0 || index >= m_operandRows.size()) {
        return {};
    }
    const QString raw = m_operandRows[index].edit->text().trimmed();
    if (raw.isEmpty()) {
        return {};
    }
    if (m_operandRows[index].wrapInParens) {
        return QStringLiteral("(") + raw + QStringLiteral(")");
    }
    return wrapOperand(raw);
}

QString FormulaBuilderDialog::joinCellRefs(const QStringList& refs, QChar op) {
    if (refs.isEmpty()) {
        return {};
    }
    QString joined = refs.first();
    for (int i = 1; i < refs.size(); ++i) {
        joined += op;
        joined += refs.at(i);
    }
    return joined;
}

void FormulaBuilderDialog::applyPickedCellRefs(const QStringList& cellRefs) {
    if (cellRefs.isEmpty()) {
        return;
    }

    switch (m_activePickSlot) {
    case FormulaPickSlot::Target: {
        int row = 0;
        int col = 0;
        if (FormulaEvaluator::cellAddressFromReference(cellRefs.first(), row, col)) {
            setTargetCell(row, col);
        }
        break;
    }
    case FormulaPickSlot::ArrayRange: {
        if (cellRefs.size() == 1) {
            int row = 0;
            int col = 0;
            if (FormulaEvaluator::cellAddressFromReference(cellRefs.first(), row, col)) {
                setArrayRange(row, col, row, col);
            }
        } else {
            int minRow = INT_MAX;
            int minCol = INT_MAX;
            int maxRow = -1;
            int maxCol = -1;
            for (const QString& ref : cellRefs) {
                int row = 0;
                int col = 0;
                if (!FormulaEvaluator::cellAddressFromReference(ref, row, col)) {
                    continue;
                }
                minRow = qMin(minRow, row);
                minCol = qMin(minCol, col);
                maxRow = qMax(maxRow, row);
                maxCol = qMax(maxCol, col);
            }
            if (maxRow >= 0) {
                setArrayRange(minRow, minCol, maxRow, maxCol);
            }
        }
        break;
    }
    case FormulaPickSlot::Operand:
        if (m_activeOperandIndex >= 0 && m_activeOperandIndex < m_operandRows.size()) {
            const QChar op = operatorForOperandPick(m_activeOperandIndex);
            m_operandRows[m_activeOperandIndex].edit->setText(joinCellRefs(cellRefs, op));
        }
        break;
    default:
        break;
    }

    endPickMode();
}

void FormulaBuilderDialog::rebuildFormulaPreview() {
    if (m_formulaPreview && m_formulaPreview->hasFocus()) {
        return;
    }

    QSignalBlocker blocker(m_formulaPreview);

    QString formulaBody;
    int lastIncludedIndex = -1;
    for (int i = 0; i < m_operandRows.size(); ++i) {
        const QString term = formatOperandTerm(i);
        if (term.isEmpty()) {
            continue;
        }
        if (lastIncludedIndex >= 0) {
            const int segmentIndex = i - 1;
            QChar op = QLatin1Char('+');
            if (segmentIndex >= 0 && segmentIndex < m_operatorSegments.size()) {
                op = m_operatorSegments.at(segmentIndex).op;
            }
            formulaBody += op;
        }
        formulaBody += term;
        lastIncludedIndex = i;
    }

    if (formulaBody.isEmpty()) {
        m_formulaPreview->clear();
        m_resultPreviewLabel->setText(tr("—"));
        return;
    }

    m_formulaPreview->setText(QStringLiteral("=") + formulaBody);
    updatePreviewResult();
}

void FormulaBuilderDialog::updatePreviewResult() {
    if (!m_model) {
        m_resultPreviewLabel->setText(tr("—"));
        return;
    }

    const QString preview = m_formulaPreview->text();
    if (preview.isEmpty()) {
        m_resultPreviewLabel->setText(tr("—"));
        return;
    }

    const FormulaResult result = m_model->evaluateFormulaAtCell(preview, m_targetRow, m_targetCol);

    if (!result.ok) {
        QString errorText = tr("오류");
        switch (result.error) {
        case FormulaError::Ref:
            errorText = tr("참조 오류");
            break;
        case FormulaError::DivZero:
            errorText = tr("0으로 나눔");
            break;
        case FormulaError::Parse:
            errorText = tr("수식 오류");
            break;
        default:
            break;
        }
        m_resultPreviewLabel->setText(errorText);
        return;
    }

    m_resultPreviewLabel->setText(m_model->formatNumber(result.value));
}

void FormulaBuilderDialog::onApplyClicked() {
    const QString preview = m_formulaPreview->text();
    if (preview.isEmpty() || preview == QStringLiteral("=")) {
        QMessageBox::warning(this, tr("수식 만들기"), tr("수식을 입력하세요."));
        return;
    }

    const QString formulaBody = preview.startsWith(QLatin1Char('='))
                                  ? preview.mid(1)
                                  : preview;

    if (m_applyMode == FormulaApplyMode::SingleCell) {
        emit formulaApplied(m_targetRow, m_targetCol, formulaBody);
        return;
    }

    if (!arrayRangeValid()) {
        QMessageBox::warning(this, tr("수식 만들기"),
                             tr("배열 수식을 적용할 범위를 표에서 고르세요."));
        return;
    }

    const int cellCount = arrayCellCount();
    const QString rangeText = m_arrayRangeEdit->text();
    const auto reply = QMessageBox::question(
        this,
        tr("배열 수식 적용"),
        tr("%1 범위의 %2개 셀에 같은 패턴의 수식을 넣을까요?\n"
           "각 셀마다 참조(A1 등)가 행·열 차이만큼 자동으로 밀립니다.")
            .arg(rangeText)
            .arg(cellCount),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (reply != QMessageBox::Yes) {
        return;
    }

    struct PendingApply {
        int row = 0;
        int col = 0;
        QString formula;
    };
    QList<PendingApply> pending;
    pending.reserve(cellCount);

    for (int row = m_rangeMinRow; row <= m_rangeMaxRow; ++row) {
        for (int col = m_rangeMinCol; col <= m_rangeMaxCol; ++col) {
            const int deltaRow = row - m_rangeMinRow;
            const int deltaCol = col - m_rangeMinCol;
            const QString shifted = FormulaEvaluator::shiftFormulaReferences(formulaBody, deltaRow, deltaCol);
            if (shifted.isEmpty()) {
                QMessageBox::warning(this, tr("수식 만들기"),
                                     tr("범위 밖으로 참조가 밀려 수식을 만들 수 없습니다."));
                return;
            }
            pending.append({row, col, shifted});
        }
    }

    for (const PendingApply& item : pending) {
        emit formulaApplied(item.row, item.col, item.formula);
    }
}
