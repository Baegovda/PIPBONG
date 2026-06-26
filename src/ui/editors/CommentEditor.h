#pragma once

#include "core/workflow/blocks/CommentBlock.h"

#include <QDialog>

class QPlainTextEdit;

class CommentEditor : public QDialog {
    Q_OBJECT
public:
    explicit CommentEditor(CommentBlock* block, QWidget* parent = nullptr, bool embedded = false);

    void setBlock(CommentBlock* block);
    void reload();
    void apply();

private:
    void setupUi();

    CommentBlock* m_block = nullptr;
    bool m_embedded = false;
    QPlainTextEdit* m_textEdit = nullptr;
};
