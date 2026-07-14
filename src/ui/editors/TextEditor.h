#pragma once

#include "core/workflow/blocks/TextBlock.h"

#include <QDialog>

class QPlainTextEdit;

class TextEditor : public QDialog {
    Q_OBJECT
public:
    explicit TextEditor(TextBlock* block, QWidget* parent = nullptr, bool embedded = false);

    void setBlock(TextBlock* block);
    void reload();
    void apply();
    void focusTextInput();

signals:
    void layoutChanged();

private:
    void setupUi();

    TextBlock* m_block = nullptr;
    bool m_embedded = false;
    QPlainTextEdit* m_textEdit = nullptr;
};
