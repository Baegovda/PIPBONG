#include "ui/editors/BranchWorkflowEditor.h"

#include "core/workflow/BlockFactory.h"
#include "ui/editors/BlockEditorDialog.h"
#include "ui/UiStrings.h"

#include <QAction>
#include <QCursor>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

BranchWorkflowEditor::BranchWorkflowEditor(Workflow* workflow, const QString& projectDirectory,
                                           const QString& title, QWidget* parent)
    : QWidget(parent)
    , m_workflow(workflow)
    , m_projectDirectory(projectDirectory) {
    setupUi(title);
    reload();
}

void BranchWorkflowEditor::setupUi(const QString& title) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* group = new QGroupBox(title, this);
    auto* groupLayout = new QVBoxLayout(group);

    m_list = new QListWidget(group);
    m_list->setMinimumHeight(100);
    groupLayout->addWidget(m_list);

    auto* buttonRow = new QHBoxLayout();
    auto* addButton = new QPushButton(tr("블록 추가"), group);
    auto* editButton = new QPushButton(tr("편집"), group);
    auto* deleteButton = new QPushButton(tr("삭제"), group);
    auto* upButton = new QPushButton(tr("위로"), group);
    auto* downButton = new QPushButton(tr("아래로"), group);
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(editButton);
    buttonRow->addWidget(deleteButton);
    buttonRow->addWidget(upButton);
    buttonRow->addWidget(downButton);
    buttonRow->addStretch();
    groupLayout->addLayout(buttonRow);

    layout->addWidget(group);

    connect(addButton, &QPushButton::clicked, this, [this]() {
        QMenu menu(this);
        const BlockType types[] = {BlockType::ImageFind,
                                   BlockType::Click,
                                   BlockType::KeyPress,
                                   BlockType::Wait,
                                   BlockType::Loop};
        for (const BlockType type : types) {
            QAction* action = menu.addAction(blockTypeDisplayName(type));
            connect(action, &QAction::triggered, this, [this, type]() { addBlockOfType(type); });
        }
        menu.exec(QCursor::pos());
    });
    connect(editButton, &QPushButton::clicked, this, &BranchWorkflowEditor::editSelectedBlock);
    connect(deleteButton, &QPushButton::clicked, this, &BranchWorkflowEditor::deleteSelectedBlock);
    connect(upButton, &QPushButton::clicked, this, &BranchWorkflowEditor::moveSelectedUp);
    connect(downButton, &QPushButton::clicked, this, &BranchWorkflowEditor::moveSelectedDown);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &BranchWorkflowEditor::editSelectedBlock);
}

void BranchWorkflowEditor::reload() {
    refreshList();
}

void BranchWorkflowEditor::setWorkflow(Workflow* workflow) {
    m_workflow = workflow;
    reload();
}

void BranchWorkflowEditor::refreshList() {
    m_list->clear();
    if (!m_workflow) {
        return;
    }
    const auto& blocks = m_workflow->blocks();
    for (size_t i = 0; i < blocks.size(); ++i) {
        const QString label =
            QStringLiteral("%1. %2 — %3")
                .arg(static_cast<int>(i) + 1)
                .arg(blockTypeWorkflowActionName(blocks[i]->type()))
                .arg(QString::fromStdString(blocks[i]->summary()));
        m_list->addItem(label);
    }
}

int BranchWorkflowEditor::selectedRow() const {
    const QList<QListWidgetItem*> selected = m_list->selectedItems();
    if (selected.isEmpty()) {
        return -1;
    }
    return m_list->row(selected.first());
}

void BranchWorkflowEditor::addBlockOfType(BlockType type) {
    if (!m_workflow) {
        return;
    }

    auto draft = BlockFactory::create(type);
    BlockEditorDialog dialog(draft.get(), m_projectDirectory, this);
    if (m_workflow) {
        dialog.setWorkflowEditorContext(static_cast<int>(m_workflow->blocks().size()) + 1,
                                        static_cast<int>(m_workflow->blocks().size()));
    }
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_workflow->addBlock(dialog.takeBlock());
    refreshList();
    m_list->setCurrentRow(static_cast<int>(m_workflow->blocks().size()) - 1);
    emit changed();
}

void BranchWorkflowEditor::editSelectedBlock() {
    if (!m_workflow) {
        return;
    }

    const int row = selectedRow();
    if (row < 0 || row >= static_cast<int>(m_workflow->blocks().size())) {
        return;
    }

    BlockEditorDialog dialog(m_workflow->blocks()[row].get(), m_projectDirectory, this);
    dialog.setWorkflowEditorContext(static_cast<int>(m_workflow->blocks().size()), row);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_workflow->replaceBlock(row, dialog.takeBlock());
    refreshList();
    m_list->setCurrentRow(row);
    emit changed();
}

void BranchWorkflowEditor::deleteSelectedBlock() {
    if (!m_workflow) {
        return;
    }

    const int row = selectedRow();
    if (row < 0 || row >= static_cast<int>(m_workflow->blocks().size())) {
        return;
    }

    const auto ret = QMessageBox::question(this,
                                           tr("블록 삭제"),
                                           tr("선택한 블록을 삭제할까요?"),
                                           QMessageBox::Yes | QMessageBox::No,
                                           QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }

    m_workflow->removeBlock(row);
    refreshList();
    emit changed();
}

void BranchWorkflowEditor::moveSelectedUp() {
    if (!m_workflow) {
        return;
    }
    const int row = selectedRow();
    if (row <= 0) {
        return;
    }
    m_workflow->moveBlockUp(row);
    refreshList();
    m_list->setCurrentRow(row - 1);
    emit changed();
}

void BranchWorkflowEditor::moveSelectedDown() {
    if (!m_workflow) {
        return;
    }
    const int row = selectedRow();
    if (row < 0 || row + 1 >= static_cast<int>(m_workflow->blocks().size())) {
        return;
    }
    m_workflow->moveBlockDown(row);
    refreshList();
    m_list->setCurrentRow(row + 1);
    emit changed();
}
