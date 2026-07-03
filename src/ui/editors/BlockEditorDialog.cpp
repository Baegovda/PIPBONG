#include "ui/editors/BlockEditorDialog.h"

#include "core/workflow/BlockFactory.h"
#include "core/workflow/blocks/ClickBlock.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/blocks/KeyPressBlock.h"
#include "core/workflow/blocks/WaitBlock.h"
#include "ui/editors/ClickEditor.h"
#include "ui/editors/ImageFindEditor.h"
#include "ui/editors/KeyPressEditor.h"
#include "ui/editors/WaitEditor.h"
#include "ui/editors/MatchTestOverlay.h"
#include "ui/editors/RoiPreviewOverlay.h"
#include "ui/editors/ScreenRegionOverlay.h"
#include "ui/UiStrings.h"
#include "ui/widgets/HintLabel.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

int stackIndexFor(BlockType type) {
    switch (type) {
    case BlockType::ImageFind:
        return 0;
    case BlockType::Click:
        return 1;
    case BlockType::KeyPress:
        return 2;
    case BlockType::Wait:
        return 3;
    }
    return 0;
}

} // namespace

BlockEditorDialog::BlockEditorDialog(Block* block, const QString& projectDirectory, QWidget* parent)
    : QDialog(parent)
    , m_initialType(block->type())
    , m_block(block->clone())
    , m_projectDirectory(projectDirectory) {
    setWindowTitle(tr("블록 편집"));
    setupUi();

    syncTypeButtonSelection(m_block->type());
    reloadEditorForType(m_block->type());
    m_stack->setCurrentIndex(stackIndexFor(m_block->type()));
    fitToCurrentPage();
}

void BlockEditorDialog::setWorkflowEditorContext(int blockCount, int editingBlockIndex) {
    m_workflowBlockCount = blockCount;
    m_editingBlockIndex = editingBlockIndex;
}

void BlockEditorDialog::setRoiCorrectionUiPolicy(bool featureGlobalEnabled, bool sessionEligible) {
    if (m_imageFindEditor) {
        m_imageFindEditor->setRoiCorrectionUiPolicy(featureGlobalEnabled, sessionEligible);
    }
}

void BlockEditorDialog::setClickFeatureRunOptions(bool lockMouseToScreenCenterDuringRun) {
    if (m_clickEditor) {
        m_clickEditor->setFeatureRunOptions(lockMouseToScreenCenterDuringRun);
    }
}

bool BlockEditorDialog::lockMouseToScreenCenterDuringRun() const {
    return m_clickEditor ? m_clickEditor->lockMouseToScreenCenterDuringRun() : false;
}

void BlockEditorDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    auto* typeRow = new QHBoxLayout();
    typeRow->setSpacing(6);
    setupTypeButtons(typeRow);
    layout->addLayout(typeRow);

    m_typeChangeNote = new HintLabel(
        tr("블록 유형을 변경하면 해당 유형의 기본값으로 초기화됩니다."),
        this,
        HintLabel::Tone::Warning);
    m_typeChangeNote->hide();
    layout->addWidget(m_typeChangeNote);

    m_stack = new QStackedWidget(this);

    m_imageFindFormBlock = BlockFactory::create(BlockType::ImageFind);
    m_clickFormBlock = BlockFactory::create(BlockType::Click);
    m_keyPressFormBlock = BlockFactory::create(BlockType::KeyPress);
    m_waitFormBlock = BlockFactory::create(BlockType::Wait);

    m_imageFindEditor = new ImageFindEditor(
        static_cast<ImageFindBlock*>(m_imageFindFormBlock.get()), m_projectDirectory, this, true);
    m_clickEditor = new ClickEditor(static_cast<ClickBlock*>(m_clickFormBlock.get()), this, true);
    m_keyPressEditor =
        new KeyPressEditor(static_cast<KeyPressBlock*>(m_keyPressFormBlock.get()), this, true);
    m_waitEditor = new WaitEditor(static_cast<WaitBlock*>(m_waitFormBlock.get()), this, true);

    m_stack->addWidget(m_imageFindEditor);
    m_stack->addWidget(m_clickEditor);
    m_stack->addWidget(m_keyPressEditor);
    m_stack->addWidget(m_waitEditor);
    m_stack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    layout->addWidget(m_stack, 0);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        switch (m_block->type()) {
        case BlockType::ImageFind:
            m_imageFindEditor->apply();
            break;
        case BlockType::Click:
            m_clickEditor->apply();
            break;
        case BlockType::KeyPress:
            if (!m_keyPressEditor->apply()) {
                return;
            }
            break;
        case BlockType::Wait:
            m_waitEditor->apply();
            break;
        }
        dismissCaptureOverlays();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_clickEditor, &ClickEditor::layoutChanged, this, &BlockEditorDialog::fitToCurrentPage);
    connect(m_waitEditor, &WaitEditor::layoutChanged, this, &BlockEditorDialog::fitToCurrentPage);
    connect(m_stack, &QStackedWidget::currentChanged, this, [this](int) {
        if (m_imageFindEditor) {
            m_imageFindEditor->refreshTemplateCaptureHotkeyHook();
        }
        if (m_clickEditor) {
            m_clickEditor->refreshClickCursorHotkeyHook();
        }
    });
}

void BlockEditorDialog::setupTypeButtons(QHBoxLayout* row) {
    m_typeButtonGroup = new QButtonGroup(this);
    m_typeButtonGroup->setExclusive(true);

    const BlockType types[] = {BlockType::ImageFind,
                               BlockType::Click,
                               BlockType::KeyPress,
                               BlockType::Wait};

    for (const BlockType type : types) {
        auto* button = new QPushButton(blockTypeDisplayName(type), this);
        button->setCheckable(true);
        m_typeButtonGroup->addButton(button, static_cast<int>(type));
        row->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, type]() { onTypeSelected(type); });
    }

    row->addStretch();
}

void BlockEditorDialog::syncTypeButtonSelection(BlockType type) {
    if (!m_typeButtonGroup) {
        return;
    }
    if (QAbstractButton* button = m_typeButtonGroup->button(static_cast<int>(type))) {
        button->blockSignals(true);
        button->setChecked(true);
        button->blockSignals(false);
    }
}

void BlockEditorDialog::reloadEditorForType(BlockType type) {
    switch (type) {
    case BlockType::ImageFind:
        m_imageFindEditor->setBlock(static_cast<ImageFindBlock*>(m_block.get()));
        break;
    case BlockType::Click:
        m_clickEditor->setBlock(static_cast<ClickBlock*>(m_block.get()));
        break;
    case BlockType::KeyPress:
        m_keyPressEditor->setBlock(static_cast<KeyPressBlock*>(m_block.get()));
        break;
    case BlockType::Wait:
        m_waitEditor->setBlock(static_cast<WaitBlock*>(m_block.get()));
        break;
    }
}

void BlockEditorDialog::onTypeSelected(BlockType newType) {
    if (newType == m_block->type()) {
        m_typeChangeNote->setVisible(m_initialType != newType);
        m_stack->setCurrentIndex(stackIndexFor(newType));
        syncTypeButtonSelection(newType);
        fitToCurrentPage();
        return;
    }

    m_block = BlockFactory::create(newType);
    m_typeChangeNote->setVisible(true);
    syncTypeButtonSelection(newType);
    reloadEditorForType(newType);
    m_stack->setCurrentIndex(stackIndexFor(newType));
    fitToCurrentPage();
}

void BlockEditorDialog::fitToCurrentPage() {
    QWidget* page = m_stack->currentWidget();
    if (!page) {
        return;
    }

    page->setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
    page->adjustSize();

    const QSize pageHint = page->sizeHint().expandedTo(page->minimumSizeHint());
    m_stack->setMinimumSize(pageHint);
    m_stack->setMaximumSize(pageHint);

    adjustSize();
    const QSize dialogHint = sizeHint();
    resize(dialogHint);
    setMinimumSize(dialogHint);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
}

void BlockEditorDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    fitToCurrentPage();
    if (m_imageFindEditor) {
        m_imageFindEditor->refreshTemplateCaptureHotkeyHook();
    }
    if (m_clickEditor) {
        m_clickEditor->refreshClickCursorHotkeyHook();
    }
}

std::unique_ptr<Block> BlockEditorDialog::takeBlock() {
    return std::move(m_block);
}

void BlockEditorDialog::dismissCaptureOverlays() {
    ScreenRegionOverlay::dismissAll();
    MatchTestOverlay::dismissAll();
    RoiPreviewOverlay::dismissAll();
}

void BlockEditorDialog::reject() {
    dismissCaptureOverlays();
    QDialog::reject();
}

void BlockEditorDialog::closeEvent(QCloseEvent* event) {
    dismissCaptureOverlays();
    QDialog::closeEvent(event);
}
