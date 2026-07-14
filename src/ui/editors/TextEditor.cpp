#include "ui/editors/TextEditor.h"

#include "ui/UiStrings.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

TextEditor::TextEditor(TextBlock* block, QWidget* parent, bool embedded)
    : QDialog(parent)
    , m_block(block)
    , m_embedded(embedded) {
    if (!m_embedded) {
        setWindowTitle(tr("텍스트 블록 편집"));
    }
    setupUi();
    reload();
}

void TextEditor::setBlock(TextBlock* block) {
    m_block = block;
    reload();
}

void TextEditor::reload() {
    if (!m_block || !m_textEdit) {
        return;
    }
    m_textEdit->setPlainText(QString::fromStdString(m_block->text));
}

void TextEditor::apply() {
    if (!m_block || !m_textEdit) {
        return;
    }
    m_block->text = m_textEdit->toPlainText().toStdString();
}

void TextEditor::focusTextInput() {
    if (m_textEdit) {
        m_textEdit->setFocus(Qt::OtherFocusReason);
    }
}

void TextEditor::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* hint = new QLabel(tr("실행 시 대상 앱에 아래 텍스트를 그대로 입력합니다. Enter로 줄바꿈하면 실행 시 Enter 키가 입력됩니다."),
                            this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setPlaceholderText(tr("입력할 텍스트…"));
    m_textEdit->setMinimumHeight(140);
    m_textEdit->setTabChangesFocus(true);
    layout->addWidget(m_textEdit, 1);

    connect(m_textEdit, &QPlainTextEdit::textChanged, this, [this]() { emit layoutChanged(); });

    if (!m_embedded) {
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        localizeDialogButtons(buttons);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
}
