#include "ui/editors/CommentEditor.h"

#include "ui/UiStrings.h"

#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>

CommentEditor::CommentEditor(CommentBlock* block, QWidget* parent, bool embedded)
    : QDialog(parent)
    , m_block(block)
    , m_embedded(embedded) {
    if (!m_embedded) {
        setWindowTitle(tr("주석 블록 편집"));
    }
    setupUi();
}

void CommentEditor::setBlock(CommentBlock* block) {
    m_block = block;
    reload();
}

void CommentEditor::reload() {
    if (!m_block) {
        return;
    }
    m_textEdit->setPlainText(QString::fromStdString(m_block->text));
}

void CommentEditor::apply() {
    if (!m_block) {
        return;
    }
    m_block->text = m_textEdit->toPlainText().toStdString();
}

void CommentEditor::setupUi() {
    auto* layout = new QVBoxLayout(this);
    m_textEdit = new QPlainTextEdit(QString::fromStdString(m_block->text), this);
    layout->addWidget(m_textEdit);

    if (!m_embedded) {
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        localizeDialogButtons(buttons);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            apply();
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
}
